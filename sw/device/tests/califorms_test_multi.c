// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "sw/device/lib/arch/device.h"
#include "sw/device/lib/base/mmio.h"
#include "sw/device/lib/dif/dif_uart.h"
#include "sw/device/lib/runtime/hart.h"
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

OTTF_DEFINE_TEST_CONFIG();

#define ARRAY_SIZES 3
#define NUM_RUNS 5

static volatile uint32_t test_array[4096];  // Increased size to accommodate largest test

// BLOC instruction implementation (this would be provided by the hardware team)
static inline void bloc(uint32_t* addr, uint32_t mask, uint32_t set) {
  asm volatile("bloc %0, %1, %2" : : "r"(addr), "r"(mask), "r"(set) : "memory");
}

static void initialize_array(size_t size) {
  for (size_t i = 0; i < size; ++i) {
    test_array[i] = i & 0xFF;  // Simple pattern: 0, 1, 2, ..., 255, 0, 1, 2, ...
  }
}

static void apply_califorms(size_t size) {
  for (size_t i = 0; i < size; i += 2) {
    bloc(&test_array[i], 0xFFFFFFFF, 0xFFFF0000);  // Protect upper half of each word
  }
}

static uint64_t run_timing_test(size_t size, uint32_t iterations) {
  uint64_t start = ibex_mcycle_read();
  uint32_t sum = 0;
  for (uint32_t i = 0; i < iterations; ++i) {
    for (size_t j = 0; j < size; ++j) {
      sum += test_array[j];
    }
  }
  uint64_t end = ibex_mcycle_read();
  LOG_INFO("Sum for size %u: %u", size, sum);
  return end - start;
}

bool test_main(void) {
  dif_uart_t uart;
  CHECK_DIF_OK(dif_uart_init(
      mmio_region_from_addr(TOP_EARLGREY_UART0_BASE_ADDR), &uart));
  CHECK_DIF_OK(dif_uart_configure(
      &uart, (dif_uart_config_t){
                 .baudrate = (uint32_t)kUartBaudrate,
                 .clk_freq_hz = (uint32_t)kClockFreqPeripheralHz,
                 .parity_enable = kDifToggleDisabled,
                 .parity = kDifUartParityEven,
                 .tx_enable = kDifToggleEnabled,
                 .rx_enable = kDifToggleEnabled,
             }));

  // Test different array sizes
  size_t sizes[ARRAY_SIZES] = {256, 1024, 4096};
  uint32_t iterations[ARRAY_SIZES] = {1000, 250, 62};  // Adjusted to keep total operations similar

  for (int s = 0; s < ARRAY_SIZES; ++s) {
    size_t size = sizes[s];
    uint64_t total_cycles = 0;

    LOG_INFO("Testing array size: %u", size);

    for (int run = 0; run < NUM_RUNS; ++run) {
      initialize_array(size);
      apply_califorms(size);
      uint64_t cycles = run_timing_test(size, iterations[s]);
      total_cycles += cycles;
      LOG_INFO("Run %d: %u cycles", run + 1, (uint32_t)cycles);
    }

    uint64_t avg_cycles = total_cycles / NUM_RUNS;
    LOG_INFO("Average cycles for size %u: %u", size, (uint32_t)avg_cycles);
  }

  return true;
}

