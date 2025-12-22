/**
 * @file lqa_database.cpp
 * @brief Implementation of LQA Database
 */

#include "ale/lqa_database.h"
#include <chrono>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace ale {

LQADatabase::LQADatabase() {
    // Default configuration already set in LQAConfig struct
}

LQADatabase::~LQADatabase() {
    // Nothing to clean up
}

void LQADatabase::set_config(const LQAConfig& config) {
    config_ = config;
}

LQAConfig LQADatabase::get_config() const {
    return config_;
}

uint32_t LQADatabase::get_current_time_ms() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

float LQADatabase::time_weighted_average(float old_value, float new_value, 
                                         uint32_t old_samples) const {
    // Time-weighted averaging: newer samples get more weight
    // Formula: weighted_avg = (old * decay * old_samples + new) / (old_samples * decay + 1)
    float decay = config_.time_decay_factor;
    float weighted_old = old_value * decay * old_samples;
    float total_weight = old_samples * decay + 1.0f;
    return (weighted_old + new_value) / total_weight;
}

void LQADatabase::update_entry(uint32_t frequency_hz,
                               const std::string& remote_station,
                               float snr_db,
                               float ber,
                               int fec_errors,
                               int total_words,
                               uint32_t timestamp_ms) {
    EntryKey key{frequency_hz, remote_station};
    uint32_t now = (timestamp_ms == 0) ? get_current_time_ms() : timestamp_ms;
    
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        // Entry exists - perform time-weighted averaging
        LQAEntry& entry = it->second;
        uint32_t old_samples = entry.sample_count;
        
        // Update metrics with time weighting
        entry.snr_db = time_weighted_average(entry.snr_db, snr_db, old_samples);
        entry.ber = time_weighted_average(entry.ber, ber, old_samples);
        entry.fec_errors += fec_errors;
        entry.total_words += total_words;
        entry.sample_count++;
        
        // Update timestamps
        if (!remote_station.empty()) {
            entry.last_contact_ms = now;
        } else {
            entry.last_sounding_ms = now;
        }
        
        // Recompute score
        entry.score = compute_score(entry);
        
    } else {
        // Create new entry
        LQAEntry entry;
        entry.frequency_hz = frequency_hz;
        entry.remote_station = remote_station;
        entry.snr_db = snr_db;
        entry.ber = ber;
        entry.fec_errors = fec_errors;
        entry.total_words = total_words;
        entry.sample_count = 1;
        
        if (!remote_station.empty()) {
            entry.last_contact_ms = now;
        } else {
            entry.last_sounding_ms = now;
        }
        
        // Compute initial score
        entry.score = compute_score(entry);
        
        entries_[key] = entry;
    }
}

void LQADatabase::update_entry_extended(uint32_t frequency_hz,
                                       const std::string& remote_station,
                                       float snr_db,
                                       float ber,
                                       float sinad_db,
                                       float multipath_score,
                                       float noise_floor_dbm,
                                       int fec_errors,
                                       int total_words,
                                       uint32_t timestamp_ms) {
    EntryKey key{frequency_hz, remote_station};
    uint32_t now = (timestamp_ms == 0) ? get_current_time_ms() : timestamp_ms;
    
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        // Entry exists - perform time-weighted averaging
        LQAEntry& entry = it->second;
        uint32_t old_samples = entry.sample_count;
        
        // Update all metrics with time weighting
        entry.snr_db = time_weighted_average(entry.snr_db, snr_db, old_samples);
        entry.ber = time_weighted_average(entry.ber, ber, old_samples);
        entry.sinad_db = time_weighted_average(entry.sinad_db, sinad_db, old_samples);
        entry.multipath_score = time_weighted_average(entry.multipath_score, 
                                                      multipath_score, old_samples);
        entry.noise_floor_dbm = time_weighted_average(entry.noise_floor_dbm, 
                                                      noise_floor_dbm, old_samples);
        entry.fec_errors += fec_errors;
        entry.total_words += total_words;
        entry.sample_count++;
        
        // Update timestamps
        if (!remote_station.empty()) {
            entry.last_contact_ms = now;
        } else {
            entry.last_sounding_ms = now;
        }
        
        // Recompute score
        entry.score = compute_score(entry);
        
    } else {
        // Create new entry
        LQAEntry entry;
        entry.frequency_hz = frequency_hz;
        entry.remote_station = remote_station;
        entry.snr_db = snr_db;
        entry.ber = ber;
        entry.sinad_db = sinad_db;
        entry.multipath_score = multipath_score;
        entry.noise_floor_dbm = noise_floor_dbm;
        entry.fec_errors = fec_errors;
        entry.total_words = total_words;
        entry.sample_count = 1;
        
        if (!remote_station.empty()) {
            entry.last_contact_ms = now;
        } else {
            entry.last_sounding_ms = now;
        }
        
        // Compute initial score
        entry.score = compute_score(entry);
        
        entries_[key] = entry;
    }
}

std::shared_ptr<LQAEntry> LQADatabase::get_entry(uint32_t frequency_hz,
                                                 const std::string& remote_station) const {
    EntryKey key{frequency_hz, remote_station};
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        return std::make_shared<LQAEntry>(it->second);
    }
    return nullptr;
}

std::vector<LQAEntry> LQADatabase::get_entries_for_channel(uint32_t frequency_hz) const {
    std::vector<LQAEntry> result;
    for (const auto& pair : entries_) {
        if (pair.first.frequency_hz == frequency_hz) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<LQAEntry> LQADatabase::get_entries_for_station(const std::string& remote_station) const {
    std::vector<LQAEntry> result;
    for (const auto& pair : entries_) {
        if (pair.first.remote_station == remote_station) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<LQAEntry> LQADatabase::get_all_entries() const {
    std::vector<LQAEntry> result;
    result.reserve(entries_.size());
    for (const auto& pair : entries_) {
        result.push_back(pair.second);
    }
    return result;
}

int LQADatabase::prune_stale_entries() {
    uint32_t now = get_current_time_ms();
    int removed = 0;
    
    auto it = entries_.begin();
    while (it != entries_.end()) {
        const LQAEntry& entry = it->second;
        uint32_t last_activity = std::max(entry.last_contact_ms, entry.last_sounding_ms);
        
        if ((now - last_activity) > config_.max_age_ms) {
            it = entries_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    return removed;
}

void LQADatabase::clear() {
    entries_.clear();
}

float LQADatabase::compute_score(const LQAEntry& entry) const {
    float score = 0.0f;
    
    // SNR component (0-31 scale)
    // Map SNR dB to 0-31: 0 dB = 0, 31 dB+ = 31
    float snr_component = std::min(31.0f, std::max(0.0f, entry.snr_db));
    score += snr_component * config_.snr_weight;
    
    // Success rate component (0-31 scale)
    // Based on BER: 0 BER = 31, high BER = 0
    float success_rate = 0.0f;
    if (entry.total_words > 0) {
        // Convert BER to success rate (1.0 - BER) and scale to 0-31
        success_rate = (1.0f - std::min(1.0f, entry.ber)) * 31.0f;
    }
    score += success_rate * config_.success_weight;
    
    // Recency component (0-31 scale)
    // Recent contact = 31, old contact = 0
    uint32_t now = get_current_time_ms();
    uint32_t last_activity = std::max(entry.last_contact_ms, entry.last_sounding_ms);
    
    if (last_activity > 0) {
        uint32_t age_ms = now - last_activity;
        float age_factor = 1.0f - (static_cast<float>(age_ms) / config_.max_age_ms);
        age_factor = std::max(0.0f, std::min(1.0f, age_factor));
        float recency = age_factor * 31.0f;
        score += recency * config_.recency_weight;
    }
    
    // Clamp to 0-31 range
    return std::min(31.0f, std::max(0.0f, score));
}

bool LQADatabase::save_to_file(const std::string& filepath) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write header
    const char magic[] = "PCALE_LQA";
    file.write(magic, sizeof(magic));
    
    uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Write config
    file.write(reinterpret_cast<const char*>(&config_), sizeof(config_));
    
    // Write entry count
    uint32_t count = static_cast<uint32_t>(entries_.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // Write each entry
    for (const auto& pair : entries_) {
        const LQAEntry& entry = pair.second;
        
        // Write frequency
        file.write(reinterpret_cast<const char*>(&entry.frequency_hz), 
                  sizeof(entry.frequency_hz));
        
        // Write station name (length-prefixed string)
        uint32_t name_len = static_cast<uint32_t>(entry.remote_station.length());
        file.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        file.write(entry.remote_station.c_str(), name_len);
        
        // Write metrics
        file.write(reinterpret_cast<const char*>(&entry.snr_db), sizeof(entry.snr_db));
        file.write(reinterpret_cast<const char*>(&entry.ber), sizeof(entry.ber));
        file.write(reinterpret_cast<const char*>(&entry.sinad_db), sizeof(entry.sinad_db));
        file.write(reinterpret_cast<const char*>(&entry.fec_errors), sizeof(entry.fec_errors));
        file.write(reinterpret_cast<const char*>(&entry.total_words), sizeof(entry.total_words));
        file.write(reinterpret_cast<const char*>(&entry.multipath_score), 
                  sizeof(entry.multipath_score));
        file.write(reinterpret_cast<const char*>(&entry.noise_floor_dbm), 
                  sizeof(entry.noise_floor_dbm));
        file.write(reinterpret_cast<const char*>(&entry.last_sounding_ms), 
                  sizeof(entry.last_sounding_ms));
        file.write(reinterpret_cast<const char*>(&entry.last_contact_ms), 
                  sizeof(entry.last_contact_ms));
        file.write(reinterpret_cast<const char*>(&entry.score), sizeof(entry.score));
        file.write(reinterpret_cast<const char*>(&entry.sample_count), 
                  sizeof(entry.sample_count));
    }
    
    file.close();
    return true;
}

bool LQADatabase::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read and verify header
    char magic[10];
    file.read(magic, sizeof(magic));
    if (std::string(magic) != "PCALE_LQA") {
        return false;
    }
    
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) {
        return false;  // Unknown version
    }
    
    // Read config
    file.read(reinterpret_cast<char*>(&config_), sizeof(config_));
    
    // Read entry count
    uint32_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    // Clear existing entries
    entries_.clear();
    
    // Read each entry
    for (uint32_t i = 0; i < count; i++) {
        LQAEntry entry;
        
        // Read frequency
        file.read(reinterpret_cast<char*>(&entry.frequency_hz), 
                 sizeof(entry.frequency_hz));
        
        // Read station name (length-prefixed string)
        uint32_t name_len;
        file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
        entry.remote_station.resize(name_len);
        file.read(&entry.remote_station[0], name_len);
        
        // Read metrics
        file.read(reinterpret_cast<char*>(&entry.snr_db), sizeof(entry.snr_db));
        file.read(reinterpret_cast<char*>(&entry.ber), sizeof(entry.ber));
        file.read(reinterpret_cast<char*>(&entry.sinad_db), sizeof(entry.sinad_db));
        file.read(reinterpret_cast<char*>(&entry.fec_errors), sizeof(entry.fec_errors));
        file.read(reinterpret_cast<char*>(&entry.total_words), sizeof(entry.total_words));
        file.read(reinterpret_cast<char*>(&entry.multipath_score), 
                 sizeof(entry.multipath_score));
        file.read(reinterpret_cast<char*>(&entry.noise_floor_dbm), 
                 sizeof(entry.noise_floor_dbm));
        file.read(reinterpret_cast<char*>(&entry.last_sounding_ms), 
                 sizeof(entry.last_sounding_ms));
        file.read(reinterpret_cast<char*>(&entry.last_contact_ms), 
                 sizeof(entry.last_contact_ms));
        file.read(reinterpret_cast<char*>(&entry.score), sizeof(entry.score));
        file.read(reinterpret_cast<char*>(&entry.sample_count), sizeof(entry.sample_count));
        
        // Store entry
        EntryKey key{entry.frequency_hz, entry.remote_station};
        entries_[key] = entry;
    }
    
    file.close();
    return true;
}

bool LQADatabase::export_to_csv(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Write CSV header
    file << "Frequency(Hz),Station,SNR(dB),BER,SINAD(dB),FEC_Errors,Total_Words,"
         << "Multipath,Noise_Floor(dBm),Last_Sounding_ms,Last_Contact_ms,Score,Samples\n";
    
    // Write each entry
    for (const auto& pair : entries_) {
        const LQAEntry& entry = pair.second;
        file << entry.frequency_hz << ","
             << entry.remote_station << ","
             << entry.snr_db << ","
             << entry.ber << ","
             << entry.sinad_db << ","
             << entry.fec_errors << ","
             << entry.total_words << ","
             << entry.multipath_score << ","
             << entry.noise_floor_dbm << ","
             << entry.last_sounding_ms << ","
             << entry.last_contact_ms << ","
             << entry.score << ","
             << entry.sample_count << "\n";
    }
    
    file.close();
    return true;
}

size_t LQADatabase::get_entry_count() const {
    return entries_.size();
}

} // namespace ale
