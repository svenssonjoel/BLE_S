#!/bin/bash

openocd -f interface/stlink.cfg -f target/nrf52.cfg  -c "init" -c "program ble_tool_nrf52_fw_build/zephyr/zephyr.hex verify reset exit"
