#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include "core/fingerprint_generator.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <sqlite3.h>

namespace vfs {
namespace database {

/**
 * @brief Manages fingerprint database operations
 * 
 * Thread-safe database manager for storing and querying fingerprints.
 * Uses connection pooling and prepared statements for high performance.
 */
class DatabaseManager {
public:
    struct ContentMetadata {
        int64_t id;
        std::string content_id;
        std::string title;
        std::string source;
        uint64_t duration_ms;
        int64_t created_at;
    };

    struct MatchResult {
        ContentMetadata metadata;
        double similarity_score;
        uint32_t matched_segments;
    };

    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    // Prevent copying
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    /**
     * @brief Initialize database schema
     */
    bool initialize();

    /**
     * @brief Store fingerprint in database
     * @param content_id Unique identifier for content
     * @param fingerprint Generated fingerprint
     * @param metadata Content metadata
     * @return Success status
     */
    bool storeFingerprint(
        const std::string& content_id,
        const core::FingerprintGenerator::Fingerprint& fingerprint,
        const ContentMetadata& metadata);

    /**
     * @brief Find matching content for a fingerprint
     * @param fingerprint Query fingerprint
     * @param min_similarity Minimum similarity threshold (0.0 to 1.0)
     * @param max_results Maximum number of results to return
     * @return Vector of matching results sorted by similarity
     */
    std::vector<MatchResult> findMatches(
        const core::FingerprintGenerator::Fingerprint& fingerprint,
        double min_similarity = 0.7,
        size_t max_results = 10);

    /**
     * @brief Get content metadata by ID
     */
    std::optional<ContentMetadata> getContentById(const std::string& content_id);

    /**
     * @brief Get database statistics
     */
    struct Stats {
        int64_t total_fingerprints;
        int64_t total_content;
        int64_t db_size_bytes;
    };
    Stats getStats();

private:
    std::string db_path_;
    sqlite3* db_;
    std::mutex db_mutex_;

    // Prepared statements for performance
    sqlite3_stmt* insert_content_stmt_;
    sqlite3_stmt* insert_fingerprint_stmt_;
    sqlite3_stmt* query_fingerprints_stmt_;

    /**
     * @brief Execute SQL statement
     */
    bool executeSql(const std::string& sql);

    /**
     * @brief Prepare all statements
     */
    bool prepareStatements();

    /**
     * @brief Cleanup prepared statements
     */
    void cleanupStatements();
};

} // namespace database
} // namespace vfs

#endif // DATABASE_MANAGER_H
