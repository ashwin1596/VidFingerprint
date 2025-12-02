#include "database/database_manager.h"
#include <iostream>
#include <sstream>
#include <optional>

namespace vfs {
namespace database {

DatabaseManager::DatabaseManager(const std::string& db_path)
    : db_path_(db_path)
    , db_(nullptr)
    , insert_content_stmt_(nullptr)
    , insert_fingerprint_stmt_(nullptr)
    , query_fingerprints_stmt_(nullptr) {
}

DatabaseManager::~DatabaseManager() {
    cleanupStatements();
    if (db_) {
        sqlite3_close(db_);
    }
}

bool DatabaseManager::initialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    // Open database
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // Enable WAL mode for better concurrency
    executeSql("PRAGMA journal_mode=WAL");
    executeSql("PRAGMA synchronous=NORMAL");
    executeSql("PRAGMA cache_size=-64000"); // 64MB cache

    // Create schema
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS content (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            content_id TEXT UNIQUE NOT NULL,
            title TEXT NOT NULL,
            source TEXT,
            duration_ms INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            INDEX idx_content_id (content_id)
        );

        CREATE TABLE IF NOT EXISTS fingerprints (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            content_id TEXT NOT NULL,
            hash_value INTEGER NOT NULL,
            position INTEGER NOT NULL,
            FOREIGN KEY (content_id) REFERENCES content(content_id),
            INDEX idx_hash (hash_value),
            INDEX idx_content (content_id)
        );

        CREATE TABLE IF NOT EXISTS fingerprint_metadata (
            content_id TEXT PRIMARY KEY,
            raw_hash TEXT NOT NULL,
            num_hashes INTEGER NOT NULL,
            FOREIGN KEY (content_id) REFERENCES content(content_id)
        );
    )";

    if (!executeSql(schema)) {
        return false;
    }

    // Prepare statements
    return prepareStatements();
}

bool DatabaseManager::executeSql(const std::string& sql) {
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);
    
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << error_msg << std::endl;
        sqlite3_free(error_msg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::prepareStatements() {
    const char* insert_content_sql = R"(
        INSERT OR IGNORE INTO content (content_id, title, source, duration_ms, created_at)
        VALUES (?, ?, ?, ?, ?)
    )";

    const char* insert_fingerprint_sql = R"(
        INSERT INTO fingerprints (content_id, hash_value, position)
        VALUES (?, ?, ?)
    )";

    const char* query_fingerprints_sql = R"(
        SELECT DISTINCT c.id, c.content_id, c.title, c.source, c.duration_ms, c.created_at,
               COUNT(*) as match_count
        FROM fingerprints f
        JOIN content c ON f.content_id = c.content_id
        WHERE f.hash_value = ?
        GROUP BY c.content_id
        ORDER BY match_count DESC
        LIMIT ?
    )";

    int rc = sqlite3_prepare_v2(db_, insert_content_sql, -1, &insert_content_stmt_, nullptr);
    if (rc != SQLITE_OK) return false;

    rc = sqlite3_prepare_v2(db_, insert_fingerprint_sql, -1, &insert_fingerprint_stmt_, nullptr);
    if (rc != SQLITE_OK) return false;

    rc = sqlite3_prepare_v2(db_, query_fingerprints_sql, -1, &query_fingerprints_stmt_, nullptr);
    if (rc != SQLITE_OK) return false;

    return true;
}

void DatabaseManager::cleanupStatements() {
    if (insert_content_stmt_) sqlite3_finalize(insert_content_stmt_);
    if (insert_fingerprint_stmt_) sqlite3_finalize(insert_fingerprint_stmt_);
    if (query_fingerprints_stmt_) sqlite3_finalize(query_fingerprints_stmt_);
}

bool DatabaseManager::storeFingerprint(
    const std::string& content_id,
    const core::FingerprintGenerator::Fingerprint& fingerprint,
    const ContentMetadata& metadata) {
    
    std::lock_guard<std::mutex> lock(db_mutex_);

    // Begin transaction
    executeSql("BEGIN TRANSACTION");

    // Insert content metadata
    sqlite3_reset(insert_content_stmt_);
    sqlite3_bind_text(insert_content_stmt_, 1, content_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert_content_stmt_, 2, metadata.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert_content_stmt_, 3, metadata.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(insert_content_stmt_, 4, fingerprint.duration_ms);
    sqlite3_bind_int64(insert_content_stmt_, 5, metadata.created_at);

    int rc = sqlite3_step(insert_content_stmt_);
    if (rc != SQLITE_DONE) {
        executeSql("ROLLBACK");
        return false;
    }

    // Insert fingerprint hashes
    for (size_t i = 0; i < fingerprint.hash_values.size(); ++i) {
        sqlite3_reset(insert_fingerprint_stmt_);
        sqlite3_bind_text(insert_fingerprint_stmt_, 1, content_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(insert_fingerprint_stmt_, 2, fingerprint.hash_values[i]);
        sqlite3_bind_int(insert_fingerprint_stmt_, 3, static_cast<int>(i));

        rc = sqlite3_step(insert_fingerprint_stmt_);
        if (rc != SQLITE_DONE) {
            executeSql("ROLLBACK");
            return false;
        }
    }

    // Insert raw hash metadata
    std::stringstream ss;
    ss << "INSERT OR REPLACE INTO fingerprint_metadata (content_id, raw_hash, num_hashes) "
       << "VALUES ('" << content_id << "', '" << fingerprint.raw_hash << "', "
       << fingerprint.hash_values.size() << ")";
    
    if (!executeSql(ss.str())) {
        executeSql("ROLLBACK");
        return false;
    }

    executeSql("COMMIT");
    return true;
}

std::vector<DatabaseManager::MatchResult> DatabaseManager::findMatches(
    const core::FingerprintGenerator::Fingerprint& fingerprint,
    double min_similarity,
    size_t max_results) {
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<MatchResult> results;

    // Collect candidate content IDs
    std::map<std::string, uint32_t> candidate_matches;

    for (uint32_t hash : fingerprint.hash_values) {
        sqlite3_reset(query_fingerprints_stmt_);
        sqlite3_bind_int(query_fingerprints_stmt_, 1, hash);
        sqlite3_bind_int(query_fingerprints_stmt_, 2, static_cast<int>(max_results * 2));

        while (sqlite3_step(query_fingerprints_stmt_) == SQLITE_ROW) {
            std::string content_id = reinterpret_cast<const char*>(
                sqlite3_column_text(query_fingerprints_stmt_, 1));
            uint32_t match_count = sqlite3_column_int(query_fingerprints_stmt_, 6);
            
            candidate_matches[content_id] += match_count;
        }
    }

    // Calculate similarity scores for candidates
    for (const auto& [content_id, match_count] : candidate_matches) {
        // Retrieve stored fingerprint
        std::stringstream ss;
        ss << "SELECT raw_hash FROM fingerprint_metadata WHERE content_id = '" 
           << content_id << "'";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr);
        
        if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            std::string stored_hash = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, 0));

            // Simple similarity based on matching hash count
            double similarity = static_cast<double>(match_count) / 
                              std::max(fingerprint.hash_values.size(), 
                                     stored_hash.length() / 8);

            if (similarity >= min_similarity) {
                auto metadata = getContentById(content_id);
                if (metadata) {
                    MatchResult result;
                    result.metadata = *metadata;
                    result.similarity_score = similarity;
                    result.matched_segments = match_count;
                    results.push_back(result);
                }
            }
        }
        
        sqlite3_finalize(stmt);
    }

    // Sort by similarity score
    std::sort(results.begin(), results.end(),
        [](const MatchResult& a, const MatchResult& b) {
            return a.similarity_score > b.similarity_score;
        });

    if (results.size() > max_results) {
        results.resize(max_results);
    }

    return results;
}

std::optional<DatabaseManager::ContentMetadata> 
DatabaseManager::getContentById(const std::string& content_id) {
    std::stringstream ss;
    ss << "SELECT id, content_id, title, source, duration_ms, created_at "
       << "FROM content WHERE content_id = '" << content_id << "'";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ContentMetadata metadata;
        metadata.id = sqlite3_column_int64(stmt, 0);
        metadata.content_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        metadata.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        metadata.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        metadata.duration_ms = sqlite3_column_int64(stmt, 4);
        metadata.created_at = sqlite3_column_int64(stmt, 5);
        
        sqlite3_finalize(stmt);
        return metadata;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

DatabaseManager::Stats DatabaseManager::getStats() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    Stats stats = {0, 0, 0};

    // Count fingerprints
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM fingerprints", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats.total_fingerprints = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    // Count content
    sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM content", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats.total_content = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    // Get DB size
    sqlite3_prepare_v2(db_, "SELECT page_count * page_size FROM pragma_page_count(), pragma_page_size()", 
                      -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats.db_size_bytes = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return stats;
}

} // namespace database
} // namespace vfs
