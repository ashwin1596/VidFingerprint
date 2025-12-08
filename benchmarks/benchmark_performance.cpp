#include "matcher/matcher_service.h"
#include "core/fingerprint_generator.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <numeric>

using namespace vfs;

void benchmarkThroughput(size_t num_requests, size_t num_threads) {
    std::cout << "\n=== Throughput Benchmark ===" << std::endl;
    std::cout << "Requests: " << num_requests << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << std::endl;
    
    std::string test_db = "benchmark.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    // Populate database
    core::FingerprintGenerator generator;
    std::cout << "Populating database with test data..." << std::endl;
    for (int i = 0; i < 100; ++i) {
        auto fp = generator.generateFromFile("content_" + std::to_string(i) + ".wav");
        
        database::DatabaseManager::ContentMetadata metadata;
        metadata.content_id = "content_" + std::to_string(i);
        metadata.title = "Test Content " + std::to_string(i);
        metadata.source = "benchmark";
        metadata.created_at = 1234567890;
        
        db->storeFingerprint(metadata.content_id, fp, metadata);
    }
    
    auto metrics = std::make_shared<monitoring::MetricsCollector>();
    
    matcher::MatcherService::Config config;
    config.num_threads = num_threads;
    config.cache_size = 10000;
    config.enable_caching = true;
    
    matcher::MatcherService service(db, metrics, config);
    
    // Generate test fingerprint
    auto query_fp = generator.generateFromFile("query.wav");
    
    // Create batch of requests
    std::vector<matcher::MatcherService::MatchRequest> requests;
    for (size_t i = 0; i < num_requests; ++i) {
        matcher::MatcherService::MatchRequest req;
        req.request_id = "bench_" + std::to_string(i);
        req.fingerprint = query_fp;
        req.min_similarity = 0.6;
        req.max_results = 10;
        requests.push_back(req);
    }
    
    std::cout << "Running benchmark..." << std::endl;
    
    auto start = std::chrono::steady_clock::now();
    auto responses = service.matchBatch(requests);
    auto end = std::chrono::steady_clock::now();
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    
    auto stats = service.getStats();
    
    std::cout << "\nResults:" << std::endl;
    std::cout << "  Total Time: " << duration_ms << " ms" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(1)
              << (num_requests * 1000.0 / duration_ms) << " req/sec" << std::endl;
    std::cout << "  Avg Latency: " << std::fixed << std::setprecision(2)
              << stats.avg_latency_us << " μs" << std::endl;
    std::cout << "  P95 Latency: " << stats.p95_latency_us << " μs" << std::endl;
    std::cout << "  P99 Latency: " << stats.p99_latency_us << " μs" << std::endl;
    std::cout << "  Cache Hit Rate: " << std::fixed << std::setprecision(1)
              << (stats.cache_hits * 100.0 / stats.total_requests) << "%" << std::endl;
    
    std::filesystem::remove(test_db);
}

void benchmarkLatency() {
    std::cout << "\n=== Latency Benchmark ===" << std::endl;
    
    std::string test_db = "latency_bench.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    core::FingerprintGenerator generator;
    
    // Populate with test data
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
    config.num_threads = 1; // Single thread for pure latency measurement
    config.enable_caching = false;
    
    matcher::MatcherService service(db, metrics, config);
    
    auto query_fp = generator.generateFromFile("query.wav");
    
    std::vector<uint64_t> latencies;
    
    std::cout << "Measuring single-request latency..." << std::endl;
    
    for (int i = 0; i < 100; ++i) {
        matcher::MatcherService::MatchRequest req;
        req.request_id = "lat_" + std::to_string(i);
        req.fingerprint = query_fp;
        
        auto start = std::chrono::steady_clock::now();
        auto response = service.match(req);
        auto end = std::chrono::steady_clock::now();
        
        auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        latencies.push_back(latency_us);
    }
    
    std::sort(latencies.begin(), latencies.end());
    
    double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    uint64_t p50 = latencies[latencies.size() / 2];
    uint64_t p95 = latencies[static_cast<size_t>(latencies.size() * 0.95)];
    uint64_t p99 = latencies[static_cast<size_t>(latencies.size() * 0.99)];
    
    std::cout << "\nLatency Distribution:" << std::endl;
    std::cout << "  Min:  " << latencies.front() << " μs" << std::endl;
    std::cout << "  Avg:  " << std::fixed << std::setprecision(2) << avg << " μs" << std::endl;
    std::cout << "  P50:  " << p50 << " μs" << std::endl;
    std::cout << "  P95:  " << p95 << " μs" << std::endl;
    std::cout << "  P99:  " << p99 << " μs" << std::endl;
    std::cout << "  Max:  " << latencies.back() << " μs" << std::endl;
    
    std::filesystem::remove(test_db);
}

void benchmarkScalability() {
    std::cout << "\n=== Scalability Benchmark ===" << std::endl;
    std::cout << "Testing throughput with different thread counts..." << std::endl;
    
    std::vector<size_t> thread_counts = {1, 2, 4, 8, 16};
    const size_t num_requests = 1000;
    
    std::cout << std::endl;
    std::cout << std::setw(10) << "Threads" 
              << std::setw(15) << "Throughput"
              << std::setw(15) << "Speedup" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    double baseline_throughput = 0;
    
    for (size_t threads : thread_counts) {
        std::string test_db = "scale_bench.db";
        std::filesystem::remove(test_db);
        
        auto db = std::make_shared<database::DatabaseManager>(test_db);
        db->initialize();
        
        core::FingerprintGenerator generator;
        
        // Minimal database population
        for (int i = 0; i < 10; ++i) {
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
        config.num_threads = threads;
        config.cache_size = 10000;
        config.enable_caching = true;
        
        matcher::MatcherService service(db, metrics, config);
        
        auto query_fp = generator.generateFromFile("query.wav");
        
        std::vector<matcher::MatcherService::MatchRequest> requests;
        for (size_t i = 0; i < num_requests; ++i) {
            matcher::MatcherService::MatchRequest req;
            req.request_id = "scale_" + std::to_string(i);
            req.fingerprint = query_fp;
            requests.push_back(req);
        }
        
        auto start = std::chrono::steady_clock::now();
        service.matchBatch(requests);
        auto end = std::chrono::steady_clock::now();
        
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        
        double throughput = num_requests * 1000.0 / duration_ms;
        
        if (threads == 1) {
            baseline_throughput = throughput;
        }
        
        double speedup = throughput / baseline_throughput;
        
        std::cout << std::setw(10) << threads
                  << std::setw(15) << std::fixed << std::setprecision(1) 
                  << throughput << " rps"
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << speedup << "x" << std::endl;
        
        std::filesystem::remove(test_db);
    }
}

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════╗
║                                                            ║
║        VIDEO FINGERPRINTING - PERFORMANCE BENCHMARK       ║
║                                                            ║
╚════════════════════════════════════════════════════════════╝
)" << std::endl;

    try {
        benchmarkThroughput(10000, 8);
        benchmarkLatency();
        benchmarkScalability();
        
        std::cout << "\n=== Benchmark Complete ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
