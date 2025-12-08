#include "matcher/matcher_service.h"
#include <chrono>
#include <algorithm>
#include <numeric>

namespace vfs {
namespace matcher {

    MatcherService::MatcherService(
        std::shared_ptr<database::DatabaseManager> db_manager,
        std::shared_ptr<monitoring::MetricsCollector> metrics,
        const Config& config)
        : db_manager_(db_manager)
        , metrics_(metrics)
        , thread_pool_(std::make_unique<utils::ThreadPool>(config.num_threads))
        , config_(config) {
    }

MatcherService::~MatcherService() = default;

MatcherService::MatchResponse 
MatcherService::match(const MatchRequest& request) {
    return processMatch(request);
}

std::future<MatcherService::MatchResponse> 
MatcherService::matchAsync(const MatchRequest& request) {
    return thread_pool_->submit([this, request]() {
        return processMatch(request);
    });
}

std::vector<MatcherService::MatchResponse> 
MatcherService::matchBatch(const std::vector<MatchRequest>& requests) {
    std::vector<std::future<MatchResponse>> futures;
    futures.reserve(requests.size());

    // Submit all requests to thread pool
    for (const auto& request : requests) {
        futures.push_back(matchAsync(request));
    }

    // Collect results
    std::vector<MatchResponse> responses;
    responses.reserve(requests.size());

    for (auto& future : futures) {
        responses.push_back(future.get());
    }

    return responses;
}

MatcherService::MatchResponse 
MatcherService::processMatch(const MatchRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    
    MatchResponse response;
    response.request_id = request.request_id;
    response.success = false;

    total_requests_.fetch_add(1, std::memory_order_relaxed);

    try {
        // Generate cache key
        std::string cache_key = generateCacheKey(request.fingerprint);

        // Check cache
        if (config_.enable_caching) {
            auto cached_results = checkCache(cache_key);
            if (cached_results) {
                cache_hits_.fetch_add(1, std::memory_order_relaxed);
                response.matches = *cached_results;
                response.success = true;
                
                auto end_time = std::chrono::steady_clock::now();
                response.processing_time_us = std::chrono::duration_cast<
                    std::chrono::microseconds>(end_time - start_time).count();
                
                metrics_->recordLatency("match_cached", response.processing_time_us);
                return response;
            }
            cache_misses_.fetch_add(1, std::memory_order_relaxed);
        }

        // Query database
        monitoring::MetricsCollector::Timer timer(metrics_.get(), "match_db_query");
        
        double min_sim = request.min_similarity > 0 
                        ? request.min_similarity 
                        : config_.default_min_similarity;
        
        size_t max_res = request.max_results > 0 
                        ? request.max_results 
                        : config_.default_max_results;

        response.matches = db_manager_->findMatches(
            request.fingerprint,
            min_sim,
            max_res
        );

        // Update cache
        if (config_.enable_caching && !response.matches.empty()) {
            updateCache(cache_key, response.matches);
        }

        response.success = true;
        successful_matches_.fetch_add(1, std::memory_order_relaxed);

    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = e.what();
        metrics_->incrementCounter("match_errors");
    }

    auto end_time = std::chrono::steady_clock::now();
    response.processing_time_us = std::chrono::duration_cast<
        std::chrono::microseconds>(end_time - start_time).count();

    // Record latency
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        latencies_.push_back(response.processing_time_us);
    }

    metrics_->recordLatency("match_total", response.processing_time_us);
    return response;
}

std::optional<std::vector<database::DatabaseManager::MatchResult>>
MatcherService::checkCache(const std::string& cache_key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = cache_.find(cache_key);
    if (it != cache_.end()) {
        // Update LRU
        cache_lru_.remove(cache_key);
        cache_lru_.push_front(cache_key);
        
        return it->second.results;
    }
    
    return std::nullopt;
}

void MatcherService::updateCache(
    const std::string& cache_key,
    const std::vector<database::DatabaseManager::MatchResult>& results) {
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Check if cache is full
    if (cache_.size() >= config_.cache_size) {
        // Remove least recently used
        std::string lru_key = cache_lru_.back();
        cache_lru_.pop_back();
        cache_.erase(lru_key);
    }
    
    // Add to cache
    CacheEntry entry;
    entry.results = results;
    entry.timestamp = std::chrono::steady_clock::now();
    
    cache_[cache_key] = entry;
    cache_lru_.push_front(cache_key);
}

std::string MatcherService::generateCacheKey(
    const core::FingerprintGenerator::Fingerprint& fingerprint) const {
    
    // Use raw hash as cache key (or first N characters for large fingerprints)
    if (fingerprint.raw_hash.length() <= 64) {
        return fingerprint.raw_hash;
    }
    return fingerprint.raw_hash.substr(0, 64);
}

MatcherService::ServiceStats MatcherService::getStats() const {
    ServiceStats stats;
    
    stats.total_requests = total_requests_.load(std::memory_order_relaxed);
    stats.successful_matches = successful_matches_.load(std::memory_order_relaxed);
    stats.cache_hits = cache_hits_.load(std::memory_order_relaxed);
    stats.cache_misses = cache_misses_.load(std::memory_order_relaxed);

    // Calculate latency statistics
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (!latencies_.empty()) {
        auto sorted_latencies = latencies_;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());
        
        stats.avg_latency_us = std::accumulate(
            sorted_latencies.begin(), 
            sorted_latencies.end(), 
            0.0
        ) / sorted_latencies.size();
        
        size_t p95_idx = static_cast<size_t>(sorted_latencies.size() * 0.95);
        size_t p99_idx = static_cast<size_t>(sorted_latencies.size() * 0.99);
        
        stats.p95_latency_us = sorted_latencies[p95_idx];
        stats.p99_latency_us = sorted_latencies[p99_idx];
    } else {
        stats.avg_latency_us = 0.0;
        stats.p95_latency_us = 0.0;
        stats.p99_latency_us = 0.0;
    }

    return stats;
}

void MatcherService::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
    cache_lru_.clear();
}

} // namespace matcher
} // namespace vfs
