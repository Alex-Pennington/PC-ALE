/**
 * @file lqa_database.h
 * @brief Link Quality Analysis Database for PC-ALE 2.0
 * 
 * MIL-STD-188-141B Appendix A: LQA (Link Quality Analysis) System
 * 
 * Persistent storage of channel quality metrics with per-channel, per-station
 * tracking. Implements time-weighted averaging (recent data weighted higher),
 * configurable history depth, and save/load for persistence across sessions.
 * 
 * Clean-room implementation from MIL-STD-188-141B specification.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace ale {

/**
 * @brief Single LQA entry for a specific channel/station combination
 * 
 * Tracks all quality metrics for one remote station on one frequency.
 */
struct LQAEntry {
    uint32_t frequency_hz;           ///< Channel frequency in Hz
    std::string remote_station;      ///< Remote station address (or "" for sounding)
    float snr_db;                    ///< Signal-to-Noise Ratio (dB)
    float ber;                       ///< Bit Error Rate (0.0 - 1.0)
    float sinad_db;                  ///< SINAD (Signal+Noise+Distortion to Noise+Distortion)
    int fec_errors;                  ///< Total Golay FEC errors corrected
    int total_words;                 ///< Total ALE words received
    float multipath_score;           ///< Multipath severity (0.0=none, 1.0=severe)
    float noise_floor_dbm;           ///< Noise floor measurement (dBm)
    uint32_t last_sounding_ms;       ///< Timestamp of last sounding (ms since epoch)
    uint32_t last_contact_ms;        ///< Timestamp of last contact (ms since epoch)
    float score;                     ///< Computed composite LQA score (0-31)
    uint32_t sample_count;           ///< Number of samples in this entry
    
    /**
     * @brief Default constructor
     */
    LQAEntry() 
        : frequency_hz(0), remote_station(""), snr_db(0.0f), ber(0.0f),
          sinad_db(0.0f), fec_errors(0), total_words(0), 
          multipath_score(0.0f), noise_floor_dbm(-120.0f),
          last_sounding_ms(0), last_contact_ms(0), score(0.0f),
          sample_count(0) {}
};

/**
 * @brief Configuration parameters for LQA scoring algorithm
 * 
 * Weights determine relative importance of different quality factors.
 * All weights should sum to 1.0 for normalized scoring.
 */
struct LQAConfig {
    float snr_weight = 0.5f;         ///< Weight for SNR in composite score
    float success_weight = 0.3f;     ///< Weight for successful reception rate
    float recency_weight = 0.2f;     ///< Weight for recent contact
    uint32_t max_age_ms = 3600000;   ///< Max age before entry expires (1 hour)
    uint32_t history_depth = 100;    ///< Max entries per channel/station
    float time_decay_factor = 0.9f;  ///< Decay factor for time-weighted averaging
    
    // Thresholds for quality assessment
    float good_snr_db = 20.0f;       ///< SNR threshold for "good" quality
    float poor_snr_db = 6.0f;        ///< SNR threshold for "poor" quality
    float good_ber = 0.001f;         ///< BER threshold for "good" quality
    float poor_ber = 0.1f;           ///< BER threshold for "poor" quality
};

/**
 * @brief LQA Database for storing and managing channel quality data
 * 
 * Provides persistent storage of Link Quality Analysis metrics for all
 * channels and stations. Implements time-weighted averaging to give
 * more importance to recent measurements while maintaining history.
 * 
 * Usage:
 * @code
 * LQADatabase db;
 * db.set_config(config);
 * 
 * // Update with new measurement
 * db.update_entry(7073000, "REMOTE", snr, ber, fec_errs, total_words);
 * 
 * // Get current LQA for decision making
 * auto entry = db.get_entry(7073000, "REMOTE");
 * if (entry && entry->score > 20.0f) {
 *     // Good quality, proceed with call
 * }
 * 
 * // Save to disk
 * db.save_to_file("lqa_data.db");
 * @endcode
 */
class LQADatabase {
public:
    /**
     * @brief Construct empty LQA database
     */
    LQADatabase();
    
    /**
     * @brief Destructor
     */
    ~LQADatabase();
    
    /**
     * @brief Set configuration parameters
     * @param config Configuration struct with weights and thresholds
     */
    void set_config(const LQAConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    LQAConfig get_config() const;
    
    /**
     * @brief Update LQA entry with new measurement
     * 
     * If entry exists, performs time-weighted averaging with existing data.
     * If entry doesn't exist, creates new entry.
     * 
     * @param frequency_hz Channel frequency in Hz
     * @param remote_station Remote station address (empty string for sounding)
     * @param snr_db Signal-to-Noise Ratio in dB
     * @param ber Bit Error Rate (0.0 - 1.0)
     * @param fec_errors Number of FEC errors corrected
     * @param total_words Total words received
     * @param timestamp_ms Timestamp of measurement (0 = use current time)
     */
    void update_entry(uint32_t frequency_hz,
                     const std::string& remote_station,
                     float snr_db,
                     float ber,
                     int fec_errors,
                     int total_words,
                     uint32_t timestamp_ms = 0);
    
    /**
     * @brief Update LQA entry with full metrics
     * 
     * Extended version allowing SINAD, multipath, noise floor updates.
     * 
     * @param frequency_hz Channel frequency in Hz
     * @param remote_station Remote station address
     * @param snr_db Signal-to-Noise Ratio in dB
     * @param ber Bit Error Rate (0.0 - 1.0)
     * @param sinad_db SINAD measurement in dB
     * @param multipath_score Multipath severity (0.0 - 1.0)
     * @param noise_floor_dbm Noise floor in dBm
     * @param fec_errors Number of FEC errors corrected
     * @param total_words Total words received
     * @param timestamp_ms Timestamp of measurement (0 = use current time)
     */
    void update_entry_extended(uint32_t frequency_hz,
                              const std::string& remote_station,
                              float snr_db,
                              float ber,
                              float sinad_db,
                              float multipath_score,
                              float noise_floor_dbm,
                              int fec_errors,
                              int total_words,
                              uint32_t timestamp_ms = 0);
    
    /**
     * @brief Get LQA entry for specific channel/station
     * 
     * @param frequency_hz Channel frequency in Hz
     * @param remote_station Remote station address (empty for sounding)
     * @return Pointer to entry if found, nullptr otherwise
     */
    std::shared_ptr<LQAEntry> get_entry(uint32_t frequency_hz,
                                        const std::string& remote_station) const;
    
    /**
     * @brief Get all LQA entries for a specific channel
     * 
     * @param frequency_hz Channel frequency in Hz
     * @return Vector of all entries for this channel
     */
    std::vector<LQAEntry> get_entries_for_channel(uint32_t frequency_hz) const;
    
    /**
     * @brief Get all LQA entries for a specific station (across all channels)
     * 
     * @param remote_station Remote station address
     * @return Vector of all entries for this station
     */
    std::vector<LQAEntry> get_entries_for_station(const std::string& remote_station) const;
    
    /**
     * @brief Get all LQA entries in database
     * 
     * @return Vector of all entries
     */
    std::vector<LQAEntry> get_all_entries() const;
    
    /**
     * @brief Remove stale entries older than max_age_ms
     * 
     * @return Number of entries removed
     */
    int prune_stale_entries();
    
    /**
     * @brief Clear all entries from database
     */
    void clear();
    
    /**
     * @brief Save database to file
     * 
     * Saves in binary format for efficient storage and fast loading.
     * 
     * @param filepath Path to save file
     * @return true if successful, false on error
     */
    bool save_to_file(const std::string& filepath) const;
    
    /**
     * @brief Load database from file
     * 
     * Replaces current database contents with loaded data.
     * 
     * @param filepath Path to load file
     * @return true if successful, false on error
     */
    bool load_from_file(const std::string& filepath);
    
    /**
     * @brief Export database to CSV for analysis
     * 
     * @param filepath Path to CSV file
     * @return true if successful, false on error
     */
    bool export_to_csv(const std::string& filepath) const;
    
    /**
     * @brief Get number of entries in database
     * 
     * @return Total entry count
     */
    size_t get_entry_count() const;
    
    /**
     * @brief Compute composite LQA score for an entry
     * 
     * Implements weighted scoring algorithm using SNR, success rate, and recency.
     * Score range: 0.0 (worst) to 31.0 (best) per MIL-STD-188-141B.
     * 
     * @param entry Entry to score
     * @return Composite score (0-31)
     */
    float compute_score(const LQAEntry& entry) const;

private:
    /**
     * @brief Internal key for database map
     */
    struct EntryKey {
        uint32_t frequency_hz;
        std::string remote_station;
        
        bool operator<(const EntryKey& other) const {
            if (frequency_hz != other.frequency_hz)
                return frequency_hz < other.frequency_hz;
            return remote_station < other.remote_station;
        }
    };
    
    /**
     * @brief Get current timestamp in milliseconds
     * @return Milliseconds since epoch
     */
    uint32_t get_current_time_ms() const;
    
    /**
     * @brief Perform time-weighted averaging between old and new values
     * 
     * @param old_value Previous measurement
     * @param new_value New measurement
     * @param old_samples Number of samples in old value
     * @return Weighted average
     */
    float time_weighted_average(float old_value, float new_value, 
                               uint32_t old_samples) const;
    
    LQAConfig config_;                           ///< Configuration parameters
    std::map<EntryKey, LQAEntry> entries_;      ///< Database of LQA entries
};

} // namespace ale
