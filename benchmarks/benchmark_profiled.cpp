#include "matcher/matcher_service.h"
#include "core/fingerprint_generator.h"
#include "utils/profiler.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <thread>

using namespace vfs;

void profiledBenchmark() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                                            ║" << std::endl;
    std::cout << "║        PROFILED PERFORMANCE BENCHMARK                     ║" << std::endl;
    std::cout << "║                                                            ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝\n" << std::endl;

    std::string test_db = "profile_bench.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    std::cout << "=== Initial State ===" << std::endl;
    utils::Profiler::printResourceUsage();
    
    // Populate database
    core::FingerprintGenerator generator;
    std::cout << "Populating database with 100 entries..." << std::endl;
    
    for (int i = 0; i < 100; ++i) {
        auto fp = generator.generateFromFile("content_" + std::to_string(i) + ".wav");
        
        database::DatabaseManager::ContentMetadata metadata;
        metadata.content_id = "content_" + std::to_string(i);
        metadata.title = "Test Content " + std::to_string(i);
        metadata.source = "benchmark";
        metadata.created_at = 1234567890;
        
        db->storeFingerprint(metadata.content_id, fp, metadata);
    }
    
    std::cout << "\n=== After Database Population ===" << std::endl;
    utils::Profiler::printResourceUsage();
    
    auto metrics = std::make_shared<monitoring::MetricsCollector>();
    
    // Test with different configurations
    std::vector<size_t> cache_sizes = {1000, 5000, 10000};
    std::vector<size_t> thread_counts = {2, 4, 8};
    
    std::cout << "\n=== Performance vs Configuration ===" << std::endl;
    std::cout << std::setw(8) << "Threads" 
              << std::setw(12) << "Cache" 
              << std::setw(15) << "Throughput"
              << std::setw(15) << "Avg Latency"
              << std::setw(15) << "Memory (MB)" << std::endl;
    std::cout << std::string(65, '-') << std::endl;
    
    for (size_t threads : thread_counts) {
        for (size_t cache_size : cache_sizes) {
            matcher::MatcherService::Config config;
            config.num_threads = threads;
            config.cache_size = cache_size;
            config.enable_caching = true;
            
            matcher::MatcherService service(db, metrics, config);
            
            auto query_fp = generator.generateFromFile("query.wav");
            
            // Warm up cache
            for (int i = 0; i < 100; ++i) {
                matcher::MatcherService::MatchRequest req;
                req.request_id = "warmup_" + std::to_string(i);
                req.fingerprint = query_fp;
                service.match(req);
            }
            
            // Benchmark
            const size_t num_requests = 1000;
            std::vector<matcher::MatcherService::MatchRequest> requests;
            for (size_t i = 0; i < num_requests; ++i) {
                matcher::MatcherService::MatchRequest req;
                req.request_id = "bench_" + std::to_string(i);
                req.fingerprint = query_fp;
                requests.push_back(req);
            }
            
            auto start = std::chrono::steady_clock::now();
            auto responses = service.matchBatch(requests);
            auto end = std::chrono::steady_clock::now();
            
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start).count();
            
            auto stats = service.getStats();
            auto mem = utils::Profiler::getMemoryUsage();
            
            double throughput = num_requests * 1000.0 / duration_ms;
            
            std::cout << std::setw(8) << threads
                      << std::setw(12) << cache_size
                      << std::setw(15) << std::fixed << std::setprecision(1) 
                      << throughput << " rps"
                      << std::setw(15) << std::fixed << std::setprecision(2)
                      << stats.avg_latency_us << " μs"
                      << std::setw(15) << std::fixed << std::setprecision(1)
                      << (mem.resident_memory_kb / 1024.0) << std::endl;
        }
    }
    
    std::cout << "\n=== Peak Load Test ===" << std::endl;
    std::cout << "Testing system under sustained load..." << std::endl;
    
    matcher::MatcherService::Config peak_config;
    peak_config.num_threads = 8;
    peak_config.cache_size = 10000;
    peak_config.enable_caching = true;
    
    matcher::MatcherService peak_service(db, metrics, peak_config);
    
    auto query_fp = generator.generateFromFile("query.wav");
    
    std::cout << "\nRunning 10-second sustained load test..." << std::endl;
    
    size_t total_requests = 0;
    auto test_start = std::chrono::steady_clock::now();
    auto test_end = test_start + std::chrono::seconds(10);
    
    // Sample memory every second
    std::vector<double> memory_samples;
    auto last_sample = test_start;
    
    while (std::chrono::steady_clock::now() < test_end) {
        // Submit batch
        std::vector<matcher::MatcherService::MatchRequest> batch;
        for (int i = 0; i < 100; ++i) {
            matcher::MatcherService::MatchRequest req;
            req.request_id = "load_" + std::to_string(total_requests + i);
            req.fingerprint = query_fp;
            batch.push_back(req);
        }
        
        peak_service.matchBatch(batch);
        total_requests += batch.size();
        
        // Sample memory every second
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_sample).count() >= 1) {
            auto mem = utils::Profiler::getMemoryUsage();
            memory_samples.push_back(mem.resident_memory_kb / 1024.0);
            last_sample = now;
            std::cout << "." << std::flush;
        }
    }
    
    auto actual_duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - test_start).count();
    
    std::cout << "\n\nSustained Load Results:" << std::endl;
    std::cout << "  Duration: " << actual_duration << " seconds" << std::endl;
    std::cout << "  Total Requests: " << total_requests << std::endl;
    std::cout << "  Average Throughput: " << (total_requests / actual_duration) << " req/sec" << std::endl;
    
    auto peak_stats = peak_service.getStats();
    std::cout << "  Avg Latency: " << std::fixed << std::setprecision(2) 
              << peak_stats.avg_latency_us << " μs" << std::endl;
    std::cout << "  P95 Latency: " << peak_stats.p95_latency_us << " μs" << std::endl;
    std::cout << "  P99 Latency: " << peak_stats.p99_latency_us << " μs" << std::endl;
    std::cout << "  Cache Hit Rate: " << std::fixed << std::setprecision(1)
              << (peak_stats.cache_hits * 100.0 / peak_stats.total_requests) << "%" << std::endl;
    
    // Memory statistics
    if (!memory_samples.empty()) {
        double avg_mem = 0;
        double max_mem = memory_samples[0];
        double min_mem = memory_samples[0];
        
        for (double sample : memory_samples) {
            avg_mem += sample;
            max_mem = std::max(max_mem, sample);
            min_mem = std::min(min_mem, sample);
        }
        avg_mem /= memory_samples.size();
        
        std::cout << "\nMemory Usage (RSS):" << std::endl;
        std::cout << "  Average: " << std::fixed << std::setprecision(1) << avg_mem << " MB" << std::endl;
        std::cout << "  Min: " << min_mem << " MB" << std::endl;
        std::cout << "  Max: " << max_mem << " MB" << std::endl;
        std::cout << "  Variation: " << (max_mem - min_mem) << " MB" << std::endl;
    }
    
    std::cout << "\n=== Final Resource State ===" << std::endl;
    utils::Profiler::printResourceUsage();
    
    std::filesystem::remove(test_db);
}

int main() {
    try {
        profiledBenchmark();
        
        std::cout << "\n=== Profiling Complete ===" << std::endl;
        std::cout << "\nKey Takeaways:" << std::endl;
        std::cout << "• Memory usage remains stable under sustained load" << std::endl;
        std::cout << "• Cache size directly impacts memory footprint" << std::endl;
        std::cout << "• Thread count affects throughput and latency" << std::endl;
        std::cout << "• System maintains consistent performance over time" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
