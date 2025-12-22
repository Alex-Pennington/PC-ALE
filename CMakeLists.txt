cmake_minimum_required(VERSION 3.15)
project(ALE-Clean-Room VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -fPIC")

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Detect platform
if(WIN32)
    set(PLATFORM "windows")
elseif(APPLE)
    set(PLATFORM "macos")
else()
    set(PLATFORM "linux")
endif()

message(STATUS "Building for ${PLATFORM}")

# Core library
add_library(ale_fsk_core
    src/fsk/fft_demodulator.cpp
    src/fsk/tone_generator.cpp
    src/fsk/symbol_decoder.cpp
    src/core/types.cpp
)

target_include_directories(ale_fsk_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# FEC library
add_library(ale_fec
    src/fec/golay.cpp
)

target_include_directories(ale_fec PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Protocol library (Phase 2)
add_library(ale_protocol
    src/protocol/ale_word.cpp
    src/protocol/ale_message.cpp
)

target_include_directories(ale_protocol PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ale_protocol ale_fsk_core ale_fec)

# AQC Protocol Extensions (Phase 4)
add_library(ale_aqc
    src/protocol/aqc_parser.cpp
)

target_include_directories(ale_aqc PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ale_aqc ale_protocol ale_fsk_core ale_fec)

# Link library (Phase 3)
add_library(ale_link
    src/link/ale_state_machine.cpp
)

target_include_directories(ale_link PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ale_link ale_protocol ale_fsk_core ale_fec)

# FS-1052 ARQ Protocol (Phase 5)
add_library(ale_fs1052
    src/fs1052/frame_format.cpp
    src/fs1052/fs1052_arq.cpp
)

target_include_directories(ale_fs1052 PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ale_fs1052 ale_protocol ale_fsk_core ale_fec)

# LQA System (Phase 6)
add_library(ale_lqa
    src/lqa_database.cpp
    src/lqa_metrics.cpp
    src/lqa_analyzer.cpp
)

target_include_directories(ale_lqa PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ale_lqa ale_protocol ale_fsk_core ale_fec)

# Tests
enable_testing()

add_executable(test_fsk_core
    tests/test_fsk_core.cpp
)
target_link_libraries(test_fsk_core ale_fsk_core ale_fec)
target_include_directories(test_fsk_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME FSKCore COMMAND test_fsk_core)

add_executable(test_protocol
    tests/test_protocol.cpp
)
target_link_libraries(test_protocol ale_protocol ale_fsk_core ale_fec)
target_include_directories(test_protocol PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME Protocol COMMAND test_protocol)

add_executable(test_state_machine
    tests/test_state_machine.cpp
)
target_link_libraries(test_state_machine ale_link ale_protocol ale_fsk_core ale_fec)
target_include_directories(test_state_machine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME StateMachine COMMAND test_state_machine)

# Phase 4: AQC-ALE tests
add_executable(test_aqc_parser
    tests/test_aqc_parser.cpp
)
target_link_libraries(test_aqc_parser ale_aqc ale_protocol ale_fsk_core ale_fec)
target_include_directories(test_aqc_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME AQCParser COMMAND test_aqc_parser)

add_executable(test_aqc_crc
    tests/test_aqc_crc.cpp
)
target_link_libraries(test_aqc_crc ale_aqc ale_protocol ale_fsk_core ale_fec)
target_include_directories(test_aqc_crc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME AQCCRC COMMAND test_aqc_crc)

# Phase 5: FS-1052 ARQ tests
add_executable(test_fs1052_frames
    tests/test_fs1052_frames.cpp
)
target_link_libraries(test_fs1052_frames ale_fs1052 ale_protocol ale_fsk_core ale_fec)
target_include_directories(test_fs1052_frames PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME FS1052Frames COMMAND test_fs1052_frames)

add_executable(test_fs1052_arq
    tests/test_fs1052_arq.cpp
)
target_link_libraries(test_fs1052_arq ale_fs1052 ale_protocol ale_fsk_core ale_fec)
target_include_directories(test_fs1052_arq PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME FS1052ARQ COMMAND test_fs1052_arq)

# Phase 6: LQA System tests
add_executable(test_lqa_database
    tests/test_lqa_database.cpp
)
target_link_libraries(test_lqa_database ale_lqa ale_protocol ale_fsk_core ale_fec)
target_include_directories(test_lqa_database PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME LQADatabase COMMAND test_lqa_database)

add_executable(test_lqa_metrics
    tests/test_lqa_metrics.cpp
)
target_link_libraries(test_lqa_metrics ale_lqa ale_protocol ale_fsk_core ale_fec)
target_include_directories(test_lqa_metrics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME LQAMetrics COMMAND test_lqa_metrics)

add_executable(test_lqa_analyzer
    tests/test_lqa_analyzer.cpp
)
target_link_libraries(test_lqa_analyzer ale_lqa ale_protocol ale_fsk_core ale_fec)
target_include_directories(test_lqa_analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME LQAAnalyzer COMMAND test_lqa_analyzer)

add_executable(debug_fft
    tests/debug_fft.cpp
)
target_link_libraries(debug_fft ale_fsk_core ale_fec)
target_include_directories(debug_fft PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Optional: Build examples
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_executable(example_ale_decoder examples/example_ale_decoder.cpp)
    target_link_libraries(example_ale_decoder ale_protocol ale_fsk_core ale_fec)
    target_include_directories(example_ale_decoder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    
    add_executable(example_complete_stack examples/example_complete_stack.cpp)
    target_link_libraries(example_complete_stack ale_link ale_protocol ale_fsk_core ale_fec)
    target_include_directories(example_complete_stack PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    
    add_executable(example_aqc_usage examples/example_aqc_usage.cpp)
    target_link_libraries(example_aqc_usage ale_aqc ale_protocol ale_fsk_core ale_fec)
    target_include_directories(example_aqc_usage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    
    add_executable(example_fs1052_transfer examples/example_fs1052_transfer.cpp)
    target_link_libraries(example_fs1052_transfer ale_fs1052 ale_protocol ale_fsk_core ale_fec)
    target_include_directories(example_fs1052_transfer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
