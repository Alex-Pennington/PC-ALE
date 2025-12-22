/**
 * @file test_lqa_metrics.cpp
 * @brief Unit tests for LQA Metrics Collector
 */

#include "ale/lqa_metrics.h"
#include "ale/lqa_database.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

using namespace ale;

void test_metrics_creation() {
    std::cout << "Test: Metrics collector creation..." << std::endl;
    
    LQAMetrics metrics;
    assert(metrics.get_sample_count() == 0);
    
    std::cout << "  PASS" << std::endl;
}

void test_add_sample() {
    std::cout << "Test: Add metrics sample..." << std::endl;
    
    LQAMetrics metrics;
    
    MetricsSample sample;
    sample.snr_db = 20.0f;
    sample.signal_power_dbm = -50.0f;
    sample.noise_power_dbm = -70.0f;
    sample.fec_errors_corrected = 1;
    sample.decode_success = true;
    
    metrics.add_sample(sample, 7073000, "REMOTE");
    
    assert(metrics.get_sample_count() == 1);
    
    std::cout << "  PASS" << std::endl;
}

void test_averaging_window() {
    std::cout << "Test: Averaging window..." << std::endl;
    
    LQADatabase db;
    LQAMetrics metrics(&db);
    
    MetricsConfig config;
    config.averaging_window = 5;
    metrics.set_config(config);
    
    // Add 5 samples (fills window)
    for (int i = 0; i < 5; i++) {
        MetricsSample sample;
        sample.snr_db = 20.0f + i;  // 20, 21, 22, 23, 24
        sample.fec_errors_corrected = 1;
        metrics.add_sample(sample, 7073000, "REMOTE");
    }
    
    // Window should have rolled over (keeping 1 sample)
    assert(metrics.get_sample_count() == 1);
    
    // Database should have entry
    auto entry = db.get_entry(7073000, "REMOTE");
    assert(entry != nullptr);
    assert(entry->total_words == 5);
    
    std::cout << "  PASS" << std::endl;
}

void test_ber_estimation() {
    std::cout << "Test: BER estimation..." << std::endl;
    
    LQAMetrics metrics;
    
    // Perfect reception (no errors)
    float ber1 = metrics.estimate_ber(0, 100);
    assert(ber1 == 0.0f);
    
    // Some errors
    float ber2 = metrics.estimate_ber(10, 100);
    assert(ber2 > 0.0f);
    assert(ber2 < 1.0f);
    
    // High error rate
    float ber3 = metrics.estimate_ber(50, 100);
    assert(ber3 > ber2);
    
    std::cout << "  BER estimates: " << ber1 << ", " << ber2 << ", " << ber3 << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_sinad_calculation() {
    std::cout << "Test: SINAD calculation..." << std::endl;
    
    LQAMetrics metrics;
    
    // High SNR should give high SINAD
    float sinad1 = metrics.calculate_sinad(30.0f, -30.0f);
    assert(sinad1 > 20.0f);
    
    // Low SNR should give lower SINAD
    float sinad2 = metrics.calculate_sinad(10.0f, -30.0f);
    assert(sinad2 < sinad1);
    
    std::cout << "  SINAD values: " << sinad1 << ", " << sinad2 << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_multipath_detection() {
    std::cout << "Test: Multipath detection..." << std::endl;
    
    LQAMetrics metrics;
    
    // No multipath (stable signal)
    std::vector<float> stable_signal = {-50.0f, -50.1f, -49.9f, -50.0f, -50.1f};
    float mp1 = metrics.detect_multipath(stable_signal);
    assert(mp1 < 0.2f);  // Low multipath score
    
    // Multipath (varying signal)
    std::vector<float> fading_signal = {-50.0f, -45.0f, -55.0f, -48.0f, -52.0f};
    float mp2 = metrics.detect_multipath(fading_signal);
    assert(mp2 > mp1);  // Higher multipath score
    
    std::cout << "  Multipath scores: " << mp1 << ", " << mp2 << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_noise_floor_measurement() {
    std::cout << "Test: Noise floor measurement..." << std::endl;
    
    LQAMetrics metrics;
    
    std::vector<float> noise_samples = {-120.0f, -118.0f, -122.0f, -119.0f};
    float noise_floor = metrics.measure_noise_floor(noise_samples);
    
    // Should return minimum
    assert(std::abs(noise_floor + 122.0f) < 0.1f);
    
    std::cout << "  Noise floor: " << noise_floor << " dBm" << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_database_integration() {
    std::cout << "Test: Database integration..." << std::endl;
    
    LQADatabase db;
    LQAMetrics metrics(&db);
    
    MetricsConfig config;
    config.averaging_window = 3;
    config.enable_sinad = true;
    config.enable_multipath = true;
    metrics.set_config(config);
    
    // Add samples to trigger database update
    for (int i = 0; i < 3; i++) {
        MetricsSample sample;
        sample.snr_db = 22.0f;
        sample.signal_power_dbm = -50.0f;
        sample.noise_power_dbm = -72.0f;
        sample.fec_errors_corrected = 1;
        sample.decode_success = true;
        
        metrics.add_sample(sample, 7073000, "REMOTE");
    }
    
    // Database should have entry with extended metrics
    auto entry = db.get_entry(7073000, "REMOTE");
    assert(entry != nullptr);
    assert(std::abs(entry->snr_db - 22.0f) < 0.5f);
    assert(entry->total_words == 3);
    assert(entry->sinad_db > 0.0f);
    
    std::cout << "  Database entry - SNR: " << entry->snr_db 
              << ", SINAD: " << entry->sinad_db
              << ", Score: " << entry->score << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_averaged_sample() {
    std::cout << "Test: Averaged sample..." << std::endl;
    
    LQAMetrics metrics;
    
    // Add multiple samples
    for (int i = 0; i < 5; i++) {
        MetricsSample sample;
        sample.snr_db = 20.0f + i;  // 20, 21, 22, 23, 24
        sample.signal_power_dbm = -50.0f - i;
        metrics.add_sample(sample, 7073000, "REMOTE");
    }
    
    // Get average
    auto avg = metrics.get_averaged_sample();
    
    // Average SNR should be around 22.0 (middle value)
    assert(std::abs(avg.snr_db - 22.0f) < 1.0f);
    
    std::cout << "  Average SNR: " << avg.snr_db << " dB" << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_reset() {
    std::cout << "Test: Reset metrics..." << std::endl;
    
    LQAMetrics metrics;
    
    // Add samples
    for (int i = 0; i < 5; i++) {
        MetricsSample sample;
        sample.snr_db = 20.0f;
        metrics.add_sample(sample, 7073000, "REMOTE");
    }
    
    assert(metrics.get_sample_count() > 0);
    
    // Reset
    metrics.reset();
    assert(metrics.get_sample_count() == 0);
    
    std::cout << "  PASS" << std::endl;
}

void test_multiple_frequencies() {
    std::cout << "Test: Multiple frequencies..." << std::endl;
    
    LQADatabase db;
    LQAMetrics metrics(&db);
    
    MetricsConfig config;
    config.averaging_window = 2;
    metrics.set_config(config);
    
    // Add samples for different frequencies
    for (int i = 0; i < 2; i++) {
        MetricsSample sample;
        sample.snr_db = 22.0f;
        sample.fec_errors_corrected = 1;
        metrics.add_sample(sample, 7073000, "REMOTE");
    }
    
    for (int i = 0; i < 2; i++) {
        MetricsSample sample;
        sample.snr_db = 18.0f;
        sample.fec_errors_corrected = 2;
        metrics.add_sample(sample, 10142000, "REMOTE");
    }
    
    // Database should have entries for both frequencies
    auto entry1 = db.get_entry(7073000, "REMOTE");
    auto entry2 = db.get_entry(10142000, "REMOTE");
    
    assert(entry1 != nullptr);
    assert(entry2 != nullptr);
    assert(entry1->snr_db > entry2->snr_db);  // Different SNR
    
    std::cout << "  PASS" << std::endl;
}

void test_configuration() {
    std::cout << "Test: Configuration..." << std::endl;
    
    LQAMetrics metrics;
    
    MetricsConfig config;
    config.enable_sinad = false;
    config.enable_multipath = false;
    config.averaging_window = 20;
    config.multipath_threshold_db = 5.0f;
    
    metrics.set_config(config);
    
    auto retrieved = metrics.get_config();
    assert(retrieved.enable_sinad == false);
    assert(retrieved.enable_multipath == false);
    assert(retrieved.averaging_window == 20);
    assert(std::abs(retrieved.multipath_threshold_db - 5.0f) < 0.1f);
    
    std::cout << "  PASS" << std::endl;
}

int main() {
    std::cout << "=== LQA Metrics Tests ===" << std::endl;
    
    test_metrics_creation();
    test_add_sample();
    test_averaging_window();
    test_ber_estimation();
    test_sinad_calculation();
    test_multipath_detection();
    test_noise_floor_measurement();
    test_database_integration();
    test_averaged_sample();
    test_reset();
    test_multiple_frequencies();
    test_configuration();
    
    std::cout << "\n=== All LQA Metrics Tests Passed ===" << std::endl;
    return 0;
}
