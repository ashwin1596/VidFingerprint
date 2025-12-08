#include "matcher/matcher_service.h"
#include "core/fingerprint_generator.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace vfs;

void testBasicMatching() {
    std::cout << "Test: Basic Matching... ";
    
    std::string test_db = "test_matcher.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    auto metrics = std::make_shared<monitoring::MetricsCollector>();
    
    matcher::MatcherService::Config config;
    config.num_threads = 4;
    config.enable_caching = false;
    
    matcher::MatcherService service(db, metrics, config);
    
    core::FingerprintGenerator generator;
    auto fp = generator.generateFromFile("test.wav");
    
    matcher::MatcherService::MatchRequest request;
    request.request_id = "test_001";
    request.fingerprint = fp;
    request.min_similarity = 0.5;
    request.max_results = 10;
    
    auto response = service.match(request);
    
    assert(response.success);
    assert(response.request_id == "test_001");
    assert(response.processing_time_us > 0);
    
    std::filesystem::remove(test_db);
    
    std::cout << "PASSED" << std::endl;
}

void testAsyncMatching() {
    std::cout << "Test: Async Matching... ";
    
    std::string test_db = "test_async.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    auto metrics = std::make_shared<monitoring::MetricsCollector>();
    matcher::MatcherService service(db, metrics);
    
    core::FingerprintGenerator generator;
    auto fp = generator.generateFromFile("test.wav");
    
    matcher::MatcherService::MatchRequest request;
    request.request_id = "async_001";
    request.fingerprint = fp;
    
    auto future = service.matchAsync(request);
    auto response = future.get();
    
    assert(response.success);
    assert(response.request_id == "async_001");
    
    std::filesystem::remove(test_db);
    
    std::cout << "PASSED" << std::endl;
}

void testBatchMatching() {
    std::cout << "Test: Batch Matching... ";
    
    std::string test_db = "test_batch.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    auto metrics = std::make_shared<monitoring::MetricsCollector>();
    
    matcher::MatcherService::Config config;
    config.num_threads = 8;
    
    matcher::MatcherService service(db, metrics, config);
    
    core::FingerprintGenerator generator;
    auto fp = generator.generateFromFile("test.wav");
    
    // Create batch of requests
    std::vector<matcher::MatcherService::MatchRequest> requests;
    for (int i = 0; i < 10; ++i) {
        matcher::MatcherService::MatchRequest req;
        req.request_id = "batch_" + std::to_string(i);
        req.fingerprint = fp;
        requests.push_back(req);
    }
    
    auto responses = service.matchBatch(requests);
    
    assert(responses.size() == 10);
    for (const auto& resp : responses) {
        assert(resp.success);
    }
    
    std::filesystem::remove(test_db);
    
    std::cout << "PASSED" << std::endl;
}

void testCaching() {
    std::cout << "Test: Caching... ";
    
    std::string test_db = "test_cache.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    auto metrics = std::make_shared<monitoring::MetricsCollector>();
    
    matcher::MatcherService::Config config;
    config.enable_caching = true;
    config.cache_size = 100;
    
    matcher::MatcherService service(db, metrics, config);
    
    core::FingerprintGenerator generator;
    auto fp = generator.generateFromFile("test.wav");
    
    matcher::MatcherService::MatchRequest request;
    request.request_id = "cache_001";
    request.fingerprint = fp;
    
    // First request - cache miss
    service.match(request);
    
    // Second request - should be cache hit
    service.match(request);
    
    auto stats = service.getStats();
    assert(stats.cache_hits > 0);
    
    std::filesystem::remove(test_db);
    
    std::cout << "PASSED" << std::endl;
}

void testServiceStats() {
    std::cout << "Test: Service Statistics... ";
    
    std::string test_db = "test_stats_svc.db";
    std::filesystem::remove(test_db);
    
    auto db = std::make_shared<database::DatabaseManager>(test_db);
    db->initialize();
    
    auto metrics = std::make_shared<monitoring::MetricsCollector>();
    matcher::MatcherService service(db, metrics);
    
    core::FingerprintGenerator generator;
    auto fp = generator.generateFromFile("test.wav");
    
    // Make several requests
    for (int i = 0; i < 5; ++i) {
        matcher::MatcherService::MatchRequest req;
        req.request_id = "stats_" + std::to_string(i);
        req.fingerprint = fp;
        service.match(req);
    }
    
    auto stats = service.getStats();
    assert(stats.total_requests == 5);
    assert(stats.avg_latency_us > 0);
    
    std::filesystem::remove(test_db);
    
    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "=== Matcher Service Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        testBasicMatching();
        testAsyncMatching();
        testBatchMatching();
        testCaching();
        testServiceStats();
        
        std::cout << std::endl;
        std::cout << "All tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
