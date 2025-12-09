#include "matcher/matcher_service.h"
#include "utils/thread_pool.h"
#include "utils/profiler.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <atomic>
#include <filesystem>

using namespace vfs;

void testThreadPoolPerformance() {
    std::cout << "\n=== Thread Pool Performance ===" << std::endl;
    
    std::vector<size_t> thread_counts = {1, 2, 4, 8};
    const size_t num_tasks = 10000;
    
    std::cout << std::endl;
    std::cout << std::setw(10) << "Threads"
              << std::setw(20) << "Tasks/second"
              << std::setw(15) << "Overhead" << std::endl;
    std::cout << std::string(45, '-') << std::endl;
    
    for (size_t threads : thread_counts) {
        utils::ThreadPool pool(threads);
        
        std::atomic<size_t> completed{0};
        
        auto start = std::chrono::steady_clock::now();
        
        std::vector<std::future<void>> futures;
        for (size_t i = 0; i < num_tasks; ++i) {
            futures.push_back(pool.submit([&completed]() {
                // Simulate lightweight work
                volatile int x = 0;
                for (int j = 0; j < 100; ++j) {
                    x += j;
                }
                completed.fetch_add(1, std::memory_order_relaxed);
            }));
        }
        
        // Wait for all tasks
        for (auto& f : futures) {
            f.get();
        }
        
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        
        double tasks_per_sec = num_tasks * 1000.0 / duration_ms;
        double overhead_us = (duration_ms * 1000.0 / num_tasks);
        
        std::cout << std::setw(10) << threads
                  << std::setw(20) << std::fixed << std::setprecision(0)
                  << tasks_per_sec
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << overhead_us << " μs" << std::endl;
    }
}

void testConcurrentMatching() {
    std::cout << "\n=== Concurrent Matching Stress Test ===" << std::endl;
    
    std::string test_db = "concurrent_bench.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    core::FingerprintGenerator generator;
    
    // Populate database
    std::cout << "Preparing database..." << std::endl;
    for (int i = 0; i < 100; ++i) {
        auto fp = generator.generateFromFile("test.wav");
        database::DatabaseManager::ContentMetadata metadata;
        metadata.content_id = "content_" + std::to_string(i);
        metadata.title = "Test " + std::to_string(i);
        metadata.source = "benchmark";
        metadata.created_at = 1234567890;
        db->storeFingerprint(metadata.content_id, fp, metadata);
    }
    
    auto metrics = std::make_shared<monitoring::MetricsCollector>();
    
    matcher::MatcherService::Config config;
    config.num_threads = 16;
    config.cache_size = 10000;
    config.enable_caching = true;
    
    matcher::MatcherService service(db, metrics, config);
    
    auto query_fp = generator.generateFromFile("query.wav");
    
    const size_t num_concurrent_requests = 10000;
    
    std::cout << "Submitting " << num_concurrent_requests << " concurrent requests..." << std::endl;
    
    std::vector<std::future<matcher::MatcherService::MatchResponse>> futures;
    
    auto start = std::chrono::steady_clock::now();
    
    for (size_t i = 0; i < num_concurrent_requests; ++i) {
        matcher::MatcherService::MatchRequest req;
        req.request_id = "concurrent_" + std::to_string(i);
        req.fingerprint = query_fp;
        req.min_similarity = 0.6;
        req.max_results = 10;
        
        futures.push_back(service.matchAsync(req));
    }
    
    // Collect all results
    size_t successful = 0;
    for (auto& future : futures) {
        auto response = future.get();
        if (response.success) {
            ++successful;
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    
    auto stats = service.getStats();
    
    std::cout << "\nResults:" << std::endl;
    std::cout << "  Total Requests: " << num_concurrent_requests << std::endl;
    std::cout << "  Successful: " << successful << std::endl;
    std::cout << "  Total Time: " << duration_ms << " ms" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(1)
              << (num_concurrent_requests * 1000.0 / duration_ms) << " req/sec" << std::endl;
    std::cout << "  Avg Latency: " << stats.avg_latency_us << " μs" << std::endl;
    std::cout << "  P95 Latency: " << stats.p95_latency_us << " μs" << std::endl;
    std::cout << "  P99 Latency: " << stats.p99_latency_us << " μs" << std::endl;
    std::cout << "  Cache Hit Rate: " << std::fixed << std::setprecision(1)
              << (stats.cache_hits * 100.0 / stats.total_requests) << "%" << std::endl;
    
    std::filesystem::remove(test_db);
}

void testCacheEfficiency() {
    std::cout << "\n=== Cache Efficiency Test ===" << std::endl;
    
    std::string test_db = "cache_bench.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    core::FingerprintGenerator generator;
    
    // Populate database
    for (int i = 0; i < 50; ++i) {
        auto fp = generator.generateFromFile("test.wav");
        database::DatabaseManager::ContentMetadata metadata;
        metadata.content_id = "content_" + std::to_string(i);
        metadata.title = "Test " + std::to_string(i);
        metadata.source = "benchmark";
        metadata.created_at = 1234567890;
        db->storeFingerprint(metadata.content_id, fp, metadata);
    }
    
    auto metrics = std::make_shared<monitoring::MetricsCollector>();
    
    matcher::MatcherService::Config config;
    config.num_threads = 8;
    config.cache_size = 100;
    config.enable_caching = true;
    
    matcher::MatcherService service(db, metrics, config);
    
    // Generate a few unique fingerprints
    std::vector<core::FingerprintGenerator::Fingerprint> fingerprints;
    for (int i = 0; i < 10; ++i) {
        fingerprints.push_back(generator.generateFromFile("unique_" + std::to_string(i) + ".wav"));
    }
    
    // Make requests with repeating patterns (Zipfian-like distribution)
    std::cout << "Simulating real-world request patterns..." << std::endl;
    
    std::vector<matcher::MatcherService::MatchRequest> requests;
    for (int i = 0; i < 1000; ++i) {
        // 80% of requests go to 20% of fingerprints
        size_t fp_idx = (i % 10 < 8) ? (i % 2) : (i % 10);
        
        matcher::MatcherService::MatchRequest req;
        req.request_id = "cache_test_" + std::to_string(i);
        req.fingerprint = fingerprints[fp_idx];
        requests.push_back(req);
    }
    
    auto start = std::chrono::steady_clock::now();
    service.matchBatch(requests);
    auto end = std::chrono::steady_clock::now();
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    
    auto stats = service.getStats();
    
    std::cout << "\nCache Performance:" << std::endl;
    std::cout << "  Total Requests: " << stats.total_requests << std::endl;
    std::cout << "  Cache Hits: " << stats.cache_hits << std::endl;
    std::cout << "  Cache Misses: " << stats.cache_misses << std::endl;
    std::cout << "  Hit Rate: " << std::fixed << std::setprecision(1)
              << (stats.cache_hits * 100.0 / stats.total_requests) << "%" << std::endl;
    std::cout << "  Total Time: " << duration_ms << " ms" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(1)
              << (1000.0 * 1000 / duration_ms) << " req/sec" << std::endl;
    
    std::filesystem::remove(test_db);
}

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║                                                            ║
║        VIDEO FINGERPRINTING - CONCURRENCY BENCHMARK       ║
║                                                            ║
╚════════════════════════════════════════════════════════════╝
)" << std::endl;

    // Print system information
    utils::Profiler::printSystemInfo();

    try {
        testThreadPoolPerformance();
        testConcurrentMatching();
        testCacheEfficiency();
        
        std::cout << "\n=== Benchmark Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
