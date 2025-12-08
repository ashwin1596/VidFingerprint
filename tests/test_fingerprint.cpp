#include "core/fingerprint_generator.h"
#include <iostream>
#include <cassert>
#include <cmath>


using namespace vfs::core;

void testBasicFingerprinting() {
    std::cout << "Test: Basic Fingerprinting... ";
    
    FingerprintGenerator generator;
    
    // Create test audio data
    FingerprintGenerator::AudioData audio;
    audio.sample_rate = 44100;
    audio.channels = 1;
    audio.samples.resize(44100); // 1 second
    
    for (size_t i = 0; i < audio.samples.size(); ++i) {
        audio.samples[i] = std::sin(2.0 * M_PI * 440.0 * i / audio.sample_rate);
    }
    
    auto fingerprint = generator.generate(audio);
    
    assert(!fingerprint.hash_values.empty());
    assert(fingerprint.duration_ms > 0);
    assert(!fingerprint.raw_hash.empty());
    
    std::cout << "PASSED" << std::endl;
}

void testSimilarityCalculation() {
    std::cout << "Test: Similarity Calculation... ";
    
    FingerprintGenerator generator;
    
    auto fp1 = generator.generateFromFile("test1.wav");
    auto fp2 = generator.generateFromFile("test2.wav");
    auto fp3 = fp1; // Identical copy
    
    // Self-similarity should be high
    double self_sim = FingerprintGenerator::calculateSimilarity(fp1, fp3);
    assert(self_sim > 0.95);
    
    // Different content should have lower similarity
    double diff_sim = FingerprintGenerator::calculateSimilarity(fp1, fp2);
    assert(diff_sim < self_sim);
    
    std::cout << "PASSED" << std::endl;
}

void testEmptyAudio() {
    std::cout << "Test: Empty Audio Handling... ";
    
    FingerprintGenerator generator;
    
    FingerprintGenerator::AudioData empty_audio;
    empty_audio.sample_rate = 44100;
    empty_audio.channels = 1;
    
    auto fingerprint = generator.generate(empty_audio);
    
    assert(fingerprint.hash_values.empty());
    assert(fingerprint.duration_ms == 0);
    
    std::cout << "PASSED" << std::endl;
}

void testConsistency() {
    std::cout << "Test: Fingerprint Consistency... ";
    
    FingerprintGenerator gen1;
    FingerprintGenerator gen2;
    
    auto fp1 = gen1.generateFromFile("test.wav");
    auto fp2 = gen2.generateFromFile("test.wav");
    
    // Same input should produce same fingerprint
    double similarity = FingerprintGenerator::calculateSimilarity(fp1, fp2);
    assert(similarity > 0.99);
    
    std::cout << "PASSED" << std::endl;
}

int main() {
    std::cout << "=== Fingerprint Generator Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        testBasicFingerprinting();
        testSimilarityCalculation();
        testEmptyAudio();
        testConsistency();
        
        std::cout << std::endl;
        std::cout << "All tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
