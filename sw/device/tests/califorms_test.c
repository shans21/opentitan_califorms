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

static volatile uint32_t test_array[1024];

// BLOC instruction implementation (this would be provided by the hardware team)
static inline void bloc(uint32_t* addr, uint32_t mask, uint32_t set) {
  asm volatile("bloc %0, %1, %2" : : "r"(addr), "r"(mask), "r"(set) : "memory");
}

bool test_main(void) {
  dif_uart_t uart;
  CHECK_DIF_OK(dif_uart_init(
      mmio_region_from_addr(TOP_EARLGREY_UART0_BASE_ADDR), &uart));
  CHECK_DIF_OK(dif_uart_configure(
      &uart, (dif_uart_config_t){
                 .baudrate = (uint32_t) kUartBaudrate,
                 .clk_freq_hz = (uint32_t) kClockFreqPeripheralHz,
                 .parity_enable = kDifToggleDisabled,
                 .parity = kDifUartParityEven,
                 .tx_enable = kDifToggleEnabled,
                 .rx_enable = kDifToggleEnabled,
             }));

  // Byte-granular protection test
  uint32_t test_value = 0;
  uint32_t* test_ptr = &test_value;
  
  bloc(test_ptr, 0xFF00FF00, 0xFF00FF00);  // Protect bytes 1 and 3
  
  *test_ptr = 0xAABBCCDD;
  
  CHECK(*test_ptr == 0xAA00CC00, "Byte-granular protection failed");

  // Timing test
  uint64_t start = ibex_mcycle_read();
  uint32_t sum = 0;
  for (int i = 0; i < 1000; ++i) {
    for (int j = 0; j < 1024; ++j) {
      sum += test_array[j];
    }
  }
  uint64_t cycles_upt = ibex_mcycle_read() - start;

  // Set security bytes for all elements
  for (int i = 0; i < 1024; ++i) {
    bloc((uint32_t *)&test_array[i], 0xFF00FF00, 0xFF00FF00);
  }

  start = ibex_mcycle_read();
  sum = 0;
  for (int i = 0; i < 1000; ++i) {
    for (int j = 0; j < 1024; ++j) {
      sum += test_array[j];
    }
  }
  uint64_t cycles_pt = ibex_mcycle_read() - start;

  // Split the 64-bit cycle count into two 32-bit values for logging
  uint32_t cycles_high_upt = (uint32_t)(cycles_upt >> 32);
  uint32_t cycles_low_upt = (uint32_t)(cycles_upt & 0xFFFFFFFF);

  // Split the 64-bit cycle count into two 32-bit values for logging
  uint32_t cycles_high_pt = (uint32_t)(cycles_pt >> 32);
  uint32_t cycles_low_pt = (uint32_t)(cycles_pt & 0xFFFFFFFF);

  LOG_INFO("Unprotected access time: %u%08u cycles", cycles_high_upt, cycles_low_upt);
  LOG_INFO("Protected access time: %u%08u cycles", cycles_high_pt, cycles_low_pt);
  // LOG_INFO("Performance overhead: %.2f%%", (float)(protected_time - unprotected_time) / unprotected_time * 100);
  LOG_INFO("Sum: %u", sum);

  return true;
}

