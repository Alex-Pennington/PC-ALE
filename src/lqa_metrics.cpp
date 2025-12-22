/**
 * @file lqa_metrics.cpp
 * @brief Implementation of LQA Metrics Collector
 */

#include "ale/lqa_metrics.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace ale {

LQAMetrics::LQAMetrics(LQADatabase* database)
    : database_(database) {
    accumulated_.frequency_hz = 0;
    accumulated_.total_fec_errors = 0;
    accumulated_.total_words = 0;
}

void LQAMetrics::set_config(const MetricsConfig& config) {
    config_ = config;
}

MetricsConfig LQAMetrics::get_config() const {
    return config_;
}

void LQAMetrics::set_database(LQADatabase* database) {
    database_ = database;
}

void LQAMetrics::add_sample(const MetricsSample& sample,
                           uint32_t frequency_hz,
                           const std::string& remote_station) {
    // Add to averaging window
    samples_.push_back(sample);
    
    // Store context for database update
    accumulated_.frequency_hz = frequency_hz;
    accumulated_.remote_station = remote_station;
    accumulated_.total_fec_errors += sample.fec_errors_corrected;
    accumulated_.total_words++;
    
    // When window is full, compute averages and update database
    if (samples_.size() >= config_.averaging_window) {
        update_database(frequency_hz, remote_station);
        
        // Keep last sample for continuity
        MetricsSample last = samples_.back();
        samples_.clear();
        samples_.push_back(last);
    }
}

void LQAMetrics::update_database(uint32_t frequency_hz, 
                                const std::string& remote_station) {
    if (!database_ || samples_.empty()) {
        return;
    }
    
    // Compute averaged metrics
    MetricsSample avg = compute_average();
    
    // Estimate BER from FEC errors
    float ber = estimate_ber(accumulated_.total_fec_errors, 
                            accumulated_.total_words);
    
    // Calculate SINAD (assume -30 dB distortion as typical)
    float sinad = calculate_sinad(avg.snr_db, -30.0f);
    
    // Detect multipath if enabled
    float multipath_score = 0.0f;
    if (config_.enable_multipath) {
        std::vector<float> signal_samples;
        for (const auto& s : samples_) {
            signal_samples.push_back(s.signal_power_dbm);
        }
        multipath_score = detect_multipath(signal_samples);
    }
    
    // Measure noise floor
    std::vector<float> noise_samples;
    for (const auto& s : samples_) {
        noise_samples.push_back(s.noise_power_dbm);
    }
    float noise_floor = measure_noise_floor(noise_samples);
    
    // Update database with extended metrics
    database_->update_entry_extended(
        frequency_hz,
        remote_station,
        avg.snr_db,
        ber,
        sinad,
        multipath_score,
        noise_floor,
        accumulated_.total_fec_errors,
        accumulated_.total_words,
        avg.timestamp_ms
    );
    
    // Reset accumulators for next window
    accumulated_.total_fec_errors = 0;
    accumulated_.total_words = 0;
}

MetricsSample LQAMetrics::compute_average() const {
    if (samples_.empty()) {
        return MetricsSample();
    }
    
    MetricsSample avg;
    float sum_snr = 0.0f;
    float sum_signal = 0.0f;
    float sum_noise = 0.0f;
    float sum_multipath = 0.0f;
    
    for (const auto& s : samples_) {
        sum_snr += s.snr_db;
        sum_signal += s.signal_power_dbm;
        sum_noise += s.noise_power_dbm;
        sum_multipath += s.multipath_delay_ms;
    }
    
    size_t n = samples_.size();
    avg.snr_db = sum_snr / n;
    avg.signal_power_dbm = sum_signal / n;
    avg.noise_power_dbm = sum_noise / n;
    avg.multipath_delay_ms = sum_multipath / n;
    
    // Use most recent timestamp
    avg.timestamp_ms = samples_.back().timestamp_ms;
    
    return avg;
}

float LQAMetrics::calculate_sinad(float snr_db, float distortion_db) const {
    // SINAD = 10 * log10((S+N+D)/(N+D))
    // Where S = signal, N = noise, D = distortion
    
    // Convert SNR to linear
    float snr_linear = std::pow(10.0f, snr_db / 10.0f);
    
    // Assume distortion is relative to signal
    float distortion_linear = std::pow(10.0f, distortion_db / 10.0f);
    
    // SINAD calculation
    // S+N+D = S + N + D (where N is 1 relative to signal at SNR)
    // N+D = 1 + D (relative to signal)
    float signal_plus_noise_plus_distortion = snr_linear + 1.0f + distortion_linear;
    float noise_plus_distortion = 1.0f + distortion_linear;
    
    float sinad_linear = signal_plus_noise_plus_distortion / noise_plus_distortion;
    float sinad_db = 10.0f * std::log10(sinad_linear);
    
    return sinad_db;
}

float LQAMetrics::estimate_ber(int errors_corrected, int total_words) const {
    if (total_words == 0) {
        return 0.0f;
    }
    
    // Golay (24,12) can correct up to 3 bit errors per 24-bit codeword
    // Each ALE word has 24 bits
    // BER estimation: errors / (total_bits)
    
    // Conservative estimate: assume each correction was 1 bit error
    // (actual could be 1-3 bits)
    float total_bits = total_words * 24.0f;
    float estimated_bit_errors = errors_corrected;  // Conservative (minimum)
    
    float ber = estimated_bit_errors / total_bits;
    
    // Clamp to valid range
    return std::min(1.0f, std::max(0.0f, ber));
}

float LQAMetrics::detect_multipath(const std::vector<float>& samples) const {
    if (samples.size() < 3) {
        return 0.0f;
    }
    
    // Multipath detection via signal variance
    // High variance in signal power indicates multipath fading
    
    // Calculate mean
    float mean = std::accumulate(samples.begin(), samples.end(), 0.0f) / samples.size();
    
    // Calculate variance
    float variance = 0.0f;
    for (float sample : samples) {
        float diff = sample - mean;
        variance += diff * diff;
    }
    variance /= samples.size();
    
    // Standard deviation
    float std_dev = std::sqrt(variance);
    
    // Normalize to 0-1 scale
    // Threshold: > 3 dB std dev indicates significant multipath
    float multipath_score = std_dev / config_.multipath_threshold_db;
    
    // Clamp to 0-1
    return std::min(1.0f, std::max(0.0f, multipath_score));
}

float LQAMetrics::measure_noise_floor(const std::vector<float>& samples) const {
    if (samples.empty()) {
        return -120.0f;  // Default very low noise floor
    }
    
    // Noise floor is the minimum power level observed
    float min_power = *std::min_element(samples.begin(), samples.end());
    
    return min_power;
}

MetricsSample LQAMetrics::get_averaged_sample() const {
    return compute_average();
}

void LQAMetrics::reset() {
    samples_.clear();
    accumulated_.total_fec_errors = 0;
    accumulated_.total_words = 0;
}

size_t LQAMetrics::get_sample_count() const {
    return samples_.size();
}

} // namespace ale
