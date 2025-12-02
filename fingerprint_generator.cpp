#include "core/fingerprint_generator.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace vfs {
namespace core {

FingerprintGenerator::FingerprintGenerator() {
    prev_features_.fill(0.0f);
}

FingerprintGenerator::Fingerprint 
FingerprintGenerator::generate(const AudioData& audio) {
    Fingerprint result;
    
    if (audio.samples.empty()) {
        return result;
    }

    // Calculate duration
    result.duration_ms = static_cast<uint64_t>(
        (audio.samples.size() * 1000.0) / audio.sample_rate
    );

    // Process audio in frames
    size_t num_frames = (audio.samples.size() - FRAME_SIZE) / HOP_SIZE + 1;
    result.hash_values.reserve(num_frames);

    std::vector<float> frame(FRAME_SIZE);
    std::array<float, NUM_BANDS> current_features;

    for (size_t i = 0; i < num_frames; ++i) {
        size_t start_idx = i * HOP_SIZE;
        
        // Extract frame
        std::copy_n(audio.samples.begin() + start_idx, 
                    FRAME_SIZE, 
                    frame.begin());

        // Apply window and extract features
        applyWindow(frame);
        current_features = extractSpectralFeatures(frame);

        // Generate hash from features
        uint32_t hash = featuresToHash(current_features, prev_features_);
        result.hash_values.push_back(hash);

        prev_features_ = current_features;
    }

    // Create raw hash string
    std::stringstream ss;
    for (uint32_t hash : result.hash_values) {
        ss << std::hex << std::setw(8) << std::setfill('0') << hash;
    }
    result.raw_hash = ss.str();

    return result;
}

FingerprintGenerator::Fingerprint 
FingerprintGenerator::generateFromFile(const std::string& filepath) {
    // For demonstration, generate synthetic audio data
    // In production, you'd use a library like libav/ffmpeg
    AudioData audio;
    audio.sample_rate = 44100;
    audio.channels = 1;
    
    // Generate 3 seconds of test audio with varying frequency
    size_t num_samples = audio.sample_rate * 3;
    audio.samples.resize(num_samples);
    
    for (size_t i = 0; i < num_samples; ++i) {
        float t = static_cast<float>(i) / audio.sample_rate;
        // Mix of frequencies to create unique fingerprint
        audio.samples[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * t) +
                          0.3f * std::sin(2.0f * M_PI * 880.0f * t) +
                          0.2f * std::sin(2.0f * M_PI * 1320.0f * t);
    }

    return generate(audio);
}

void FingerprintGenerator::applyWindow(std::vector<float>& frame) {
    // Apply Hamming window
    for (size_t i = 0; i < frame.size(); ++i) {
        float window = 0.54f - 0.46f * std::cos(2.0f * M_PI * i / (frame.size() - 1));
        frame[i] *= window;
    }
}

std::vector<float> FingerprintGenerator::computeFFT(const std::vector<float>& frame) {
    // Simplified magnitude spectrum calculation
    // In production, use FFTW or similar library
    std::vector<float> magnitude(FRAME_SIZE / 2);
    
    for (size_t k = 0; k < magnitude.size(); ++k) {
        float real = 0.0f;
        float imag = 0.0f;
        
        for (size_t n = 0; n < frame.size(); ++n) {
            float angle = 2.0f * M_PI * k * n / frame.size();
            real += frame[n] * std::cos(angle);
            imag -= frame[n] * std::sin(angle);
        }
        
        magnitude[k] = std::sqrt(real * real + imag * imag);
    }
    
    return magnitude;
}

std::array<float, FingerprintGenerator::NUM_BANDS> 
FingerprintGenerator::extractSpectralFeatures(const std::vector<float>& frame) {
    std::array<float, NUM_BANDS> features;
    features.fill(0.0f);

    // Compute magnitude spectrum
    auto spectrum = computeFFT(frame);

    // Group spectrum into logarithmic frequency bands
    size_t bins_per_band = spectrum.size() / NUM_BANDS;
    
    for (size_t band = 0; band < NUM_BANDS; ++band) {
        float energy = 0.0f;
        size_t start_bin = band * bins_per_band;
        size_t end_bin = (band + 1) * bins_per_band;
        
        for (size_t bin = start_bin; bin < end_bin && bin < spectrum.size(); ++bin) {
            energy += spectrum[bin] * spectrum[bin];
        }
        
        // Convert to log scale
        features[band] = std::log1p(energy);
    }

    return features;
}

uint32_t FingerprintGenerator::featuresToHash(
    const std::array<float, NUM_BANDS>& features,
    const std::array<float, NUM_BANDS>& prev_features) {
    
    uint32_t hash = 0;
    
    // Create hash based on feature differences (temporal derivative)
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        float diff = features[i] - prev_features[i];
        if (diff > 0) {
            hash |= (1u << i);
        }
    }
    
    return hash;
}

double FingerprintGenerator::calculateSimilarity(
    const Fingerprint& fp1, 
    const Fingerprint& fp2) {
    
    if (fp1.hash_values.empty() || fp2.hash_values.empty()) {
        return 0.0;
    }

    // Use Hamming distance for hash comparison
    size_t min_length = std::min(fp1.hash_values.size(), fp2.hash_values.size());
    size_t matching_bits = 0;
    size_t total_bits = 0;

    for (size_t i = 0; i < min_length; ++i) {
        uint32_t xor_result = fp1.hash_values[i] ^ fp2.hash_values[i];
        
        // Count matching bits (inverted popcount)
        matching_bits += 32 - __builtin_popcount(xor_result);
        total_bits += 32;
    }

    return static_cast<double>(matching_bits) / total_bits;
}

} // namespace core
} // namespace vfs
