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

| Parameter                    | Default | Description                        |
|------------------------------|---------|------------------------------------|
| `TEST_FLASH_ONLY`            | `n`     | Enable flash tests, disable others |
| `TEST_SPI_NB_LOOPS`          | `2`     | Number of SPI test iterations      |
| `TEST_CONFIG_RADIO_NB_LOOPS` | `2`     | Number of radio config test loops  |

## Compilation

**Standard HAL tests:**
```bash
west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_lr2021mb1xxs samples/usp/lbm/porting_tests
```

## Usage

1. **Build and Flash**: Compile and flash the application to target hardware
2. **Monitor Output**: Connect to UART/RTT console to view test results
3. **Automatic Execution**: Tests run automatically on startup and report pass/fail status
4. **Flash Tests** (if enabled): Requires MCU reset and relaunch to verify persistent storage

## Expected Output

### Standard Test Sequence
```
[00:00:00.000,000] <inf> porting_tests: PORTING_TESTS example is starting
[00:00:00.203,000] <inf> porting_tests: ---------------------------------------- porting_test_spi :
[00:00:00.203,000] <inf> porting_tests:  OK
[00:00:01.447,000] <inf> porting_tests: ---------------------------------------- porting_test_radio_irq :
[00:00:01.447,000] <inf> porting_tests:  OK
[00:00:06.716,000] <inf> porting_tests: ---------------------------------------- porting_test_get_time :
[00:00:06.716,000] <inf> porting_tests:  OK
[00:00:12.014,000] <inf> porting_tests: ---------------------------------------- porting_test_timer_irq :
[00:00:12.014,000] <inf> porting_tests:  OK
[00:00:29.313,000] <inf> porting_tests: ---------------------------------------- PORTING_TESTS END
```

## Technical Notes

- **Test Validation**: Each test validates specific HAL functions with timing tolerances and error margins
- **Hardware Dependencies**: Tests require proper GPIO, SPI, timer, and RTC configuration in device tree
- **Firmware Requirements**: LR11xx transceivers require compatible firmware versions for proper operation
- **Debug Support**: Comprehensive logging shows detailed test progress and failure diagnostics