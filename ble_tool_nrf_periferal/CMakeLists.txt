# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(cdc_acm)

add_definitions(-D_32_BIT_ -D_PRELUDE -DTINY_SYMTAB)

add_custom_command(OUTPUT ../cdc_acm/lispbm/src/prelude.xxd 
                   COMMAND xxd
		   ARGS -i < ../cdc_acm/lispbm/src/prelude.lisp > ../cdc_acm/lispbm/src/prelude.xxd
		   DEPENDS ../cdc_acm/lispbm/src/prelude.lisp
                  )

FILE(GLOB app_sources src/*.c)
FILE(GLOB lisp_sources lispbm/src/*.c)
target_sources(app PRIVATE ${app_sources}
		   PRIVATE ${lisp_sources}
		   PRIVATE ../cdc_acm/lispbm/src/prelude.xxd)
target_include_directories(app PRIVATE lispbm/include
 			       PRIVATE lispbm/src)

#set_property(SOURCE ../cdc_acm/lispbm/src/prelude.c APPEND PROPERTY OBJECT_DEPENDS ../cdc_acm/lispbm/src/prelude.xxd)

#add_library(app STATIC lib/liblispbm.a)
# target_link_libraries(app lib/liblispbm.a)

