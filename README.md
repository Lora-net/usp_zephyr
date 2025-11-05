# Unified Software Platform (USP) for Zephyr

> **⚠️ ALPHA RELEASE v0.5.1** - First release of USP for Zephyr with partial validation.
> See [known limitations](doc/KNOWN_LIMITATIONS.md).

## Roadmap

**Version 1.0.0** (Coming Soon)
- ✅ Complete validation of all samples
- ✅ Bug fixes and stability improvements
- ✅ Production-ready release

## Overview

This repository aims to make USP available for [Zephyr RTOS](https://zephyrproject.org/). It wraps the existing
USP repository from Semtech, at the version defined in [west.yml](west.yml).

The USP repository includes LoRa Basics Modem 4.9.0.

Compatible [Zephyr versions](https://docs.zephyrproject.org/latest/releases):
- validated[<sup>7</sup>](#comments) on v4.2.0
- buildable[<sup>7</sup>](#comments) on v3.7.0 LTS

Supported Semtech radios:
- Validated[<sup>7</sup>](#comments) on [LoRa Plus EVK (Wio-LR2021 radio)](https://www.semtech.com/products/wireless-rf/lora-plus/lr2021)
- buildable[<sup>7</sup>](#comments) on [LR11xx shield radios](https://www.semtech.com/products/wireless-rf/lora-connect/lr1121)
- buildable[<sup>7</sup>](#comments) on [SX126x shield radios](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)

Supported MCU boards:
- Validated[<sup>7</sup>](#comments) on Xiao-nRF54L15
- buildable[<sup>7</sup>](#comments) on STMicro Nucleo-L476RG
- buildable[<sup>7</sup>](#comments) on Nordic nRF54L15-DK
- buildable[<sup>7</sup>](#comments) on Nordic nRF52840-DK

## Architecture

Documentation is available in [USP Architecture](doc/USP_Architecture.md).

## Installation

The following steps were tested for both Linux & Windows Development OS.

### Install Zephyr

Install Zephyr following : https://docs.zephyrproject.org/latest/develop/getting_started/index.html. \
If using a natively-supported MCU Board, you may test it with `blinky` sample.

Notes regarding SDK & Toolchains :
- The Zephyr SDK contains toolchains for each of Zephyr’s supported architectures (i.e arm, x86, ...). That does include a compiler, assembler, linker and other programs required to build and debug Zephyr applications.
- **The samples were built and tested with Zephyr RTOS v4.2 & Zephyr SDK v0.17.0.**
- Follow steps [here](https://docs.zephyrproject.org/4.2.0/develop/getting_started/index.html#install-the-zephyr-sdk) to install/update the Zephyr SDK on your specific platform. This needs to be done only once.
- Ensure your SDK is compliant with the used version of Zephyr RTOS : https://github.com/zephyrproject-rtos/sdk-ng/wiki/Zephyr-Version-Compatibility
- To use other Toolchains than the one provided by default with the Zephyr SDK, please refer to https://docs.zephyrproject.org/latest/develop/toolchains/index.html (**Please be aware that the current delivery was not validated with other toolchains than the one from the default Zephyr SDK**)

### Install USP for Zephyr

```bash
mkdir zephyr_workspace
cd zephyr_workspace
git clone https://github.com/Lora-net/usp_zephyr.git
west init -l usp_zephyr
west update
```

## Samples

Samples are located in [`samples/usp`](samples/usp/README.md) directory (click for more details, and test after full installation is complete):

## Build & Run for several MCU boards, Semtech Radio boards, and several Zephyr versions

Using Semtech LoRa Plus Development Kit (Xiao-nRF54L15 + LR2021-Wio) on `periodical_uplink` sample :
```bash
cd zephyr_workspace
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_lr2021wio usp_zephyr/samples/usp/lbm/periodical_uplink
west flash
```

You need to inform Zephyr which board and Semtech radio shield are to be used. Semtech shields are in the `boards/shields/` subfolders as `*.overlay` files.
They describe which transceiver model will be used by the application, e.g `semtech_lr2021wio`.

This will build your project in the `build` subdirectory (can be configured by `--build-dir <dir>` or `-b <dir>` command-line option).
You can pass `--pristine` or `-p always` to ensure the build directory is clean.


#### User-side board overlays
In each sample folder, there is a `boards` subfolder is which is placed an `.overlay` file for each supported board. This overlay file is used to enable or disable board features (e.g, disabling USART peripheral to optimize power consumption).

You might want to customize the LoRa transceiver configuration (to change TCXO startup time for example). This can be achieved by:
- Modifying the `boards/<used_board>.overlay` file accordingly, or
- Adding an extra overlay file and passing it to the build command via the CMake option `-DEXTRA_DTC_OVERLAY_FILE="<path_to_overlay_file>"`.

For example, if you wan to change TCXO wakeup time on LR1110 radio:

```dts
&lora_semtech_lr1110mb1xxs {
    tcxo-wakeup-time = <12>;
};
```

Please note that modifying the default values may decrease radio performance and/or increase the radio power consumption.

### Supported MCU & Radio boards

USP for Zephyr is:
- **Validated** on Semtech&copy; LR2021-Wio radio (that uses the "Wio" interface), with or without the LoRa Plus Expansion Board, on the Xiao-nRF54 `xiao_nrf54l15/nrf54l15/cpuapp`[<sup>2</sup>](#comments)
- **Buildable** on Semtech&copy; LR2021-Wio radio (that uses the "Wio" interface), with the LoRa Plus Expansion Board, on the following boards:
    - Nordic nRF52840-DK `nrf52840dk/nrf52840`[<sup>3</sup>](#comments)
    - Nordic nRF54L15-DK `nrf54l15dk/nrf54l15/cpuapp`[<sup>3</sup>](#comments)
    - STMicro Nucleo-L476RG `nucleo_l476rg`[<sup>3</sup>](#comments)
- **Buildable** on Semtech&copy; LR1110, LR1120 and LR1121 shields (based on official Semtech Reference Designs available [here](https://www.semtech.fr/products/wireless-rf/lora-edge/)) using the Arduino-style mbed connector, on the following boards:
    - Nordic nRF52840-DK `nrf52840dk/nrf52840`
    - Nordic nRF54L15-DK `nrf54l15dk/nrf54l15/cpuapp`[<sup>1</sup>](#comments)
    - STMicro Nucleo-L476RG `nucleo_l476rg`

### Zephyr version support
The following Zephyr versions are supported:

| Board | Zephyr 4.2.0 | Zephyr 3.7.0 LTS |
| :---- | :----------: | :--------------: |
| Nucleo-L476RG | yes | yes |
| nRF52840-DK   | yes | yes |
| nRF54L15-DK   | yes | no[<sup>4</sup>](#comments) |
| Xiao-nRF54    | yes | no  |

USP for Zephyr samples:
- are validated on Zephyr v4.2.0
- are buildable but not validated on Zephyr v3.7.0 LTS
- _might_ work with other Zephyr versions, but are not guaranteed to.

### Matrix of Board & Zephyr Support

How to build depending on the MCU board, the Radio board, the Zephyr version ?
```bash
west build --pristine --board <board> --shield <shield>
```

<table>
    <thead>
        <tr>
            <th rowspan=2 align="center">Board</th>
            <th rowspan=2 align="center">Zephyr</th>
            <th rowspan=2 align="center">MCU tag</th>
            <th colspan=2 align="center">Wio-LR2021</th>
            <th rowspan=2 align="center">LR11xx<img src="doc/assets/LR1110.jpg" alt="LR1110.jpg"></img></th>
            <th rowspan=2 align="center">SX126x<img src="doc/assets/SX1261.jpg" alt="SX1261.png"></img></th>
        </tr>
        <tr>
            <th align="center">Standalone (No LoRa Plus Expansion board)<img src="doc/assets/Wio_standalone.jpg" alt="LR2021Wio.jpg"></img></th>
            <th align="center">Using LoRa Plus Expansion Board<img src="doc/assets/LoRa_Plus_Expansion_Board_wio.png" alt="LoRa_Plus_Expansion_Board_wio.png"></img></th>
        </tr>
    </thead>
    <tbody>
      <tr>
        <td align="center">Xiao nRF54L15<img src="doc/assets/XIAO_nRF54L15.png" alt="XIAO_nRF54L15.png"></img></td>
        <td align="center">4.2</td>
        <td align="center"><code>--board xiao_nrf54l15/nrf54l15/cpuapp</code></td>
        <td align="center"><code>--shield semtech_lr2021wio</code></td>
        <td align="center"><code>--shield semtech_loraplus_expansion_board --shield semtech_lr2021wio</code></td>
        <td align="center">N/A</td>
        <td align="center">N/A</td>
      </tr>
      <tr>
        <td align="center">Nucleo L476RG<img src="doc/assets/Nucleo_L476RG.png" alt="Nucleo_L476RG.png"></td>
        <td align="center">3.7 or 4.2</td>
        <td align="center"><code>--board nucleo_l476rg/stm32l476xx</code></td>
        <td align="center">N/A</td>
        <td align="center"><code>--shield semtech_mbed_wio_interface --shield semtech_lr2021wio</code></td>
        <td align="center"><code>--shield semtech_lr1110mb1xxs</code> or <code>--shield semtech_lr1120mb1xxs</code> or <code>--shield semtech_lr1121mb1xxs</code></td>
        <td align="center"><code>--shield semtech_sx1261mb2bas</code></td>
      </tr>
      <tr>
        <td align="center">nRF52840 DK<img src="doc/assets/nRF52840_DK.png" alt="nRF52840_DK.png"></td>
        <td align="center">3.7 or 4.2</td>
        <td align="center"><code>--board nrf52840dk/nrf52840</code></td>
        <td align="center">N/A</td>
        <td align="center"><code>--shield semtech_mbed_wio_interface --shield semtech_lr2021wio</code></td>
        <td align="center"><code>--shield semtech_lr1110mb1xxs</code> or <code>--shield semtech_lr1120mb1xxs</code> or <code>--shield semtech_lr1121mb1xxs</code></td>
        <td align="center"><code>--shield semtech_sx1261mb2bas</code></td>
      </tr>
      <tr>
        <td align="center">nRF54L15 DK<img src="doc/assets/nRF54L15_DK.png" alt="nRF54L15_DK.png"></td>
        <td align="center">4.2</td>
        <td align="center"><code>--board nrf54l15dk/nrf54l15/cpuapp</code></td>
        <td align="center">N/A</td>
        <td align="center"><code>--shield semtech_nrf54l15dk_mbed_interface --shield semtech_mbed_wio_interface --shield semtech_lr2021wio</code></td>
        <td align="center"><code>--shield semtech_nrf54l15dk_mbed_interface --shield semtech_lr1110mb1xxs</code> or <code>--shield semtech_nrf54l15dk_mbed_interface --shield semtech_lr1120mb1xxs</code> or <code>--shield semtech_nrf54l15dk_mbed_interface --shield semtech_lr1121mb1xxs</code></td>
        <td align="center"><code>--shield semtech_nrf54l15dk_mbed_interface --shield semtech_sx1261mb2bas</code></td>
      </tr>
      <tr>
        <td align="center">Any other board embedding Arduino (mbed) header</td>
        <td align="center">Depends on the board</td>
        <td align="center"><code>--board board/cpu</code></td>
        <td align="center">N/A</td>
        <td align="center">based on <code>--shield semtech_mbed_wio_interface --shield semtech_lr2021wio</code></td>
        <td align="center">based on <code>--shield semtech_lr1110mb1xxs</code> or <code>--shield semtech_lr1120mb1xxs</code> or <code>--shield semtech_lr1121mb1xxs</code></td>
        <td align="center">based on <code>--shield semtech_sx1261mb2bas</code></td>
      </tr>
    </tbody>
</table>

### Building the samples - details

Navigate into the sample folder you want to build, then run (with the relevant board and shield names):

```bash
west build -p auto --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_lr2021wio
```

By default, West builds the samples in the `build` folder. You can choose another name by passing `-d <build_directory_name>` (or `--build-dir`) to the build command line.
You can pass the `-p <always|auto>` (or `--pristine <always|auto>`) to configure the pristine build. The `always` option will delete the build folder and rebuild all, the `auto` will let West decide if a pristine build is necessary, and build only necessary files. It is recommended to us `always` build if you encounter unknown compilation errors. Using `-p` (or `--pristine`) without a velue has the same effect as giving `always`.

### Flashing the samples
To flash the samples, you will need additional tools installed to your computer.

#### Nordic Development Kits (DKs)
For Nordic DKs, you will need:
- the Nordic `nrfutil` tool (alongside with its `device` module) installed and available in your PATH. Also, please note that `nrfjprog` tool by Nordic is deprecated and doesn't include support for latest Nordic CPUs such as nRF54 series, so it is recommended to always use `nrfutil`.
Please visit [here](https://docs.nordicsemi.com/bundle/nrfutil/page/guides/installing.html) for a guide on how to install `nrfutil` and its `device` module.
- SEGGER J-Link drivers, available on [Segger website](https://www.segger.com/downloads/jlink/). Download and install the driver corresponding to your system.

Then, from the same directory the sample was built, run:
```bash
west flash --runner nrfutil
# or
west flash --runner nrfutil --build-dir nrf54_build # If a different build output folder name was supplied when building
```

#### Xiao-nRF54 target
The Xiao-nRF54 doesn't embeds a J-Link interface, it is instead interfaced with a CMSIS-DAP interface.
We recommend using `pyocd`, version >= 0.38.0 for both flashing and debugging. By default, `pyocd` should be included when installing Zephyr requirements, it will be intalled inside the venv. Use `pyocd --version` to check the installed version. Use `pip install pyocd` if it is not already installed.
On Debian-based systems, you may need to install additional udev rules to be able to connect to the board. You might want to follow the instructions [here](https://github.com/pyocd/pyOCD/tree/main/udev) to install PyOCD known USB devices. Additionaly, you will have to manually install `99-xiao-nrf54l15.rules` file found in `usp_zephyr/boards/seeed/xiao_nrf54l15/support` using the same procedure.

From the sample directory, run:
```bash
west flash
```

In case you want to flash with native pyocd command, you can run:
```bash
pyocd flash -t nrf54l ./build_xiao/zephyr/zephyr.hex # Replace build_xiao with your build folder name
```
If you encounter an error saying the the board is locked, run
```bash
pyocd erase -c --target nrf54l
```
before.

#### STM32L476RG
STM32 chips use ST-Link as interface, and are programmed using STM32CubeProgrammer tool. You might want to [download and install STM32CubeProg](https://www.st.com/en/development-tools/stm32cubeprog.html) from ST website. Then, Zephyr should be able to locate the installation folder and use the tool.

Then, simply run
```bash
west flash
```

### Samples output messages
The samples uses Zephyr's `logging` api to oputput logging messages. Then, depending on the board, these messages are sent to the user, typically via a UART link. On all boards officialy supported by Semtech, the logs are sent simply via UART to a debugger IC present on the board, that exposes (at least) one CDC interface (Virtual Serial/COM Port) on its USB interface. Some boards may expose more than one port to the host computer, and only one will be used.
Any serial terminal software can be used to read data. On Unix systems, `gtkterm` is recommended as it handles ANSI color formatting used by LBM in its output messages. On Windows, `TeraTerm` can be used. \
Default config is **115200 baud, 8 data bits, 1 stop bit, no parity, no flow control**. \
The ANSI color formatting can be disabled in the sample prj.conf via the `CONFIG_LOG_BACKEND_SHOW_COLOR` Kconfig option.

## Integration in VS Code

Files provided to support VS Code integration are :
- `usp_zephyr_workspace.code-workspace`
- Build & flash : `.vscode/tasks.json` (to be update to local user host configuration)
- Debug : `.vscode/launch.json` (to be update to local user host configuration)

Open the VS Code Workspace file (File->Open Workspace from file) : `usp_zephyr/usp_zephyr_workspace.code-workspace` to load USP integration in VS Code.

The configuration is for both Windows & Linux.

CTRL-SHIFT-P will allow you to run tasks :
- West Build
- West Interactive Build
- West Flash

F5 will allow you to run & debug.

Default configuration is for
- Radios
- LoRa Plus dev kit + xiao nRF54l15,
- Wio LR2021,
- with Cortex-Debug extension from marus25.

For other MCU and radio boards, modify tasks.json & launch.json.

### Debugging the samples (`launch.json`)

This section presents how to debug the sample from within VSCode environment using `Cortex-Debug` extension. As the name implies, this extension is designed for debugging Cortex-based MCUs.

We will use as example the nRF52840-DK Development Kit board embedding the nRF52840 MCU.

This section shows the configuration for debugging with zephyr SDK GDB tool using
- Xiao nRF54 with pyOCD
- STM32L476RG with openocd
- nRF52840 DK with JLink
- nRF54l15 DK with JLink

The installation steps of openocd, jlink is not covered by this documentation. Please refer to those tools installation guide.

Edit the configuration:
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Xiao nRF54 with PyOCD",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder:Zephyr Workspace}/build/zephyr/zephyr.elf",
            "request": "launch",
            "type": "cortex-debug",
            "runToEntryPoint": "main",
            "servertype": "pyocd",
            "device": "nrf54l",
            "targetId": "nrf54l",
            "postRestartCommands": [
                "monitor reset halt"
            ],
            //"preLaunchTask": "West Build",
            "toolchainPrefix": "arm-zephyr-eabi",
            "windows": {
                "gdbPath": "${userHome}:\\zephyr-sdk-0.17.0\\arm-zephyr-eabi\\bin\\arm-zephyr-eabi-gdb.exe"
            },
            "linux": {
                "gdbPath": "${userHome}/zephyr-sdk-0.17.0/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb"
            },
            "interface": "swd",
            "showDevDebugOutput": "raw"
        },
        {
            "name": "STM32L476RG with openocd",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder:Zephyr Workspace}/build/zephyr/zephyr.elf",
            "request": "launch",
            "type": "cortex-debug",
            "runToEntryPoint": "main",
            "showDevDebugOutput": "none",
            "servertype": "openocd",
            "configFiles": [
                "interface/stlink.cfg",
                "target/stm32l4x.cfg"
            ],
            // "openOCDLaunchCommands": [
            //     "hla_serial  0671FF495648807567102644"
            // ],
            //"preLaunchTask": "West Build",
            "windows": {
                "gdbPath": "c:\\zephyr-sdk-0.17.1\\arm-zephyr-eabi\\bin\\arm-zephyr-eabi-gdb.exe"
            },
            "linux": {
                "gdbPath": "${userHome}/zephyr-sdk-0.17.0/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb"
            }
        },
        {
            "name": "nRF52840 DK with JLink",
            "device": "nRF52840_xxAA",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder:Zephyr Workspace}/build/zephyr/zephyr.elf",
            "request": "launch",
            "type": "cortex-debug",
            "runToEntryPoint": "main",
            "servertype": "jlink",
            //"preLaunchTask": "West Build",
            "windows": {
                "gdbPath": "${userHome}:\\zephyr-sdk-0.17.0\\arm-zephyr-eabi\\bin\\arm-zephyr-eabi-gdb.exe"
            },
            "linux": {
                "gdbPath": "${userHome}/zephyr-sdk-0.17.0/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb"
            }
        },
        {
            "name": "nRF54l15 DK with JLink",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder:Zephyr Workspace}/build/zephyr/zephyr.elf",
            "request": "launch",
            "type": "cortex-debug",
            "runToEntryPoint": "main",
            "servertype": "jlink",
            "device": "nRF54L15_M33",
            //"preLaunchTask": "West Build",
            "windows": {
                "gdbPath": "${userHome}\\zephyr-sdk-0.17.0\\arm-zephyr-eabi\\bin\\arm-zephyr-eabi-gdb.exe"
            },
            "linux": {
                "gdbPath": "${userHome}/zephyr-sdk-0.17.0/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb"
            }
            "interface": "swd",
            "showDevDebugOutput": "raw"
        }
    ]
}
```
Change the `gdbPath` to point to zephyr SDK debugger. `arm-zephyr-eaby-gdb` executable is typically located in the Zephyr SDK folder.
You may have to update `executable` if you changed the location of build directory. Typically, zephyr.elf is generated in a `build/zephyr` folder.
You can uncomment `preLaunchTask` field if you want to automatically build your project before debugging it.

### Build in VS Code

The first solution is to open a Terminal and use the compilation command lines described above.

Alternately, you may tune the provided `tasks.json` file to
- your west environment
- your MCU & radio boards
- your application : path & compilation flags

Below is an excerpt from the tasks.json file for the "West Build" task :
```json
{
	"version": "2.0.0",
	"tasks": [
        {
            "label": "West Build",
            "type": "shell",
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "options": {
                "cwd": "${workspaceFolder:Zephyr Workspace}",
            },
            "linux": {
                "command": "${workspaceFolder:Zephyr Workspace}/../zephyrproject/.venv/bin/west",
                "args": [
                    "build",
                    "--pristine",
                    "auto",
                    "--board",
                    "xiao_nrf54l15/nrf54l15/cpuapp",
                    "--shield",
                    "semtech_lr2021wio",
                    "${workspaceFolder:LoRa MultiProtocol SW Platform for Zephyr}/samples/usp/sdk/ranging_demo",
                    "--",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCMAKE_C_FLAGS=\"-DRANGING_DEVICE_MODE=RANGING_DEVICE_MODE_SUBORDINATE\"",
                ],
            },
            "windows": {
                "command": "${workspaceFolder:Zephyr Workspace}\\..\\zephyrproject\\.venv\\Scripts\\west.exe",
                "args": [
                    "build",
                    "--pristine",
                    "auto",
                    "--board",
                    "xiao_nrf54l15/nrf54l15/cpuapp",
                    "--shield",
                    "semtech_lr2021wio",
                    "${workspaceFolder:LoRa MultiProtocol SW Platform for Zephyr}\\samples\\usp\\sdk\\ranging_demo",
                    "--",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCMAKE_C_FLAGS=\"-DRANGING_DEVICE_MODE=RANGING_DEVICE_MODE_SUBORDINATE\"",
                ],
            },
            "osx": {
                "command": "${workspaceFolder:Zephyr Workspace}/../zephyrproject/.venv/bin/west",
                "args": [
                    "build",
                    "--pristine",
                    "auto",
                    "--board",
                    "xiao_nrf54l15/nrf54l15/cpuapp",
                    "--shield",
                    "semtech_lr2021wio",
                    "${workspaceFolder:LoRa MultiProtocol SW Platform for Zephyr}/samples/usp/sdk/ranging_demo",
                    "--",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DCMAKE_C_FLAGS=\"-DRANGING_DEVICE_MODE=RANGING_DEVICE_MODE_SUBORDINATE\"",
                ],
            },
            "problemMatcher": []
        },

```

The user has to update
- `${workspaceFolder:Zephyr Workspace}/../zephyrproject/.venv/bin/west` with the installation path of west
- `${workspaceFolder:LoRa MultiProtocol SW Platform for Zephyr}/samples/usp/sdk/ranging_demo`with the application path ,
- `-DCMAKE_C_FLAGS=\"-DRANGING_DEVICE_MODE=RANGING_DEVICE_MODE_SUBORDINATE\"` with the compilation flags

The user may want to update
- the default MCU board `xiao_nrf54l15/nrf54l15/cpuapp`
- the default radio board `semtech_lr2021wio`

## Build your own project

Example of use of LoRa Multi-Protocol SW Platform in you project :

```bash
source your_west_virtual_environment/.venv/bin/activate
mkdir zephyr_workspace & cd zephyr_workspace
mkdir application # or git clone https://xxxxx application
```

Edit there your `application/west.yml` manifest file choosing the revision of ups_zephyr & ups :
<div style="display: flex; gap: 2em;">
  <div>
    <h3>Few dependency description</h3>
    <pre>
manifest:
  projects:
    - name: usp_zephyr
      path: usp_zephyr
      url: https://github.com/Lora-net/usp_zephyr.git
      revision: develop
      import: true
  self:
    path: application
    </pre>
  </div>
  <div>
    <h3>More clear dependency description with selected version</h3>
    <pre>
manifest:
  projects:
    - name: zephyr
      url: https://github.com/zephyrproject-rtos/zephyr
      revision: v4.2.0
      import: true
    - name: usp_zephyr
      path: usp_zephyr
      url: https://github.com/Lora-net/usp_zephyr.git
      revision: develop
    - name: usp
      path: modules/lib/usp
      url: https://github.com/Lora-net/usp.git
      revision: develop
      submodules: true
  self:
    path: application
    </pre>
  </div>
</div>

and do :
```bash
west init -l application
west update
```

Work in your source files in `zephyr_workspace/application`
```bash
application/my_project
├── boards
├── CMakeLists.txt
├── prj.conf
├── prj_lowpower.conf
├── README.rst
└── src
```

And compile & flash as usual :
```bash
west build --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_lr2021wio application/xxx
west flash
```

## Porting Guide

(Coming Soon)
This chapter will explain how to port
- on other MCU
- on other radio PCB

## Power Consumption
Zephyr RTOS implements a way of handling the power states of the used devices (called "Power Management" (PM) actions). Basically, it offers a simple way for the RTOS to "tell" connected devices to enter sleep mode when Zephyr does so. However, as it's done without any driver intervention, the timing when the devices enter/exit sleep mode can't be precisely controlled, leading to a possible increase of power consumption. \
As so, Semtech radio drivers **don't** use Zephyr system for handling sleep mode, and are entered sleep as soon as possible to optimize power consumption.
It is important to note that the Zephyr PM actions are correctly handled by the internal MCU peripherals, allowing the samples to reach the following current consumption values when in sleep:

| Board | Current (uA) |
| ----- | ------------ |
| Xiao-nRF54L15 | **TO BE MEAS** |
| nRF52840-DK | ~3[<sup>5</sup>](#comments) |
| nRF54L15-DK | ~25[<sup>6</sup>](#comments) |

To reach low power, use `prj_lowpower.conf` when building, disabling logs:
```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_lr2021wio usp_zephyr/samples/usp/lbm/periodical_uplink -- -DCONF_FILE="prj_lowpower.conf"
```

## Troubleshooting
This section presents frequentely-encountered errors and how to fix them.

| Description | Cause | Workaround |
| :---------- | :---- | :--------- |
| Nothing outputs to the serial console when plugged. | <ul><li>The serial port opened isn't the board serial port, or the serial port configuration is wrong.</li><li>The board or the shield isn't properly connected.</li><li>The BUSY pin is held high by the radio.</li></ul> | Open another serial port or change serial port configuration. Check if board/shield is properly plugged and in the right orientation. Try another radio shield. |
| I can't see LoRa messages on my NS console. | <ul><li>The programmed region is wrong.</li><li>The antenna is disconnected or faulty.</li><li>You are not in range of any LoRa gateway connected to your NS.</li><li>The entered user credentials are wrong.</li></ul> | Check if the programmed region corresponds to your geographic location. Check the antenna connection. Check if you have a gateway in range, or host a gateway. Check the user credentials (devEUI, joinEUI, appKey). |

## Comments
> <sup>1</sup> The nRF54L15-DK board doesn't implement an Arduino interface as the other boards do, so an adapter (interface) board is needed in order to connect the Semtech shields. The mandatory pins are:
> | nRF54L15-DK Pin | Arduino connector Pin | LRXXxx shield pin function |
> | --------------- | --------------------- | -------------------------- |
> | P0.00 | A0 | LR_NRESET |
> | P1.06 | D7 | NSS |
> | P1.11 | D3 | BUSY |
> | P1.12 | D5 | DIO9 |
> | P2.06 | D13 | SPI_SCK |
> | P2.08 | D11 | SPI_MOSI |
> | P2.09 | D12 | SPI_MISO |
>
> The mandatory pin for geoloc example on shield embedding a LNA is:
> | nRF54L15-DK Pin | Arduino connector Pin | LRXXxx shield pin function |
> | --------------- | --------------------- | -------------------------- |
> | P1.13 | A3 | LNA_CTRL_MCU |
>
>
> Other pins:
> | nRF54L15-DK Pin | Arduino connector Pin | LRXXxx shield pin function |
> | --------------- | --------------------- | -------------------------- |
> | P0.01 | A5 | LED_RX |
> | P0.02 | A4 | LED_TX |
> | P0.03 | D14 | I2C_SDA |
> | P0.04 | D15 | I2C_SCL |

> <sup>2</sup> The Xiao-nRF54 board is, as of today, not available in official Zephyr sources, and will be added in a future Zephyr release.

> <sup>3</sup> The LR2021-Wio shield can also be plugged to nRF52840-DK, nRF54L15-DK and Nucleo-L476RG by using the LoRa Plus Expansion Board as interface board.

> <sup>4</sup> In Zephyr 3.7.0 LTS, the Nordic nRF54L15-DK is named nRF54L14PDK and its support is experimental: many peripherals are not properly handled, the power optimisations features are not implemented. It has been decided not to support this Zephyr version for this board.

> <sup>5</sup> Measured @VDD = 3V.

> <sup>6</sup> Measured @VDD = 3.3V. The nRF54L15 MCU is able to go as low as ~3µA, but we probably have some cross power-domains effects happening with the pinout used. See the [official documentation](https://docs.nordicsemi.com/bundle/ps_nrf54L15/page/chapters/pin.html#d379e188) for more details.

> <sup>7</sup> `Validated` : passed the Semtech nominal validation process, `Buildable` : can be compiled but did not go through full Semtech validation process and can be considered experimental
