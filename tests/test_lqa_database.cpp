/**
 * @file test_lqa_database.cpp
 * @brief Unit tests for LQA Database
 */

#include "ale/lqa_database.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <thread>
#include <chrono>
#include <fstream>

using namespace ale;

void test_database_creation() {
    std::cout << "Test: Database creation..." << std::endl;
    
    LQADatabase db;
    assert(db.get_entry_count() == 0);
    
    std::cout << "  PASS" << std::endl;
}

void test_basic_entry_update() {
    std::cout << "Test: Basic entry update..." << std::endl;
    
    LQADatabase db;
    
    // Add entry
    db.update_entry(7073000, "REMOTE", 20.0f, 0.01f, 2, 100);
    
    assert(db.get_entry_count() == 1);
    
    // Retrieve entry
    auto entry = db.get_entry(7073000, "REMOTE");
    assert(entry != nullptr);
    assert(entry->frequency_hz == 7073000);
    assert(entry->remote_station == "REMOTE");
    assert(std::abs(entry->snr_db - 20.0f) < 0.1f);
    assert(std::abs(entry->ber - 0.01f) < 0.001f);
    assert(entry->fec_errors == 2);
    assert(entry->total_words == 100);
    
    std::cout << "  PASS" << std::endl;
}

void test_time_weighted_averaging() {
    std::cout << "Test: Time-weighted averaging..." << std::endl;
    
    LQADatabase db;
    
    // First update
    db.update_entry(7073000, "REMOTE", 20.0f, 0.01f, 1, 10);
    
    auto entry1 = db.get_entry(7073000, "REMOTE");
    float snr1 = entry1->snr_db;
    
    // Second update (should average with first)
    db.update_entry(7073000, "REMOTE", 25.0f, 0.005f, 1, 10);
    
    auto entry2 = db.get_entry(7073000, "REMOTE");
    float snr2 = entry2->snr_db;
    
    // SNR should be weighted average (closer to second value due to decay)
    assert(snr2 > snr1);
    assert(snr2 < 25.0f);  // But not fully 25 due to averaging
    
    // Errors and words should accumulate
    assert(entry2->fec_errors == 2);
    assert(entry2->total_words == 20);
    assert(entry2->sample_count == 2);
    
    std::cout << "  PASS" << std::endl;
}

void test_extended_metrics() {
    std::cout << "Test: Extended metrics update..." << std::endl;
    
    LQADatabase db;
    
    db.update_entry_extended(
        7073000, "REMOTE",
        22.0f,    // SNR
        0.001f,   // BER
        20.0f,    // SINAD
        0.3f,     // Multipath score
        -110.0f,  // Noise floor
        1, 50
    );
    
    auto entry = db.get_entry(7073000, "REMOTE");
    assert(entry != nullptr);
    assert(std::abs(entry->sinad_db - 20.0f) < 0.1f);
    assert(std::abs(entry->multipath_score - 0.3f) < 0.01f);
    assert(std::abs(entry->noise_floor_dbm + 110.0f) < 0.1f);
    
    std::cout << "  PASS" << std::endl;
}

void test_multiple_stations() {
    std::cout << "Test: Multiple stations..." << std::endl;
    
    LQADatabase db;
    
    db.update_entry(7073000, "ALFA", 22.0f, 0.001f, 1, 50);
    db.update_entry(7073000, "BRAVO", 18.0f, 0.01f, 2, 50);
    db.update_entry(10142000, "ALFA", 25.0f, 0.0005f, 0, 50);
    
    assert(db.get_entry_count() == 3);
    
    // Get entries for channel
    auto channel_entries = db.get_entries_for_channel(7073000);
    assert(channel_entries.size() == 2);
    
    // Get entries for station
    auto station_entries = db.get_entries_for_station("ALFA");
    assert(station_entries.size() == 2);
    
    std::cout << "  PASS" << std::endl;
}

void test_score_computation() {
    std::cout << "Test: Score computation..." << std::endl;
    
    LQADatabase db;
    LQAConfig config;
    config.snr_weight = 0.5f;
    config.success_weight = 0.3f;
    config.recency_weight = 0.2f;
    db.set_config(config);
    
    // High SNR, low BER = high score
    db.update_entry(7073000, "REMOTE", 28.0f, 0.001f, 0, 100);
    
    auto entry = db.get_entry(7073000, "REMOTE");
    assert(entry != nullptr);
    assert(entry->score > 20.0f);  // Should be high quality
    assert(entry->score <= 31.0f); // Max score is 31
    
    std::cout << "  Score: " << entry->score << std::endl;
    std::cout << "  PASS" << std::endl;
}

void test_prune_stale_entries() {
    std::cout << "Test: Prune stale entries..." << std::endl;
    
    LQADatabase db;
    LQAConfig config;
    config.max_age_ms = 100;  // 100ms max age
    db.set_config(config);
    
    // Add entry
    db.update_entry(7073000, "REMOTE", 20.0f, 0.01f, 1, 50);
    assert(db.get_entry_count() == 1);
    
    // Wait for entry to become stale
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // Prune
    int removed = db.prune_stale_entries();
    assert(removed == 1);
    assert(db.get_entry_count() == 0);
    
    std::cout << "  PASS" << std::endl;
}

void test_save_and_load() {
    std::cout << "Test: Save and load database..." << std::endl;
    
    const std::string filepath = "test_lqa.db";
    
    // Create database with data
    {
        LQADatabase db;
        db.update_entry(7073000, "ALFA", 22.0f, 0.001f, 1, 50);
        db.update_entry(7073000, "BRAVO", 18.0f, 0.01f, 2, 50);
        db.update_entry(10142000, "CHARLIE", 25.0f, 0.0005f, 0, 50);
        
        assert(db.save_to_file(filepath));
    }
    
    // Load database
    {
        LQADatabase db;
        assert(db.load_from_file(filepath));
        
        assert(db.get_entry_count() == 3);
        
        auto entry = db.get_entry(7073000, "ALFA");
        assert(entry != nullptr);
        assert(entry->remote_station == "ALFA");
        assert(std::abs(entry->snr_db - 22.0f) < 0.1f);
    }
    
    // Clean up
    std::remove(filepath.c_str());
    
    std::cout << "  PASS" << std::endl;
}

void test_export_csv() {
    std::cout << "Test: Export to CSV..." << std::endl;
    
    const std::string filepath = "test_lqa.csv";
    
    LQADatabase db;
    db.update_entry(7073000, "ALFA", 22.0f, 0.001f, 1, 50);
    db.update_entry(10142000, "BRAVO", 18.0f, 0.01f, 2, 50);
    
    assert(db.export_to_csv(filepath));
    
    // Verify file exists and has content
    std::ifstream file(filepath);
    assert(file.is_open());
    
    std::string line;
    std::getline(file, line);  // Header
    assert(line.find("Frequency") != std::string::npos);
    
    std::getline(file, line);  // First entry
    assert(line.find("ALFA") != std::string::npos || line.find("BRAVO") != std::string::npos);
    
    file.close();
    
    // Clean up
    std::remove(filepath.c_str());
    
    std::cout << "  PASS" << std::endl;
}

void test_get_all_entries() {
    std::cout << "Test: Get all entries..." << std::endl;
    
    LQADatabase db;
    
    db.update_entry(7073000, "ALFA", 22.0f, 0.001f, 1, 50);
    db.update_entry(7073000, "BRAVO", 18.0f, 0.01f, 2, 50);
    db.update_entry(10142000, "CHARLIE", 25.0f, 0.0005f, 0, 50);
    
    auto all_entries = db.get_all_entries();
    assert(all_entries.size() == 3);
    
    // Verify all stations present
    bool found_alfa = false, found_bravo = false, found_charlie = false;
    for (const auto& entry : all_entries) {
        if (entry.remote_station == "ALFA") found_alfa = true;
        if (entry.remote_station == "BRAVO") found_bravo = true;
        if (entry.remote_station == "CHARLIE") found_charlie = true;
    }
    assert(found_alfa && found_bravo && found_charlie);
    
    std::cout << "  PASS" << std::endl;
}

void test_configuration() {
    std::cout << "Test: Configuration..." << std::endl;
    
    LQADatabase db;
    
    LQAConfig config;
    config.snr_weight = 0.6f;
    config.success_weight = 0.3f;
    config.recency_weight = 0.1f;
    config.max_age_ms = 600000;
    
    db.set_config(config);
    
    auto retrieved = db.get_config();
    assert(std::abs(retrieved.snr_weight - 0.6f) < 0.01f);
    assert(std::abs(retrieved.success_weight - 0.3f) < 0.01f);
    assert(std::abs(retrieved.recency_weight - 0.1f) < 0.01f);
    assert(retrieved.max_age_ms == 600000);
    
    std::cout << "  PASS" << std::endl;
}

int main() {
    std::cout << "=== LQA Database Tests ===" << std::endl;
    
    test_database_creation();
    test_basic_entry_update();
    test_time_weighted_averaging();
    test_extended_metrics();
    test_multiple_stations();
    test_score_computation();
    test_prune_stale_entries();
    test_save_and_load();
    test_export_csv();
    test_get_all_entries();
    test_configuration();
    
    std::cout << "\n=== All LQA Database Tests Passed ===" << std::endl;
    return 0;
}
