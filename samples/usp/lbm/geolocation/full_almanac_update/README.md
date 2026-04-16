# LR11xx full almanac update example

## Description

This application executes a full almanac update from a given almanac binary image, by using dedicated LoRa Basics Modem
API.

This example also provides a simple python script 'get_full_almanac.py' which is available in usp `full_almanac_update_example`. This script
fetches almanac content from traxmate.io and generate a C header file that is compiled with the embedded binary. Refer to USP Documentation for more details.

**NOTE**: This example is only applicable to LR1110 / LR1120 chips.

## Generation of almanac C header file

The python script usage to generate the almanac C header file can be obtained with:

```bash
$ python ./get_full_almanac.py --help
```

For example, in order to get the latest almanac image, one can execute the following:

```bash
$ python get_full_almanac.py -f almanac.h PUT_YOUR_TRAXMATE_TOKEN_HERE
```

In order to get an almanac image for a specific date (for testing purpose), one can execute the following:

```bash
$ python get_full_almanac.py -f almanac.h -g 1419724818 PUT_YOUR_TRAXMATE_TOKEN_HERE
```

> Note: update the GPS time provided with the -g option with the desired GPS time.

## Compile and flash the binary code

The example code expects the almanac C header file produced by *get_full_almanac.py* python script to be named ['almanac.h'](../../../../../modules/lib/usp/examples/main_examples/full_almanac_update/almanac.h).

The full almanac flasher tool can be compiled with :
```bash
west build --pristine --board nucleo_l476rg/stm32l476xx --shield semtech_lr1110mb1xxs usp_zephyr/samples/usp/lbm/geolocation/full_almanac_update/
```

**Flash the firmware:**
```bash
west flash
```
