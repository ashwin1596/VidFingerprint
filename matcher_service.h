#ifndef MATCHER_SERVICE_H
#define MATCHER_SERVICE_H

#include "core/fingerprint_generator.h"
#include "database/database_manager.h"
#include "utils/thread_pool.h"
#include "monitoring/metrics.h"
#include <memory>
#include <future>
#include <atomic>

namespace vfs {
namespace matcher {

/**
 * @brief High-performance concurrent fingerprint matching service
 * 
 * Handles multiple concurrent match requests using thread pool and caching.
 * Designed for low-latency (<100ms) matching at scale.
 */
class MatcherService {
public:
    struct MatchRequest {
        std::string request_id;
        core::FingerprintGenerator::Fingerprint fingerprint;
        double min_similarity;
        size_t max_results;
    };

    struct MatchResponse {
        std::string request_id;
        std::vector<database::DatabaseManager::MatchResult> matches;
        uint64_t processing_time_us;
        bool success;
        std::string error_message;
    };

    struct Config {
        size_t num_threads = 8;
        size_t cache_size = 10000;
        bool enable_caching = true;
        double default_min_similarity = 0.7;
        size_t default_max_results = 10;
    };

    MatcherService(
        std::shared_ptr<database::DatabaseManager> db_manager,
        std::shared_ptr<monitoring::MetricsCollector> metrics,
        const Config& config = Config{});

    ~MatcherService();

    /**
     * @brief Process match request synchronously
     */
    MatchResponse match(const MatchRequest& request);

    /**
     * @brief Process match request asynchronously
     */
    std::future<MatchResponse> matchAsync(const MatchRequest& request);

    /**
     * @brief Process batch of requests
     */
    std::vector<MatchResponse> matchBatch(const std::vector<MatchRequest>& requests);

    /**
     * @brief Get service statistics
     */
    struct ServiceStats {
        uint64_t total_requests;
        uint64_t successful_matches;
        uint64_t cache_hits;
        uint64_t cache_misses;
        double avg_latency_us;
        double p95_latency_us;
        double p99_latency_us;
    };
    ServiceStats getStats() const;

    /**
     * @brief Clear cache
     */
    void clearCache();

private:
    std::shared_ptr<database::DatabaseManager> db_manager_;
    std::shared_ptr<monitoring::MetricsCollector> metrics_;
    std::unique_ptr<utils::ThreadPool> thread_pool_;
    Config config_;

    // LRU Cache for hot fingerprints
    struct CacheEntry {
        std::vector<database::DatabaseManager::MatchResult> results;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    mutable std::mutex cache_mutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    std::list<std::string> cache_lru_;

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> successful_matches_{0};
    std::atomic<uint64_t> cache_hits_{0};
    std::atomic<uint64_t> cache_misses_{0};
    std::vector<uint64_t> latencies_;

    /**
     * @brief Check cache for fingerprint
     */
    std::optional<std::vector<database::DatabaseManager::MatchResult>>
    checkCache(const std::string& cache_key);

    /**
     * @brief Update cache with new results
     */
    void updateCache(
        const std::string& cache_key,
        const std::vector<database::DatabaseManager::MatchResult>& results);

    /**
     * @brief Generate cache key from fingerprint
     */
    std::string generateCacheKey(
        const core::FingerprintGenerator::Fingerprint& fingerprint) const;

    /**
     * @brief Process single match request (internal)
     */
    MatchResponse processMatch(const MatchRequest& request);
};

} // namespace matcher
} // namespace vfs

#endif // MATCHER_SERVICE_H
