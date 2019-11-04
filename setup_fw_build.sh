#!/bin/bash

if [ -d "ble_tool_nrf52_fw_build" ]; then
    echo "Build directory exists!"
else
    mkdir ble_tool_nrf52_fw_build
    cd ble_tool_nrf52_fw_build
    cmake -G "Eclipse CDT4 - Unix Makefiles" -DBOARD=nrf52840_pca10056 ../ble_tool_nrf52_fw
fi    
