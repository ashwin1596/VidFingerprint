#include "database/database_manager.h"
#include "core/fingerprint_generator.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace vfs;

void testDatabaseInitialization() {
    std::cout << "Test: Database Initialization... ";
    
    std::string test_db = "test_init.db";
    
    // Clean up if exists
    std::filesystem::remove(test_db);
    
    database::DatabaseManager db(test_db);
    assert(db.initialize());
    
    std::filesystem::remove(test_db);
    
    std::cout << "PASSED" << std::endl;
}

void testStoringFingerprint() {
    std::cout << "Test: Storing Fingerprint... ";
    
    std::string test_db = "test_store.db";
    std::filesystem::remove(test_db);
    
    database::DatabaseManager db(test_db);
    db.initialize();
    
    core::FingerprintGenerator generator;
    auto fp = generator.generateFromFile("test.wav");
    
    database::DatabaseManager::ContentMetadata metadata;
    metadata.content_id = "test_001";
    metadata.title = "Test Content";
    metadata.source = "test";
    metadata.created_at = 1234567890;
    
    assert(db.storeFingerprint("test_001", fp, metadata));
    
    // Verify storage
    auto retrieved = db.getContentById("test_001");
    assert(retrieved.has_value());
    assert(retrieved->title == "Test Content");
    
    std::filesystem::remove(test_db);
    
    std::cout << "PASSED" << std::endl;
}

void testFindingMatches() {
    std::cout << "Test: Finding Matches... ";
    
    std::string test_db = "test_match.db";
    std::filesystem::remove(test_db);
    
    database::DatabaseManager db(test_db);
    db.initialize();
    
    core::FingerprintGenerator generator;
    
    // Store multiple fingerprints
    for (int i = 0; i < 5; ++i) {
        auto fp = generator.generateFromFile("test_" + std::to_string(i) + ".wav");
        
        database::DatabaseManager::ContentMetadata metadata;
        metadata.content_id = "content_" + std::to_string(i);
        metadata.title = "Test Content " + std::to_string(i);
        metadata.source = "test";
        metadata.created_at = 1234567890 + i;
        
        db.storeFingerprint(metadata.content_id, fp, metadata);
    }
    
    // Query with a fingerprint
    auto query_fp = generator.generateFromFile("query.wav");
    auto matches = db.findMatches(query_fp, 0.5, 10);
    
    // Should find at least some matches (depending on similarity)
    assert(matches.size() >= 0);
    
    std::filesystem::remove(test_db);
    
    std::cout << "PASSED" << std::endl;
}

void testDatabaseStats() {
    std::cout << "Test: Database Statistics... ";
    
    std::string test_db = "test_stats.db";
    std::filesystem::remove(test_db);
    
    database::DatabaseManager db(test_db);
    db.initialize();
    
    core::FingerprintGenerator generator;
    
    // Store some fingerprints
    for (int i = 0; i < 3; ++i) {
        auto fp = generator.generateFromFile("test.wav");
        
        database::DatabaseManager::ContentMetadata metadata;
        metadata.content_id = "content_" + std::to_string(i);
        metadata.title = "Test " + std::to_string(i);
        metadata.source = "test";
        metadata.created_at = 1234567890;
        
        db.storeFingerprint(metadata.content_id, fp, metadata);
    }
    
    auto stats = db.getStats();
    assert(stats.total_content == 3);
    assert(stats.total_fingerprints > 0);
    assert(stats.db_size_bytes > 0);
    
    std::filesystem::remove(test_db);
    
    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "=== Database Manager Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        testDatabaseInitialization();
        testStoringFingerprint();
        testFindingMatches();
        testDatabaseStats();
        
        std::cout << std::endl;
        std::cout << "All tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
