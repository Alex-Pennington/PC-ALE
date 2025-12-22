/**
 * @file test_lqa_analyzer.cpp
 * @brief Unit tests for LQA Analyzer
 */

#include "ale/lqa_analyzer.h"
#include "ale/lqa_database.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <thread>
#include <chrono>

using namespace ale;

void test_analyzer_creation() {
    std::cout << "Test: Analyzer creation..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    auto config = analyzer.get_config();
    assert(config.min_acceptable_score > 0.0f);
    
    std::cout << "  PASS" << std::endl;
}

void test_process_sounding() {
    std::cout << "Test: Process sounding..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    // Process sounding from remote station
    analyzer.process_sounding("REMOTE", 7073000, 22.0f, 0.001f);
    
    // Should update database
    auto entry = db.get_entry(7073000, "REMOTE");
    assert(entry != nullptr);
    assert(std::abs(entry->snr_db - 22.0f) < 0.5f);
    
    std::cout << "  PASS" << std::endl;
}

void test_process_sounding_extended() {
    std::cout << "Test: Process sounding extended..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    MetricsSample sample;
    sample.snr_db = 25.0f;
    sample.signal_power_dbm = -45.0f;
    sample.noise_power_dbm = -70.0f;
    sample.fec_errors_corrected = 1;
    sample.decode_success = true;
    sample.multipath_delay_ms = 2.0f;
    
    analyzer.process_sounding_extended("REMOTE", 7073000, sample);
    
    auto entry = db.get_entry(7073000, "REMOTE");
    assert(entry != nullptr);
    assert(std::abs(entry->snr_db - 25.0f) < 0.5f);
    assert(entry->multipath_score > 0.0f);
    
    std::cout << "  PASS" << std::endl;
}

void test_best_channel_for_station() {
    std::cout << "Test: Best channel for station..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    // Add soundings on different channels
    analyzer.process_sounding("REMOTE", 7073000, 22.0f, 0.001f);   // Good
    analyzer.process_sounding("REMOTE", 10142000, 18.0f, 0.01f);   // Fair
    analyzer.process_sounding("REMOTE", 14107000, 28.0f, 0.0005f); // Excellent
    
    // Get best channel
    auto best = analyzer.get_best_channel_for_station("REMOTE");
    
    assert(best != nullptr);
    assert(best->frequency_hz == 14107000);  // Should select highest SNR
    assert(best->score > 20.0f);
    
    std::cout << "  Best channel: " << best->frequency_hz << " Hz, Score: " 
              << best->score << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_best_channel_overall() {
    std::cout << "Test: Best overall channel..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    // Add soundings from different stations
    analyzer.process_sounding("ALFA", 7073000, 20.0f, 0.001f);
    analyzer.process_sounding("BRAVO", 10142000, 25.0f, 0.0005f);
    analyzer.process_sounding("CHARLIE", 14107000, 18.0f, 0.01f);
    
    auto best = analyzer.get_best_channel();
    
    assert(best != nullptr);
    assert(best->score > 15.0f);
    
    std::cout << "  Best channel: " << best->frequency_hz << " Hz, Score: " 
              << best->score << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_rank_all_channels() {
    std::cout << "Test: Rank all channels..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    // Add data for multiple channels
    analyzer.process_sounding("REMOTE", 7073000, 22.0f, 0.001f);
    analyzer.process_sounding("REMOTE", 10142000, 18.0f, 0.01f);
    analyzer.process_sounding("REMOTE", 14107000, 25.0f, 0.0005f);
    
    auto ranked = analyzer.rank_all_channels();
    
    assert(ranked.size() == 3);
    
    // Should be sorted by score (highest first)
    assert(ranked[0].score >= ranked[1].score);
    assert(ranked[1].score >= ranked[2].score);
    
    std::cout << "  Ranked channels:" << std::endl;
    for (const auto& rank : ranked) {
        std::cout << "    " << rank.frequency_hz << " Hz: " 
                  << rank.score << std::endl;
    }
    std::cout << "  PASS" << std::endl;
}

void test_rank_channels_for_station() {
    std::cout << "Test: Rank channels for specific station..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    // Add data for REMOTE on multiple channels
    analyzer.process_sounding("REMOTE", 7073000, 22.0f, 0.001f);
    analyzer.process_sounding("REMOTE", 10142000, 18.0f, 0.01f);
    
    // Add data for OTHER on same channels (should not affect REMOTE ranking)
    analyzer.process_sounding("OTHER", 7073000, 15.0f, 0.1f);
    
    auto ranked = analyzer.rank_channels_for_station("REMOTE");
    
    assert(ranked.size() == 2);  // Only REMOTE's channels
    assert(ranked[0].best_station == "REMOTE");
    
    std::cout << "  PASS" << std::endl;
}

void test_sounding_due() {
    std::cout << "Test: Sounding due detection..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    AnalyzerConfig config;
    config.sounding_interval_ms = 100;  // Very short for testing
    analyzer.set_config(config);
    
    // Channel with no data - sounding should be due
    assert(analyzer.is_sounding_due(7073000));
    
    // Add recent sounding
    analyzer.process_sounding("REMOTE", 7073000, 20.0f, 0.01f);
    
    // Sounding should not be due immediately
    assert(!analyzer.is_sounding_due(7073000));
    
    // Wait for interval
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // Now sounding should be due
    assert(analyzer.is_sounding_due(7073000));
    
    std::cout << "  PASS" << std::endl;
}

void test_channels_needing_sounding() {
    std::cout << "Test: Channels needing sounding..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    AnalyzerConfig config;
    config.sounding_interval_ms = 100;
    analyzer.set_config(config);
    
    // Add soundings
    analyzer.process_sounding("REMOTE", 7073000, 20.0f, 0.01f);
    analyzer.process_sounding("REMOTE", 10142000, 22.0f, 0.005f);
    
    // Wait for interval
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    auto channels = analyzer.get_channels_needing_sounding();
    
    assert(channels.size() == 2);
    
    std::cout << "  Channels needing sounding: " << channels.size() << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_quality_summary() {
    std::cout << "Test: Quality summary..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    // Add data
    analyzer.process_sounding("REMOTE", 7073000, 25.0f, 0.001f);
    
    std::string summary = analyzer.get_channel_quality_summary(7073000);
    
    assert(!summary.empty());
    assert(summary.find("dB") != std::string::npos);
    
    std::cout << "  Summary: " << summary << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_station_quality_summary() {
    std::cout << "Test: Station quality summary..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    analyzer.process_sounding("REMOTE", 7073000, 22.0f, 0.001f);
    
    std::string summary = analyzer.get_station_quality_summary("REMOTE", 7073000);
    
    assert(!summary.empty());
    assert(summary.find("SNR") != std::string::npos);
    assert(summary.find("BER") != std::string::npos);
    
    std::cout << "  Summary: " << summary << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_min_acceptable_score() {
    std::cout << "Test: Minimum acceptable score threshold..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    AnalyzerConfig config;
    config.min_acceptable_score = 20.0f;  // High threshold
    analyzer.set_config(config);
    
    // Add low quality channel
    analyzer.process_sounding("REMOTE", 7073000, 10.0f, 0.1f);  // Poor quality
    
    // Should return nullptr (below threshold)
    auto best = analyzer.get_best_channel_for_station("REMOTE");
    assert(best == nullptr);
    
    // Add high quality channel
    analyzer.process_sounding("REMOTE", 10142000, 28.0f, 0.001f);  // Excellent
    
    // Should return the good channel
    best = analyzer.get_best_channel_for_station("REMOTE");
    assert(best != nullptr);
    assert(best->frequency_hz == 10142000);
    
    std::cout << "  PASS" << std::endl;
}

void test_sounding_callback() {
    std::cout << "Test: Sounding callback..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    AnalyzerConfig config;
    config.enable_automatic_sounding = true;
    config.sounding_interval_ms = 50;
    analyzer.set_config(config);
    
    // Track sounding requests
    std::vector<uint32_t> sounding_requests;
    analyzer.set_sounding_callback([&](uint32_t freq) {
        sounding_requests.push_back(freq);
    });
    
    // Add old sounding
    analyzer.process_sounding("REMOTE", 7073000, 20.0f, 0.01f);
    
    // Wait for interval
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Update should trigger callback
    analyzer.update();
    
    assert(!sounding_requests.empty());
    
    std::cout << "  Sounding requests: " << sounding_requests.size() << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_configuration() {
    std::cout << "Test: Configuration..." << std::endl;
    
    LQADatabase db;
    LQAAnalyzer analyzer(&db);
    
    AnalyzerConfig config;
    config.min_acceptable_score = 15.0f;
    config.sounding_interval_ms = 600000;
    config.prefer_recent_contacts = false;
    
    analyzer.set_config(config);
    
    auto retrieved = analyzer.get_config();
    assert(std::abs(retrieved.min_acceptable_score - 15.0f) < 0.1f);
    assert(retrieved.sounding_interval_ms == 600000);
    assert(retrieved.prefer_recent_contacts == false);
    
    std::cout << "  PASS" << std::endl;
}

int main() {
    std::cout << "=== LQA Analyzer Tests ===" << std::endl;
    
    test_analyzer_creation();
    test_process_sounding();
    test_process_sounding_extended();
    test_best_channel_for_station();
    test_best_channel_overall();
    test_rank_all_channels();
    test_rank_channels_for_station();
    test_sounding_due();
    test_channels_needing_sounding();
    test_quality_summary();
    test_station_quality_summary();
    test_min_acceptable_score();
    test_sounding_callback();
    test_configuration();
    
    std::cout << "\n=== All LQA Analyzer Tests Passed ===" << std::endl;
    return 0;
}
