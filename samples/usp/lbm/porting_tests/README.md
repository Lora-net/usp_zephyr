# LoRaWAN Porting Tests

This application provides **comprehensive Hardware Abstraction Layer (HAL) testing** for LoRa Basics Modem (LBM) integration. It validates critical system functions required for proper modem operation, including SPI communication, timing, interrupts, and low-power functionality.

## Key Features

- **SPI Communication Testing**: Verifies radio transceiver communication via SPI interface
- **Radio Interrupt Validation**: Tests radio IRQ handling and callback functionality
- **Timing System Tests**: Validates time measurement functions (seconds and milliseconds)
- **Timer Interrupt Testing**: Verifies low-power timer operation and IRQ callbacks
- **Random Number Generation**: Tests hardware random number generation functionality
- **Radio Configuration Tests**: Validates RX/TX radio setup and timing performance
- **Sleep Mode Testing**: Verifies low-power sleep functionality and wake-up timing
- **Flash Storage Tests**: Optional non-volatile memory read/write validation

## Configuration

### Test Modes

| Parameter                                         | Default | Description                                                     |
|---------------------------------------------------|---------|-----------------------------------------------------------------|
| `ENABLE_TEST_FLASH`                               | `n`     | Enable flash tests, disable others                              |
| `NB_LOOP_TEST_SPI`                                | `2`     | Number of SPI test iterations                                   |
| `NB_LOOP_TEST_CONFIG_RADIO`                       | `2`     | Number of radio config test loops                               |
| `LR20XX_PORTING_TEST_SPI_REGMEM_ENDURANCE`        | `n`     | Enable SPI endurance test                                       |
| `LR20XX_PORTING_TEST_SPI_REGMEM_NB_ITERATIONS`    | `10000` | Number of iterations for SPI endurance                          |
| `LR20XX_PORTING_TEST_SPI_REGMEM_SIZE_BYTES`       | `1024`  | Number of bytes per iteration for SPI endurance (multiple of 4) |

## Compilation

**Build:**
```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/lbm/porting_tests

```build with LR20XX_SPI_ENDURANCE
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/lbm/porting_tests -- -DEXTRA_CFLAGS="-DLR20XX_PORTING_TEST_SPI_REGMEM_ENDURANCE=1"
```

**Flash the firmware:**
```bash
west flash --runner pyocd
```

## Usage

1. **Build and Flash**: Compile and flash the application to target hardware
2. **Monitor Output**: Connect to UART/RTT console to view test results
3. **Automatic Execution**: Tests run automatically on startup and report pass/fail status
4. **Flash Tests** (if enabled): Requires MCU reset and relaunch to verify persistent storage

## Expected Output

### Standard Test Sequence
```
*** Booting Zephyr OS build vx.y.z ***
[00:50:27.826,323] <inf> porting_tests: 
[00:50:27.826,326] <inf> porting_tests: 
[00:50:27.826,330] <inf> porting_tests: PORTING_TESTS example is starting
[00:50:27.826,333] <inf> porting_tests: 
[00:50:27.826,337] <inf> porting_tests: 
[00:50:27.826,359] <inf> porting_tests: ---------------------------------------- porting_test_spi :
[00:50:27.833,471] <inf> porting_tests:  OK 
[00:50:27.833,494] <inf> porting_tests: ---------------------------------------- porting_test_spi_regmem_endurance :
[00:50:27.837,676] <inf> porting_tests:  regmem endurance: iteration 0 
[00:50:32.157,248] <inf> porting_tests:  regmem endurance: iteration 1000 
[00:50:36.477,649] <inf> porting_tests:  regmem endurance: iteration 2000 
[00:50:40.799,041] <inf> porting_tests:  regmem endurance: iteration 3000 
[00:50:45.120,391] <inf> porting_tests:  regmem endurance: iteration 4000 
[00:50:49.441,351] <inf> porting_tests:  regmem endurance: iteration 5000 
[00:50:53.761,601] <inf> porting_tests:  regmem endurance: iteration 6000 
[00:50:58.082,391] <inf> porting_tests:  regmem endurance: iteration 7000 
[00:51:02.402,862] <inf> porting_tests:  regmem endurance: iteration 8000 
[00:51:06.724,911] <inf> porting_tests:  regmem endurance: iteration 9000 
[00:51:11.043,424] <inf> porting_tests:  regmem endurance: iteration 9999 
[00:51:11.043,432] <inf> porting_tests:  OK 
[00:51:11.043,454] <inf> porting_tests: ---------------------------------------- porting_test_radio_irq :
[00:51:12.098,836] <inf> porting_tests:  OK 
[00:51:12.098,863] <inf> porting_tests: ---------------------------------------- porting_test_get_time :
[00:51:12.098,867] <inf> porting_tests:  * Get time in second: 
[00:51:17.143,014] <inf> porting_tests:  OK 
[00:51:17.143,020] <inf> porting_tests:  Time expected 5s / get 5s (no margin)
[00:51:17.143,024] <inf> porting_tests:  * Get time in millisecond: 
[00:51:19.172,671] <inf> porting_tests:  OK 
[00:51:19.172,681] <inf> porting_tests:  Time expected 1966ms / get 1966ms (margin +/-1ms)
[00:51:19.172,702] <inf> porting_tests: ---------------------------------------- porting_test_timer_irq :
[00:51:22.178,815] <inf> porting_tests:  OK 
[00:51:22.178,828] <inf> porting_tests:  Timer irq configured with 3000ms / get 3000ms (margin +2ms)
[00:51:22.178,847] <inf> porting_tests: ---------------------------------------- porting_test_stop_timer :
[00:51:24.178,438] <inf> porting_tests:  OK 
[00:51:24.178,458] <inf> porting_tests: ---------------------------------------- porting_test_disable_enable_irq :
[00:51:28.178,455] <inf> porting_tests:  OK 
[00:51:28.178,475] <inf> porting_tests: ---------------------------------------- porting_test_random :
[00:51:28.178,479] <inf> porting_tests:  * Get random nb : 
[00:51:28.178,543] <inf> porting_tests:  OK 
[00:51:28.178,547] <inf> porting_tests:  random1 = 4201829233, random2 = 1598311636
[00:51:28.178,554] <inf> porting_tests:  * Get random nb in range : 
[00:51:28.178,614] <inf> porting_tests:  OK 
[00:51:28.178,621] <inf> porting_tests:  random1 = 21, random2 = 31 in range [1;42]
[00:51:28.178,625] <inf> porting_tests:  * Get random draw : 
[00:51:30.962,660] <inf> porting_tests:  OK 
[00:51:30.962,668] <inf> porting_tests:  Random draw of 100000 numbers between [1;10] range
[00:51:30.962,685] <inf> porting_tests: ---------------------------------------- porting_test_config_rx_radio :
[00:51:31.519,488] <inf> porting_tests:  OK 
[00:51:31.519,518] <inf> porting_tests: ---------------------------------------- porting_test_config_tx_radio :
[00:51:31.576,697] <inf> porting_tests:  OK 
[00:51:31.576,721] <inf> porting_tests: ---------------------------------------- porting_test_sleep_ms :
[00:51:33.581,497] <inf> porting_tests:  OK 
[00:51:33.581,510] <inf> porting_tests:  Sleep time expected 2000ms / get 2000ms (margin +/-2ms)
[00:51:33.581,529] <inf> porting_tests: ---------------------------------------- porting_test_timer_irq_low_power :
[00:51:41.586,518] <inf> porting_tests:  OK 
[00:51:41.586,531] <inf> porting_tests:  Timer irq configured with 3000ms / get 3000ms (margin +2ms)
[00:51:41.586,535] <inf> porting_tests: ---------------------------------------- PORTING_TESTS END
```

## Technical Notes

- **Test Validation**: Each test validates specific HAL functions with timing tolerances and error margins
- **Hardware Dependencies**: Tests require proper GPIO, SPI, timer, and RTC configuration in device tree
- **Firmware Requirements**: LR11xx transceivers require compatible firmware versions for proper operation
- **Debug Support**: Comprehensive logging shows detailed test progress and failure diagnostics
