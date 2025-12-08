#ifndef FINGERPRINT_GENERATOR_H
#define FINGERPRINT_GENERATOR_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <array>

namespace vfs {
namespace core {

/**
 * @brief Generates audio fingerprints from raw audio data
 * 
 * This class implements a simplified audio fingerprinting algorithm
 * inspired by Chromaprint. It extracts perceptual features from audio
 * and generates compact fingerprints for matching.
 */
class FingerprintGenerator {
public:
    struct AudioData {
        std::vector<float> samples;
        uint32_t sample_rate;
        uint32_t channels;
    };

    struct Fingerprint {
        std::vector<uint32_t> hash_values;
        uint64_t duration_ms;
        std::string raw_hash;  // Hex string representation
        
        Fingerprint() : duration_ms(0) {}
    };

    FingerprintGenerator();
    ~FingerprintGenerator() = default;

    /**
     * @brief Generate fingerprint from audio samples
     * @param audio Raw audio data
     * @return Generated fingerprint
     */
    Fingerprint generate(const AudioData& audio);

    /**
     * @brief Generate fingerprint from audio file
     * @param filepath Path to audio file
     * @return Generated fingerprint
     */
    Fingerprint generateFromFile(const std::string& filepath);

    /**
     * @brief Calculate similarity between two fingerprints
     * @param fp1 First fingerprint
     * @param fp2 Second fingerprint
     * @return Similarity score (0.0 to 1.0)
     */
    static double calculateSimilarity(const Fingerprint& fp1, const Fingerprint& fp2);

private:
    // FFT frame size
    static constexpr size_t FRAME_SIZE = 4096;
    static constexpr size_t HOP_SIZE = FRAME_SIZE / 2;
    static constexpr size_t NUM_BANDS = 33;

    /**
     * @brief Extract spectral features from audio frame
     */
    std::array<float, NUM_BANDS> extractSpectralFeatures(
        const std::vector<float>& frame);

    /**
     * @brief Apply Hamming window to frame
     */
    void applyWindow(std::vector<float>& frame);

    /**
     * @brief Compute FFT (simplified version)
     */
    std::vector<float> computeFFT(const std::vector<float>& frame);

    /**
     * @brief Convert spectral features to hash
     */
    uint32_t featuresToHash(const std::array<float, NUM_BANDS>& features,
                            const std::array<float, NUM_BANDS>& prev_features);

    std::array<float, NUM_BANDS> prev_features_;
};

} // namespace core
} // namespace vfs

#endif // FINGERPRINT_GENERATOR_H
