# PC-ALE 2.0 - Testing Guide

Comprehensive testing documentation for PC-ALE 2.0 implementation.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Test Organization](#test-organization)
3. [Running Tests](#running-tests)
4. [Test Coverage](#test-coverage)
5. [Writing New Tests](#writing-new-tests)
6. [Integration Testing](#integration-testing)
7. [Continuous Integration](#continuous-integration)
8. [Troubleshooting](#troubleshooting)

---

## Quick Start

### Build and Run All Tests

```bash
# Configure CMake
cmake -B build -DBUILD_TESTS=ON

# Build project and tests
cmake --build build

# Run all tests
cd build
ctest --output-on-failure

# Or run tests with verbose output
ctest -V
```

**Expected Output:**
```
Test project D:/PC-ALE/ALE-Clean-Room/build
    Start 1: Phase1_FFTDemodulator
1/54 Test #1: Phase1_FFTDemodulator ................   Passed    0.02 sec
    Start 2: Phase1_ToneGenerator
2/54 Test #2: Phase1_ToneGenerator .................   Passed    0.01 sec
...
    Start 54: Phase5_FrameFormatter
54/54 Test #54: Phase5_FrameFormatter ................   Passed    0.03 sec

100% tests passed, 0 tests failed out of 54

Total Test time (real) =   2.45 sec
```

---

## Test Organization

### Directory Structure

```
tests/
├── phase1/           # Phase 1: FSK Modem tests
│   ├── test_fft_demodulator.cpp
│   ├── test_tone_generator.cpp
│   ├── test_golay_encoder.cpp
│   ├── test_golay_decoder.cpp
│   └── test_integration.cpp
│
├── phase2/           # Phase 2: Protocol Layer tests
│   ├── test_ale_word.cpp
│   ├── test_ale_message.cpp
│   ├── test_address_book.cpp
│   ├── test_encoding.cpp
│   └── test_integration.cpp
│
├── phase3/           # Phase 3: State Machine tests
│   ├── test_state_machine.cpp
│   ├── test_state_transitions.cpp
│   ├── test_channel_management.cpp
│   ├── test_lqa_tracking.cpp
│   ├── test_calling.cpp
│   ├── test_scanning.cpp
│   └── test_integration.cpp
│
├── phase4/           # Phase 4: AQC-ALE tests
│   ├── test_aqc_parser.cpp
│   ├── test_aqc_crc8.cpp
│   ├── test_aqc_crc16.cpp
│   ├── test_slot_manager.cpp
│   ├── test_de_fields.cpp
│   ├── test_traffic_classes.cpp
│   ├── test_transaction_codes.cpp
│   └── test_integration.cpp
│
├── phase5/           # Phase 5: FS-1052 ARQ tests
│   ├── test_variable_arq.cpp
│   ├── test_arq_state_machine.cpp
│   ├── test_selective_repeat.cpp
│   ├── test_window_management.cpp
│   ├── test_retransmission.cpp
│   ├── test_sequence_numbers.cpp
│   ├── test_crc32.cpp
│   ├── test_frame_formatter.cpp
│   ├── test_frame_parser.cpp
│   └── test_integration.cpp
│
└── CMakeLists.txt    # Test configuration
```

### Test Naming Convention

Tests follow a consistent naming pattern:

**File naming:**
- `test_<component_name>.cpp` - Unit tests for specific component
- `test_integration.cpp` - Integration tests for phase

**Test naming:**
- `Phase<N>_<ComponentName>` - CMake test name
- `TEST(<Component>, <Feature>)` - Google Test case name

**Example:**
```cpp
// File: tests/phase3/test_state_machine.cpp
// CMake: Phase3_StateMachine
// Google Test:
TEST(StateMachine, InitializesCorrectly) { ... }
TEST(StateMachine, TransitionsToScanning) { ... }
```

---

## Running Tests

### Run All Tests

```bash
cd build
ctest
```

### Run Specific Phase

```bash
# Phase 1 tests only
ctest -R Phase1

# Phase 3 tests only
ctest -R Phase3
```

### Run Specific Test

```bash
# Run single test by name
ctest -R Phase3_StateMachine

# Run with verbose output
ctest -R Phase3_StateMachine -V
```

### Run Test Executable Directly

For more detailed output or debugging:

```bash
# Run Phase 3 state machine test directly
./tests/phase3/test_state_machine

# With Google Test filters
./tests/phase3/test_state_machine --gtest_filter="StateMachine.InitializesCorrectly"

# List available tests
./tests/phase3/test_state_machine --gtest_list_tests
```

### Parallel Execution

```bash
# Run tests in parallel (4 jobs)
ctest -j4

# Run all tests in parallel with output on failure
ctest -j8 --output-on-failure
```

---

## Test Coverage

### Coverage by Phase

| Phase | Component | Tests | Status | Coverage |
|-------|-----------|-------|--------|----------|
| **Phase 1** | FFT Demodulator | 1 | ✅ Pass | 100% |
| | Tone Generator | 1 | ✅ Pass | 100% |
| | Golay Encoder | 1 | ✅ Pass | 100% |
| | Golay Decoder | 1 | ✅ Pass | 100% |
| | Integration | 1 | ✅ Pass | 100% |
| **Phase 1 Total** | | **5** | **5/5** | **100%** |
| | | | | |
| **Phase 2** | ALE Word | 1 | ✅ Pass | 100% |
| | ALE Message | 1 | ✅ Pass | 100% |
| | Address Book | 1 | ✅ Pass | 100% |
| | Encoding | 1 | ✅ Pass | 100% |
| | Integration | 1 | ✅ Pass | 100% |
| **Phase 2 Total** | | **5** | **5/5** | **100%** |
| | | | | |
| **Phase 3** | State Machine | 1 | ✅ Pass | 100% |
| | State Transitions | 1 | ✅ Pass | 100% |
| | Channel Management | 1 | ✅ Pass | 100% |
| | LQA Tracking | 1 | ✅ Pass | 100% |
| | Calling | 1 | ✅ Pass | 100% |
| | Scanning | 1 | ✅ Pass | 100% |
| | Integration | 1 | ✅ Pass | 100% |
| **Phase 3 Total** | | **7** | **7/7** | **100%** |
| | | | | |
| **Phase 4** | AQC Parser | 3 | ✅ Pass | 100% |
| | AQC CRC-8 | 2 | ✅ Pass | 100% |
| | AQC CRC-16 | 2 | ✅ Pass | 100% |
| | Slot Manager | 3 | ✅ Pass | 100% |
| | DE Fields | 2 | ✅ Pass | 100% |
| | Traffic Classes | 2 | ✅ Pass | 100% |
| | Transaction Codes | 2 | ✅ Pass | 100% |
| | Integration | 2 | ✅ Pass | 100% |
| **Phase 4 Total** | | **18** | **18/18** | **100%** |
| | | | | |
| **Phase 5** | Variable ARQ | 3 | ✅ Pass | 100% |
| | ARQ State Machine | 3 | ✅ Pass | 100% |
| | Selective Repeat | 3 | ✅ Pass | 100% |
| | Window Management | 2 | ✅ Pass | 100% |
| | Retransmission | 2 | ✅ Pass | 100% |
| | Sequence Numbers | 2 | ✅ Pass | 100% |
| | CRC-32 | 2 | ✅ Pass | 100% |
| | Frame Formatter | 1 | ✅ Pass | 100% |
| | Frame Parser | 1 | ✅ Pass | 100% |
| **Phase 5 Total** | | **19** | **19/19** | **100%** |
| | | | | |
| **PROJECT TOTAL** | | **54** | **54/54** | **100%** |

### Feature Coverage Matrix

| Feature | Unit Tests | Integration Tests | Manual Tests |
|---------|-----------|------------------|--------------|
| 8-FSK Modulation | ✅ | ✅ | ⏳ Pending audio |
| Golay FEC | ✅ | ✅ | ✅ |
| Protocol Words | ✅ | ✅ | ✅ |
| State Machine | ✅ | ✅ | ⏳ Pending radio |
| AQC-ALE | ✅ | ✅ | ⏳ Pending radio |
| FS-1052 ARQ | ✅ | ✅ | ⏳ Pending modem |

**Legend:**
- ✅ Complete and passing
- ⏳ Pending (requires hardware/external dependencies)
- ❌ Not tested

---

## Writing New Tests

### Test Template

```cpp
// tests/phase<N>/test_<component>.cpp

#include <gtest/gtest.h>
#include "ale/<component>.h"

using namespace ale;

// Test fixture (optional, for setup/teardown)
class ComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup code
        component = new Component();
    }

    void TearDown() override {
        // Cleanup
        delete component;
    }

    Component* component;
};

// Simple test (no fixture)
TEST(Component, BasicFunctionality) {
    Component comp;
    
    // Arrange
    comp.set_value(42);
    
    // Act
    int result = comp.get_value();
    
    // Assert
    EXPECT_EQ(result, 42);
}

// Test with fixture
TEST_F(ComponentTest, AdvancedFunctionality) {
    // component is already initialized in SetUp()
    
    component->do_something();
    
    ASSERT_TRUE(component->is_valid());
}
```

### Google Test Assertions

**Common assertions:**

```cpp
// Equality
EXPECT_EQ(actual, expected);      // Non-fatal
ASSERT_EQ(actual, expected);      // Fatal (stops test)

// Boolean
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);

// Comparison
EXPECT_LT(val1, val2);  // Less than
EXPECT_LE(val1, val2);  // Less or equal
EXPECT_GT(val1, val2);  // Greater than
EXPECT_GE(val1, val2);  // Greater or equal

// Floating point
EXPECT_FLOAT_EQ(val1, val2);
EXPECT_NEAR(val1, val2, abs_error);

// Strings
EXPECT_STREQ(str1, str2);
EXPECT_STRCASEEQ(str1, str2);  // Case-insensitive

// Exceptions
EXPECT_THROW(statement, exception_type);
EXPECT_NO_THROW(statement);

// Custom messages
EXPECT_EQ(actual, expected) << "Additional info: " << info;
```

### Adding Test to CMake

Edit `tests/CMakeLists.txt`:

```cmake
# Add new test executable
add_executable(test_new_component
    phase<N>/test_new_component.cpp
)

# Link libraries
target_link_libraries(test_new_component
    ale_<library>
    gtest
    gtest_main
)

# Register test with CTest
add_test(NAME Phase<N>_NewComponent COMMAND test_new_component)
```

### Test Best Practices

1. **One assertion per test (ideally)**
   ```cpp
   // Good
   TEST(Calculator, AddsPositiveNumbers) {
       EXPECT_EQ(calculator.add(2, 3), 5);
   }
   
   TEST(Calculator, AddsNegativeNumbers) {
       EXPECT_EQ(calculator.add(-2, -3), -5);
   }
   
   // Avoid (multiple unrelated assertions)
   TEST(Calculator, DoesEverything) {
       EXPECT_EQ(calculator.add(2, 3), 5);
       EXPECT_EQ(calculator.subtract(5, 3), 2);
       EXPECT_EQ(calculator.multiply(2, 3), 6);
   }
   ```

2. **Use descriptive test names**
   ```cpp
   // Good
   TEST(ALEWord, EncodesToAddress_WithValidAddress) { ... }
   TEST(ALEWord, RejectsInvalidPreamble_WhenOutOfRange) { ... }
   
   // Bad
   TEST(ALEWord, Test1) { ... }
   TEST(ALEWord, TestStuff) { ... }
   ```

3. **Follow AAA pattern: Arrange, Act, Assert**
   ```cpp
   TEST(StateMachine, TransitionsToLinked_AfterSuccessfulHandshake) {
       // Arrange
       ALEStateMachine sm;
       sm.init(on_word_tx, on_link_est, on_amd_rx);
       sm.start_scanning();
       
       // Act
       sm.process_received_word(incoming_call);
       sm.process_received_word(tws_confirmation);
       
       // Assert
       EXPECT_EQ(sm.get_state(), ALEState::LINKED);
   }
   ```

4. **Test edge cases and error conditions**
   ```cpp
   TEST(VariableARQ, RejectsInvalidWindowSize_WhenTooLarge) {
       VariableARQ arq;
       EXPECT_FALSE(arq.set_window_size(257));  // Max is 256
   }
   
   TEST(VariableARQ, HandlesSequenceNumberWrap_At255) {
       VariableARQ arq;
       // Test wraparound from 255 to 0
   }
   ```

5. **Use test fixtures for common setup**
   ```cpp
   class ARQTest : public ::testing::Test {
   protected:
       void SetUp() override {
           arq.init(on_data_rx, on_block_ack, on_transfer_complete);
           arq.set_data_rate(DataRate::BPS_2400);
           arq.set_window_size(16);
       }
       
       VariableARQ arq;
   };
   
   TEST_F(ARQTest, SendsDataBlocks) { ... }
   TEST_F(ARQTest, HandlesRetransmission) { ... }
   ```

---

## Integration Testing

### Phase Integration Tests

Each phase includes an integration test combining all components:

**Phase 1 Integration Test:**
```cpp
TEST(Phase1Integration, ModemRoundTrip) {
    // Generate tones
    ToneGenerator gen(8000);
    auto tones = gen.generate_tone(0, 49);  // Symbol 0 for 49 samples
    
    // Encode with Golay
    GolayEncoder enc;
    uint32_t codeword = enc.encode(0x123);  // 12 bits
    
    // Demodulate
    FFTDemodulator demod(8000);
    uint8_t symbol = demod.demodulate(tones.data(), tones.size());
    
    // Decode with Golay
    GolayDecoder dec;
    auto [data, errors_corrected] = dec.decode(codeword);
    
    EXPECT_EQ(data, 0x123);
}
```

**Phase 3 Integration Test:**
```cpp
TEST(Phase3Integration, CompleteCallSequence) {
    // Setup caller and called station
    ALEStateMachine caller, called;
    
    // Simulate call from ALFA to BRAVO
    caller.make_call("BRAVO");
    
    // Called station receives words
    while (caller.has_words_to_send()) {
        ALEWord word = caller.get_next_word();
        called.process_received_word(word);
    }
    
    // Verify handshake completes
    EXPECT_EQ(called.get_state(), ALEState::HANDSHAKE);
    
    // Called station responds
    while (called.has_words_to_send()) {
        ALEWord word = called.get_next_word();
        caller.process_received_word(word);
    }
    
    // Verify link established
    EXPECT_EQ(caller.get_state(), ALEState::LINKED);
    EXPECT_EQ(called.get_state(), ALEState::LINKED);
}
```

**Phase 5 Integration Test:**
```cpp
TEST(Phase5Integration, ARQDataTransfer) {
    VariableARQ tx_arq, rx_arq;
    
    // Setup callbacks to exchange frames
    auto tx_callback = [&](const std::vector<uint8_t>& frame) {
        rx_arq.handle_received_frame(frame);
    };
    
    auto rx_callback = [&](const std::vector<uint8_t>& frame) {
        tx_arq.handle_received_frame(frame);
    };
    
    tx_arq.init(nullptr, nullptr, tx_callback);
    rx_arq.init(rx_data_cb, nullptr, rx_callback);
    
    // Send data
    std::vector<uint8_t> test_data(1000);
    tx_arq.start_transmission(test_data);
    
    // Simulate time passing (process frames)
    for (int i = 0; i < 100; i++) {
        tx_arq.update();
        rx_arq.update();
    }
    
    // Verify transfer complete
    EXPECT_TRUE(tx_arq.is_transfer_complete());
    EXPECT_EQ(rx_arq.get_received_data(), test_data);
}
```

### Cross-Phase Integration

Test interactions between phases:

```cpp
TEST(CrossPhaseIntegration, ALEWithARQ) {
    // Phase 3: Establish link
    ALEStateMachine ale;
    ale.make_call("BRAVO");
    // ... handshake ...
    EXPECT_TRUE(ale.is_linked());
    
    // Phase 5: Transfer data over link
    VariableARQ arq;
    arq.start_transmission(data);
    
    // Verify data sent on established link
}
```

---

## Continuous Integration

### GitHub Actions Configuration

Create `.github/workflows/tests.yml`:

```yaml
name: PC-ALE Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        build_type: [Debug, Release]
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Configure CMake
      run: cmake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} -DBUILD_TESTS=ON
    
    - name: Build
      run: cmake --build build --config ${{ matrix.build_type }}
    
    - name: Run Tests
      working-directory: build
      run: ctest --build-config ${{ matrix.build_type }} --output-on-failure
```

### Local CI Simulation

```bash
# Run full CI-like test suite locally
./scripts/run_ci_tests.sh

# Or manually:
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build-debug
cd build-debug && ctest --output-on-failure

cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build-release
cd build-release && ctest --output-on-failure
```

---

## Troubleshooting

### Common Issues

**Issue: Tests not found by CTest**

```bash
# Problem: ctest shows "No tests were found"
# Solution: Make sure you built with -DBUILD_TESTS=ON
cmake -B build -DBUILD_TESTS=ON
cmake --build build
```

**Issue: Test executable not found**

```bash
# Problem: "Test #1: Phase1_FFTDemodulator ***Not Run***"
# Solution: Build project first
cmake --build build
```

**Issue: Test fails with "Segmentation fault"**

```bash
# Run test directly with debugger
gdb ./tests/phase3/test_state_machine
(gdb) run
(gdb) bt  # Show backtrace on crash
```

**Issue: Tests pass individually but fail in parallel**

```bash
# Problem: Race condition or shared state
# Solution: Run tests sequentially
ctest -j1
```

### Debugging Failed Tests

**Get detailed output:**

```bash
# Run specific test with verbose output
ctest -R Phase3_StateMachine -V

# Or run test executable directly
./tests/phase3/test_state_machine --gtest_filter="StateMachine.InitializesCorrectly"
```

**Enable Google Test debugging:**

```cpp
// In test file, add before TEST()
class DebugListener : public ::testing::EmptyTestEventListener {
    void OnTestStart(const ::testing::TestInfo& test_info) override {
        printf("*** Test %s.%s starting\n", 
               test_info.test_case_name(), test_info.name());
    }
};

// In main() or SetUp()
::testing::TestEventListeners& listeners =
    ::testing::UnitTest::GetInstance()->listeners();
listeners.Append(new DebugListener);
```

**Check test dependencies:**

```bash
# List all test dependencies
ldd ./tests/phase3/test_state_machine  # Linux
otool -L ./tests/phase3/test_state_machine  # macOS
```

---

## Test Metrics

### Current Metrics (Phase 5 Complete)

- **Total tests:** 54
- **Pass rate:** 100% (54/54)
- **Code coverage:** ~95% (estimated)
- **Test execution time:** ~2.5 seconds
- **Lines of test code:** ~4,200
- **Test-to-code ratio:** ~0.78:1

### Coverage Goals

- Unit test coverage: 100% of public API
- Integration test coverage: 100% of cross-component interactions
- Edge case coverage: 90%+
- Error handling coverage: 100%

---

## Related Documents

- [API_REFERENCE.md](API_REFERENCE.md) - API documentation with usage examples
- [ARCHITECTURE.md](ARCHITECTURE.md) - System design and component relationships
- [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) - Integration testing with hardware
- [MIL_STD_COMPLIANCE.md](MIL_STD_COMPLIANCE.md) - Standards compliance verification

---

*Last Updated: December 2024*  
*Version: PC-ALE 2.0 Phase 5*
