/**
 * @file lqa_analyzer.h
 * @brief LQA Analyzer for Sounding and Channel Selection
 * 
 * Analyzes sounding results, ranks channels by quality, and selects
 * best channels for outbound calls. Integrates with Phase 3 state machine.
 * 
 * Clean-room implementation from MIL-STD-188-141B specification.
 */

#pragma once

#include "ale/lqa_database.h"
#include "ale/lqa_metrics.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace ale {

/**
 * @brief Channel ranking entry
 * 
 * Associates a channel with its quality score for selection.
 */
struct ChannelRank {
    uint32_t frequency_hz;    ///< Channel frequency
    float score;              ///< Composite LQA score (0-31)
    std::string best_station; ///< Station with best LQA on this channel
    uint32_t last_update_ms;  ///< Last update timestamp
    
    ChannelRank()
        : frequency_hz(0), score(0.0f), best_station(""), last_update_ms(0) {}
    
    ChannelRank(uint32_t freq, float sc, const std::string& st, uint32_t ts)
        : frequency_hz(freq), score(sc), best_station(st), last_update_ms(ts) {}
};

/**
 * @brief Configuration for LQA analyzer
 */
struct AnalyzerConfig {
    float min_acceptable_score = 10.0f;   ///< Minimum score for usable channel
    uint32_t sounding_interval_ms = 300000; ///< Sounding interval (5 minutes)
    bool prefer_recent_contacts = true;   ///< Weight recent contacts higher
    bool enable_automatic_sounding = false; ///< Auto-sound periodically
};

/**
 * @brief LQA Analyzer
 * 
 * High-level LQA analysis for channel selection and quality management.
 * Processes sounding results, ranks channels, and provides best channel
 * selection for calls.
 * 
 * Usage:
 * @code
 * LQAAnalyzer analyzer(&lqa_database);
 * 
 * // Process received sounding (TIS word)
 * analyzer.process_sounding("REMOTE", frequency, snr, ber);
 * 
 * // Get best channel for call
 * auto best = analyzer.get_best_channel_for_station("REMOTE");
 * if (best) {
 *     radio.set_frequency(best->frequency_hz);
 *     state_machine.make_call("REMOTE");
 * }
 * 
 * // Rank all channels
 * auto ranked = analyzer.rank_all_channels();
 * for (const auto& rank : ranked) {
 *     std::cout << rank.frequency_hz << ": " << rank.score << std::endl;
 * }
 * @endcode
 */
class LQAAnalyzer {
public:
    /**
     * @brief Construct analyzer
     * @param database LQA database to analyze
     */
    explicit LQAAnalyzer(LQADatabase* database);
    
    /**
     * @brief Set configuration
     * @param config Configuration parameters
     */
    void set_config(const AnalyzerConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Configuration
     */
    AnalyzerConfig get_config() const;
    
    /**
     * @brief Set LQA database
     * @param database Database pointer
     */
    void set_database(LQADatabase* database);
    
    /**
     * @brief Process received sounding (TIS word)
     * 
     * Updates LQA database with sounding result.
     * 
     * @param station Station that sent sounding
     * @param frequency_hz Channel frequency
     * @param snr_db Measured SNR
     * @param ber Estimated BER
     * @param timestamp_ms Sounding timestamp (0 = current time)
     */
    void process_sounding(const std::string& station,
                         uint32_t frequency_hz,
                         float snr_db,
                         float ber,
                         uint32_t timestamp_ms = 0);
    
    /**
     * @brief Process received sounding with full metrics
     * 
     * @param station Station that sent sounding
     * @param frequency_hz Channel frequency
     * @param sample Full metrics sample
     */
    void process_sounding_extended(const std::string& station,
                                   uint32_t frequency_hz,
                                   const MetricsSample& sample);
    
    /**
     * @brief Get best channel for calling specific station
     * 
     * Selects channel with highest LQA score for given station.
     * 
     * @param station Target station address
     * @return Best channel rank, or nullptr if no suitable channel
     */
    std::shared_ptr<ChannelRank> get_best_channel_for_station(
        const std::string& station) const;
    
    /**
     * @brief Get best overall channel (regardless of station)
     * 
     * Selects channel with highest aggregate LQA score.
     * 
     * @return Best channel rank, or nullptr if no channels
     */
    std::shared_ptr<ChannelRank> get_best_channel() const;
    
    /**
     * @brief Rank all channels by quality
     * 
     * Returns channels sorted by LQA score (highest first).
     * 
     * @return Vector of ranked channels
     */
    std::vector<ChannelRank> rank_all_channels() const;
    
    /**
     * @brief Rank channels for specific station
     * 
     * @param station Target station address
     * @return Vector of ranked channels for this station
     */
    std::vector<ChannelRank> rank_channels_for_station(
        const std::string& station) const;
    
    /**
     * @brief Check if sounding is due for a channel
     * 
     * Determines if sufficient time has passed since last sounding.
     * 
     * @param frequency_hz Channel frequency
     * @return true if sounding should be sent
     */
    bool is_sounding_due(uint32_t frequency_hz) const;
    
    /**
     * @brief Get channels that need sounding
     * 
     * @return Vector of frequencies needing sounding
     */
    std::vector<uint32_t> get_channels_needing_sounding() const;
    
    /**
     * @brief Register callback for sounding requests
     * 
     * Called when automatic sounding is enabled and sounding is due.
     * 
     * @param callback Function to call with frequency when sounding needed
     */
    void set_sounding_callback(std::function<void(uint32_t)> callback);
    
    /**
     * @brief Update analyzer (call periodically in main loop)
     * 
     * Checks for automatic sounding triggers, prunes stale data.
     */
    void update();
    
    /**
     * @brief Get quality summary for a channel
     * 
     * @param frequency_hz Channel frequency
     * @return Summary string (e.g., "Good (SNR: 22dB, Score: 28)")
     */
    std::string get_channel_quality_summary(uint32_t frequency_hz) const;
    
    /**
     * @brief Get quality summary for station on channel
     * 
     * @param station Station address
     * @param frequency_hz Channel frequency
     * @return Summary string
     */
    std::string get_station_quality_summary(const std::string& station,
                                            uint32_t frequency_hz) const;

private:
    /**
     * @brief Compute aggregate score for a channel
     * 
     * Averages LQA scores from all stations heard on this channel.
     * 
     * @param frequency_hz Channel frequency
     * @return Aggregate score
     */
    float compute_channel_aggregate_score(uint32_t frequency_hz) const;
    
    /**
     * @brief Get current timestamp in milliseconds
     * @return Milliseconds since epoch
     */
    uint32_t get_current_time_ms() const;
    
    /**
     * @brief Quality level from score
     * @param score LQA score (0-31)
     * @return Quality level string
     */
    std::string score_to_quality_level(float score) const;
    
    LQADatabase* database_;                      ///< LQA database
    AnalyzerConfig config_;                      ///< Configuration
    std::function<void(uint32_t)> sounding_cb_;  ///< Sounding callback
};

} // namespace ale
