/**
 * @file lqa_analyzer.cpp
 * @brief Implementation of LQA Analyzer
 */

#include "ale/lqa_analyzer.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <set>

namespace ale {

LQAAnalyzer::LQAAnalyzer(LQADatabase* database)
    : database_(database), sounding_cb_(nullptr) {
}

void LQAAnalyzer::set_config(const AnalyzerConfig& config) {
    config_ = config;
}

AnalyzerConfig LQAAnalyzer::get_config() const {
    return config_;
}

void LQAAnalyzer::set_database(LQADatabase* database) {
    database_ = database;
}

uint32_t LQAAnalyzer::get_current_time_ms() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void LQAAnalyzer::process_sounding(const std::string& station,
                                  uint32_t frequency_hz,
                                  float snr_db,
                                  float ber,
                                  uint32_t timestamp_ms) {
    if (!database_) {
        return;
    }
    
    uint32_t now = (timestamp_ms == 0) ? get_current_time_ms() : timestamp_ms;
    
    // Update database with sounding result
    // For sounding, use empty station address to indicate general channel quality
    database_->update_entry(frequency_hz, "", snr_db, ber, 0, 1, now);
    
    // Also update for specific station
    database_->update_entry(frequency_hz, station, snr_db, ber, 0, 1, now);
}

void LQAAnalyzer::process_sounding_extended(const std::string& station,
                                           uint32_t frequency_hz,
                                           const MetricsSample& sample) {
    if (!database_) {
        return;
    }
    
    uint32_t now = (sample.timestamp_ms == 0) ? get_current_time_ms() : sample.timestamp_ms;
    
    // Estimate BER from decode success
    float ber = sample.decode_success ? 0.001f : 0.1f;
    
    // Update with full metrics
    database_->update_entry_extended(
        frequency_hz,
        station,
        sample.snr_db,
        ber,
        sample.snr_db,  // SINAD approximation (same as SNR for now)
        sample.multipath_delay_ms / 10.0f,  // Normalize to 0-1
        sample.noise_power_dbm,
        sample.fec_errors_corrected,
        1,  // One word received
        now
    );
}

std::shared_ptr<ChannelRank> LQAAnalyzer::get_best_channel_for_station(
    const std::string& station) const {
    
    if (!database_) {
        return nullptr;
    }
    
    auto entries = database_->get_entries_for_station(station);
    if (entries.empty()) {
        return nullptr;
    }
    
    // Find entry with highest score
    auto best = std::max_element(entries.begin(), entries.end(),
        [](const LQAEntry& a, const LQAEntry& b) {
            return a.score < b.score;
        });
    
    // Check if score meets minimum threshold
    if (best->score < config_.min_acceptable_score) {
        return nullptr;
    }
    
    uint32_t last_update = std::max(best->last_contact_ms, best->last_sounding_ms);
    
    return std::make_shared<ChannelRank>(
        best->frequency_hz,
        best->score,
        station,
        last_update
    );
}

std::shared_ptr<ChannelRank> LQAAnalyzer::get_best_channel() const {
    if (!database_) {
        return nullptr;
    }
    
    auto all_entries = database_->get_all_entries();
    if (all_entries.empty()) {
        return nullptr;
    }
    
    // Find entry with highest score
    auto best = std::max_element(all_entries.begin(), all_entries.end(),
        [](const LQAEntry& a, const LQAEntry& b) {
            return a.score < b.score;
        });
    
    // Check if score meets minimum threshold
    if (best->score < config_.min_acceptable_score) {
        return nullptr;
    }
    
    uint32_t last_update = std::max(best->last_contact_ms, best->last_sounding_ms);
    
    return std::make_shared<ChannelRank>(
        best->frequency_hz,
        best->score,
        best->remote_station,
        last_update
    );
}

std::vector<ChannelRank> LQAAnalyzer::rank_all_channels() const {
    std::vector<ChannelRank> ranks;
    
    if (!database_) {
        return ranks;
    }
    
    // Get all entries
    auto all_entries = database_->get_all_entries();
    
    // Group by frequency and compute aggregate scores
    std::map<uint32_t, std::vector<LQAEntry>> by_frequency;
    for (const auto& entry : all_entries) {
        by_frequency[entry.frequency_hz].push_back(entry);
    }
    
    // Create ranks
    for (const auto& pair : by_frequency) {
        uint32_t freq = pair.first;
        const auto& entries = pair.second;
        
        // Find best station on this channel
        auto best = std::max_element(entries.begin(), entries.end(),
            [](const LQAEntry& a, const LQAEntry& b) {
                return a.score < b.score;
            });
        
        // Compute aggregate score (average of all stations)
        float aggregate_score = 0.0f;
        uint32_t latest_update = 0;
        for (const auto& e : entries) {
            aggregate_score += e.score;
            uint32_t update = std::max(e.last_contact_ms, e.last_sounding_ms);
            latest_update = std::max(latest_update, update);
        }
        aggregate_score /= entries.size();
        
        ranks.emplace_back(freq, aggregate_score, best->remote_station, latest_update);
    }
    
    // Sort by score (highest first)
    std::sort(ranks.begin(), ranks.end(),
        [](const ChannelRank& a, const ChannelRank& b) {
            return a.score > b.score;
        });
    
    return ranks;
}

std::vector<ChannelRank> LQAAnalyzer::rank_channels_for_station(
    const std::string& station) const {
    
    std::vector<ChannelRank> ranks;
    
    if (!database_) {
        return ranks;
    }
    
    auto entries = database_->get_entries_for_station(station);
    
    for (const auto& entry : entries) {
        uint32_t last_update = std::max(entry.last_contact_ms, entry.last_sounding_ms);
        ranks.emplace_back(entry.frequency_hz, entry.score, station, last_update);
    }
    
    // Sort by score (highest first)
    std::sort(ranks.begin(), ranks.end(),
        [](const ChannelRank& a, const ChannelRank& b) {
            return a.score > b.score;
        });
    
    return ranks;
}

float LQAAnalyzer::compute_channel_aggregate_score(uint32_t frequency_hz) const {
    if (!database_) {
        return 0.0f;
    }
    
    auto entries = database_->get_entries_for_channel(frequency_hz);
    if (entries.empty()) {
        return 0.0f;
    }
    
    float total_score = 0.0f;
    for (const auto& entry : entries) {
        total_score += entry.score;
    }
    
    return total_score / entries.size();
}

bool LQAAnalyzer::is_sounding_due(uint32_t frequency_hz) const {
    if (!database_) {
        return true;  // No data, sounding is needed
    }
    
    auto entries = database_->get_entries_for_channel(frequency_hz);
    if (entries.empty()) {
        return true;  // No data, sounding is needed
    }
    
    // Find most recent sounding on this channel
    uint32_t latest_sounding = 0;
    for (const auto& entry : entries) {
        latest_sounding = std::max(latest_sounding, entry.last_sounding_ms);
    }
    
    if (latest_sounding == 0) {
        return true;  // Never sounded
    }
    
    uint32_t now = get_current_time_ms();
    uint32_t age = now - latest_sounding;
    
    return age >= config_.sounding_interval_ms;
}

std::vector<uint32_t> LQAAnalyzer::get_channels_needing_sounding() const {
    std::vector<uint32_t> channels;
    
    if (!database_) {
        return channels;
    }
    
    auto all_entries = database_->get_all_entries();
    
    // Get unique frequencies
    std::set<uint32_t> frequencies;
    for (const auto& entry : all_entries) {
        frequencies.insert(entry.frequency_hz);
    }
    
    // Check each frequency
    for (uint32_t freq : frequencies) {
        if (is_sounding_due(freq)) {
            channels.push_back(freq);
        }
    }
    
    return channels;
}

void LQAAnalyzer::set_sounding_callback(std::function<void(uint32_t)> callback) {
    sounding_cb_ = callback;
}

void LQAAnalyzer::update() {
    if (!database_) {
        return;
    }
    
    // Prune stale entries
    database_->prune_stale_entries();
    
    // Check for automatic sounding
    if (config_.enable_automatic_sounding && sounding_cb_) {
        auto channels = get_channels_needing_sounding();
        for (uint32_t freq : channels) {
            sounding_cb_(freq);
        }
    }
}

std::string LQAAnalyzer::score_to_quality_level(float score) const {
    if (score >= 25.0f) return "Excellent";
    if (score >= 20.0f) return "Good";
    if (score >= 15.0f) return "Fair";
    if (score >= 10.0f) return "Poor";
    return "Very Poor";
}

std::string LQAAnalyzer::get_channel_quality_summary(uint32_t frequency_hz) const {
    if (!database_) {
        return "No data";
    }
    
    auto entries = database_->get_entries_for_channel(frequency_hz);
    if (entries.empty()) {
        return "No data";
    }
    
    // Compute aggregate
    float avg_snr = 0.0f;
    float avg_score = 0.0f;
    for (const auto& e : entries) {
        avg_snr += e.snr_db;
        avg_score += e.score;
    }
    avg_snr /= entries.size();
    avg_score /= entries.size();
    
    std::ostringstream oss;
    oss << score_to_quality_level(avg_score)
        << " (SNR: " << std::fixed << std::setprecision(1) << avg_snr << "dB"
        << ", Score: " << std::fixed << std::setprecision(0) << avg_score << ")";
    
    return oss.str();
}

std::string LQAAnalyzer::get_station_quality_summary(const std::string& station,
                                                     uint32_t frequency_hz) const {
    if (!database_) {
        return "No data";
    }
    
    auto entry = database_->get_entry(frequency_hz, station);
    if (!entry) {
        return "No data";
    }
    
    std::ostringstream oss;
    oss << score_to_quality_level(entry->score)
        << " (SNR: " << std::fixed << std::setprecision(1) << entry->snr_db << "dB"
        << ", BER: " << std::scientific << std::setprecision(2) << entry->ber
        << ", Score: " << std::fixed << std::setprecision(0) << entry->score << ")";
    
    return oss.str();
}

} // namespace ale
