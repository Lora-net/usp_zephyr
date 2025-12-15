# LoRa Basics Modem (LBM) Porting Guide for Zephyr

## Overview

This guide explains how to port existing LoRa Basics Modem applications to the **USP for Zephyr** environment. It covers the key API changes, initialization sequence, and compile definitions management specific to the Zephyr integration.

**Target Audience:** Developers porting existing LBM applications (from bare-metal or other RTOS) to Zephyr.

---

## Table of Contents

1. [Key Changes Summary](#key-changes-summary)
2. [Initialization Sequence](#initialization-sequence)
3. [Main Loop Integration](#main-loop-integration)
4. [Multithreading Considerations](#multithreading-considerations)
5. [Compile Definitions Management](#compile-definitions-management)

---

## Key Changes Summary

When porting from traditional LBM to USP for Zephyr, you need to:

| Traditional LBM | USP for Zephyr | Reason |
|----------------|----------------|--------|
| `smtc_modem_init()` only | **`smtc_rac_init()` + `smtc_modem_init()`** | RAC (Radio Access Controller) must be initialized first |
| `smtc_modem_run_engine()` only | **`smtc_modem_run_engine()` + `smtc_rac_run_engine()`** | Both engines need to run |
| Manual compile definitions | Update **`prj.conf`** with Kconfig options + Link if required the application against **`lbm_compile_definitions`** | Automatic propagation via CMake INTERFACE library |
| Direct API calls | **Use `SMTC_SW_PLATFORM_*` macros (optional)** | Mutex protection in multithreaded context |

---

## Initialization Sequence

### ✅ Correct Initialization Order

```c
#include <smtc_zephyr_usp_api.h>
#include <smtc_sw_platform_helper.h>
#include <smtc_modem_api.h>

int main(void)
{
    // 1. Initialize the platform (Zephyr-specific setup)
    SMTC_SW_PLATFORM_INIT();
    
    // 2. Initialize RAC (Radio Access Controller) - MUST BE FIRST
    SMTC_SW_PLATFORM_VOID(smtc_rac_init());
    
    // 3. Initialize LBM modem - AFTER RAC
    SMTC_SW_PLATFORM_VOID(smtc_modem_init(&modem_event_callback));
    
    // Your application code...
}
```

### ⚠️ Critical: Order Matters!

**`smtc_rac_init()` MUST be called BEFORE `smtc_modem_init()`**

This is because:
- RAC manages the radio hardware abstraction layer
- LBM depends on RAC for radio operations
- Calling them in wrong order will cause initialization failures

---

## Main Loop Integration

### Traditional LBM Main Loop

```c
// Traditional (bare-metal or FreeRTOS)
while(1) {
    uint32_t sleep_time_ms = smtc_modem_run_engine();
    // Sleep or handle events
    sleep(sleep_time_ms);
}
```

### USP for Zephyr Main Loop

```c
// USP for Zephyr - TWO engines to run!
while(true) {
    // 1. Run the modem engine
    uint32_t sleep_time_ms = smtc_modem_run_engine();
    
    // 2. Run the RAC engine - REQUIRED!
    smtc_rac_run_engine();
    
    // 3. Check for pending IRQ before sleeping
    if(smtc_rac_is_irq_flag_pending()) {
        continue; // Don't sleep if radio IRQ is pending
    }
    
    // 4. Wait for events or timeout
    struct k_sem* sems[] = { 
        smtc_modem_hal_get_event_sem(), 
        &my_app_event_sem 
    };
    wait_on_sems(sems, 2, K_MSEC(sleep_time_ms));
}
```

### Key Points

1. **`smtc_rac_run_engine()` is MANDATORY** - Must be called in addition to `smtc_modem_run_engine()`
2. **Check IRQ flags** - Use `smtc_rac_is_irq_flag_pending()` to avoid missing radio events
3. **Semaphore-based waiting** - Use Zephyr semaphores for efficient event handling

---

## Multithreading Considerations

### USP Threading Models

USP for Zephyr supports two threading models:

#### 1. **Single-Threaded Mode** (Default)
- Application manages the main loop
- Calls `smtc_modem_run_engine()` and `smtc_rac_run_engine()` directly
- No automatic threading

#### 2. **Multi-Threaded Mode** (`CONFIG_USP_MAIN_THREAD=y` and `CONFIG_USP_THREADS_MUTEXES=y` if required)
- USP creates a dedicated thread for modem/RAC engines
- Application callbacks run in USP thread context
- **Requires mutex protection for shared resources in case of preemptive threads use**

### Using Platform Macros for Thread Safety

The `SMTC_SW_PLATFORM_*` macros provide automatic mutex protection in preemptive multi-threaded mode:

```c
// Without macro (NOT thread-safe in preemptive multi-threaded mode)
smtc_modem_request_uplink(STACK_ID, port, false, buff, 4);

// With macro (thread-safe in ALL modes)
SMTC_SW_PLATFORM_VOID(smtc_modem_request_uplink(STACK_ID, port, false, buff, 4));
```

### Available Macros

| Macro | Usage | Description |
|-------|-------|-------------|
| `SMTC_SW_PLATFORM_INIT()` | Initialization | Initialize platform and mutex (if needed) |
| `SMTC_SW_PLATFORM_VOID(func)` | Void functions | Execute with mutex protection |
| `SMTC_SW_PLATFORM(var, func)` | Return value | Execute and assign result with mutex |

### Example: Thread-Safe API Calls

```c
// Initialization
SMTC_SW_PLATFORM_INIT();
SMTC_SW_PLATFORM_VOID(smtc_rac_init());
SMTC_SW_PLATFORM_VOID(smtc_modem_init(&modem_event_callback));

// Setting credentials
SMTC_SW_PLATFORM_VOID(smtc_modem_set_deveui(stack_id, dev_eui));
SMTC_SW_PLATFORM_VOID(smtc_modem_set_joineui(stack_id, join_eui));

// Requesting uplink
SMTC_SW_PLATFORM_VOID(smtc_modem_request_uplink(STACK_ID, port, false, buff, len));

// Getting status (with return value)
smtc_modem_status_mask_t status;
SMTC_SW_PLATFORM(status, smtc_modem_get_status(STACK_ID, &status));
```

### ⚠️ Important: Callback Context

When using multi-threaded mode (`CONFIG_USP_MAIN_THREAD=y` and `CONFIG_USP_THREADS_MUTEXES=y` if required):

```c
// This callback runs in the USP/RAC thread!
static void modem_event_callback(void)
{
    // Already protected by USP thread mutex
    // Safe to call LBM API directly here
    smtc_modem_get_event(&event, &count);
    
    // If you need to signal your application thread:
    k_sem_give(&my_app_sem); // Wake up application
}
```

**Key Point:** The modem event callback and `smtc_modem_run_engine()` / `smtc_rac_run_engine()` run in **different thread** from your application's main thread when `CONFIG_USP_MAIN_THREAD=y` is set.

---

## Compile Definitions Management

USP for Zephyr provides an **INTERFACE library** (`lbm_compile_definitions`) that can automatically propagates all LBM compile definitions to your application.

#### In Your Application CMakeLists.txt

If required, propagate in your application LBM compile definitions by using `target_link_libraries(app PRIVATE lbm_compile_definitions)`

```cmake
cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_lbm_app)

target_sources(app PRIVATE
  src/main.c
)

# Link against LBM compile definitions interface
# This gives you ALL LBM definitions automatically!
target_link_libraries(app PRIVATE lbm_compile_definitions)
```

### What Definitions Are Included?

#### How It Works: Kconfig → Compile Definitions

**The INTERFACE library automatically translates Kconfig options (configured in `prj.conf`) into C preprocessor definitions used by LBM code.**

```
prj.conf                         Your C Code
────────────────────      ─►     ───────────────────
CONFIG_LORA_BASICS_MODEM_CLASS_B=y    →    #if defined(ADD_CLASS_B)
CONFIG_LORA_BASICS_MODEM_RELAY_TX=y   →    #if defined(ADD_RELAY_TX)
CONFIG_LORA_BASICS_MODEM_GEOLOCATION=y →   #if defined(ADD_LBM_GEOLOCATION)
```

**You configure features in your `prj.conf` file**, and the build system automatically generates the corresponding `ADD_*`, `ENABLE_*`, and other preprocessor definitions.

#### Configuration in prj.conf

**Example from `samples/usp/lbm/lctt_certif/prj.conf`:**

```conf
# Certification test configuration with most features enabled
CONFIG_LORA_BASICS_MODEM_CLASS_B=y
CONFIG_LORA_BASICS_MODEM_CLASS_C=y
CONFIG_LORA_BASICS_MODEM_MULTICAST=y
CONFIG_LORA_BASICS_MODEM_CSMA=y

CONFIG_LORA_BASICS_MODEM_ALC_SYNC=y
CONFIG_LORA_BASICS_MODEM_ALC_SYNC_V2=y

CONFIG_LORA_BASICS_MODEM_FUOTA=y
CONFIG_LORA_BASICS_MODEM_FUOTA_V2=y
CONFIG_LORA_BASICS_MODEM_FUOTA_FMP=y
CONFIG_LORA_BASICS_MODEM_FUOTA_MPA=y

CONFIG_LORA_BASICS_MODEM_ALMANAC=y
CONFIG_LORA_BASICS_MODEM_STREAM=y
CONFIG_LORA_BASICS_MODEM_LFU=y
CONFIG_LORA_BASICS_MODEM_DEVICE_MANAGEMENT=y
CONFIG_LORA_BASICS_MODEM_STORE_AND_FORWARD=y
```

#### Kconfig Options → Generated Definitions

The following table shows the **Kconfig options** you set in `prj.conf` and the **C preprocessor definitions** automatically generated in your code:

| Kconfig Option (in prj.conf) | Generated C Define | Description |
|------------------------------|-------------------|-------------|
| **Core** | | |
| `CONFIG_LORA_BASICS_MODEM_NUMBER_OF_STACKS` | `NUMBER_OF_STACKS` | Number of LoRaWAN stacks |
| **Classes** | | |
| `CONFIG_LORA_BASICS_MODEM_CLASS_B=y` | `ADD_CLASS_B` | Class B support |
| `CONFIG_LORA_BASICS_MODEM_CLASS_C=y` | `ADD_CLASS_C` | Class C support |
| **Features** | | |
| `CONFIG_LORA_BASICS_MODEM_CSMA=y` | `ADD_CSMA` | CSMA support |
| `CONFIG_LORA_BASICS_MODEM_CSMA_BY_DEFAULT=y` | `ENABLE_CSMA_BY_DEFAULT` | CSMA enabled by default |
| `CONFIG_LORA_BASICS_MODEM_ALMANAC=y` | `ADD_ALMANAC` | Almanac support |
| `CONFIG_LORA_BASICS_MODEM_ALC_SYNC=y` | `ADD_SMTC_ALC_SYNC` | App Layer Clock Sync |
| `CONFIG_LORA_BASICS_MODEM_GEOLOCATION=y` | `ADD_LBM_GEOLOCATION` | Geolocation services |
| `CONFIG_LORA_BASICS_MODEM_STREAM=y` | `ADD_SMTC_STREAM` | Stream service |
| `CONFIG_LORA_BASICS_MODEM_LFU=y` | `ADD_SMTC_LFU` | Large File Upload |
| `CONFIG_LORA_BASICS_MODEM_DEVICE_MANAGEMENT=y` | `ADD_SMTC_CLOUD_DEVICE_MANAGEMENT` | Device Management |
| `CONFIG_LORA_BASICS_MODEM_STORE_AND_FORWARD=y` | `ADD_SMTC_STORE_AND_FORWARD` | Store and Forward |
| `CONFIG_LORA_BASICS_MODEM_BEACON_TX=y` | `MODEM_BEACON_APP` | Beacon TX app |
| `CONFIG_LORA_BASICS_MODEM_MULTICAST=y` | `SMTC_MULTICAST` | Multicast support |
| **Relay** | | |
| `CONFIG_LORA_BASICS_MODEM_RELAY_RX=y` | `ADD_RELAY_RX` | Relay RX support |
| `CONFIG_LORA_BASICS_MODEM_RELAY_TX=y` | `ADD_RELAY_TX` + `USE_RELAY_TX` | Relay TX support |
| **FUOTA** | | |
| `CONFIG_LORA_BASICS_MODEM_FUOTA=y` | `ADD_FUOTA` | FUOTA support |
| `CONFIG_LORA_BASICS_MODEM_FUOTA_FMP=y` | `ENABLE_FUOTA_FMP` | Firmware Mgmt Protocol |
| `CONFIG_LORA_BASICS_MODEM_FUOTA_MPA=y` | `ENABLE_FUOTA_MPA` | Multi-Package Access |
| `CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_NB_OF_FRAGMENTS_CONFIG=y` | `FRAG_MAX_NB` | Max fragments number |
| `CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_SIZE_OF_FRAGMENTS` | `FRAG_MAX_SIZE` | Max fragment size |
| `CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_FRAGMENTS_REDUNDANCY` | `FRAG_MAX_REDUNDANCY` | Max redundancy |
| **Debug/Test** | | |
| `CONFIG_LORA_BASICS_MODEM_PERF_TEST=y` | `PERF_TEST_ENABLED` | Performance testing |
| `CONFIG_LORA_BASICS_MODEM_DISABLE_JOIN_DUTY_CYCLE=y` | `TEST_BYPASS_JOIN_DUTY_CYCLE` | Bypass join duty cycle (test only) |

#### How to Enable Features in Your Application

1. **Edit your `prj.conf` file** to enable the LBM features you need:

```conf
# My application's prj.conf
CONFIG_USP_LORA_BASICS_MODEM=y
CONFIG_LORA_BASICS_MODEM_CLASS_C=y
CONFIG_LORA_BASICS_MODEM_GEOLOCATION=y
```

2. **Link against the INTERFACE library** in your `CMakeLists.txt`:

```cmake
target_link_libraries(app PRIVATE lbm_compile_definitions)
```
The LBM code will now see `ADD_CLASS_C` and `ADD_LBM_GEOLOCATION` defined, and compile the corresponding features.

### Architecture

```
┌─────────────────────────────────┐
│ lbm_compile_definitions         │
│ (INTERFACE library)             │
│                                 │
│ - NUMBER_OF_STACKS=1            │
│ - ADD_CLASS_B (if enabled)      │
│ - ADD_CLASS_C (if enabled)      │
│ - ADD_RELAY_RX, ADD_RELAY_TX    │
│ - ... (all LBM definitions)     │
└─────────────────────────────────┘
         ▲               ▲
         │               │
         │ link          │ link
         │               │
┌────────┴────────┐  ┌───┴──────────┐
│ LBM Library     │  │ Your App     │
│ (automatically  │  │ (via target_ │
│  linked)        │  │  link_libs)  │
└─────────────────┘  └──────────────┘
```

**Note:** The LBM library itself also links against this interface, ensuring consistency between library and applications.

---

## Reference Examples

### Minimal Working Example
See: `samples/usp/lbm/periodical_uplink/`

This example demonstrates:
- Proper initialization sequence
- Both run engines
- Event handling with semaphores
- Relay TX configuration
- Thread-safe API usage

### Key Files to Study
1. `samples/usp/lbm/periodical_uplink/src/main.c` - Complete application example
2. `doc/usp_zephyr_porting_guide.md` - Hardware porting guide

---

## Additional Resources

- **USP Architecture**: `doc/USP_Architecture.md`
- **Multi-Threading Management**: `doc/THREAD_MANAGEMENT.md`
- **Zephyr Porting Guide**: `doc/usp_zephyr_porting_guide.md`
- **Zephyr Documentation**: https://docs.zephyrproject.org

---

## Summary

Porting LBM applications to USP for Zephyr requires three main changes:

1. **Initialization**: Add `smtc_rac_init()` before `smtc_modem_init()`
2. **Main Loop**: Add `smtc_rac_run_engine()` alongside `smtc_modem_run_engine()`
3. **Build System**: Link against `lbm_compile_definitions` instead of manual definitions

The platform macros (`SMTC_SW_PLATFORM_*`) and Zephyr semaphores handle threading and synchronization automatically, making the porting process straightforward.

For a complete working example, refer to `samples/usp/lbm/periodical_uplink/`.

---


