# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(ble_tool_nrf_fw)

add_definitions(-D_32_BIT_ -D_PRELUDE -DTINY_SYMTAB)

add_custom_command(OUTPUT ../ble_tool_nrf52_fw/lispbm/src/prelude.xxd 
                   COMMAND xxd
		   ARGS -i < ../ble_tool_nrf52_fw/lispbm/src/prelude.lisp > ../ble_tool_nrf52_fw/lispbm/src/prelude.xxd
		   DEPENDS ../ble_tool_nrf52_fw/lispbm/src/prelude.lisp
                  )

FILE(GLOB app_sources src/*.c)
FILE(GLOB lisp_sources lispbm/src/*.c)
target_sources(app PRIVATE ${app_sources}
		   PRIVATE ${lisp_sources}
		   PRIVATE ../ble_tool_nrf52_fw/lispbm/src/prelude.xxd)
target_include_directories(app PRIVATE lispbm/include
 			       PRIVATE lispbm/src)


