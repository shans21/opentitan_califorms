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

#define ARRAY_SIZE 32
#define ITER 10

OTTF_DEFINE_TEST_CONFIG();

static volatile uint32_t test_array[ARRAY_SIZE];

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

  // Byte access test
  uint32_t test_value = 0xAABBCCDD;
  uint8_t *byte_ptr = (uint8_t *)&test_value;
  CHECK(byte_ptr[0] == 0xDD, "Byte 0 access failed");
  CHECK(byte_ptr[1] == 0xCC, "Byte 1 access failed");
  CHECK(byte_ptr[2] == 0xBB, "Byte 2 access failed");
  CHECK(byte_ptr[3] == 0xAA, "Byte 3 access failed");

  // Timing test
  uint64_t start = ibex_mcycle_read();
  uint32_t sum = 0;
  for (int i = 0; i < ITER; ++i) {
    for (int j = 0; j < ARRAY_SIZE; ++j) {
      sum += test_array[j];
    }
  }
  uint64_t end = ibex_mcycle_read();

  uint64_t cycles = end - start;
  
  // Split the 64-bit cycle count into two 32-bit values for logging
  uint32_t cycles_high = (uint32_t)(cycles >> 32);
  uint32_t cycles_low = (uint32_t)(cycles & 0xFFFFFFFF);

  LOG_INFO("Access time: %u%08u cycles", cycles_high, cycles_low);
  LOG_INFO("Sum: %u", sum);

  return true;
}

