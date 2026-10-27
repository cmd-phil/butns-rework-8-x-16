cmake_minimum_required(VERSION 3.13)

# butns 16x8 firmware with monome iii (Lua scripting) and monome serial mode.
#
# Dependencies (git submodules in lib/, or override the paths below):
#   lib/iii       https://codeberg.org/tehn/iii           (GPLv3)
#   lib/lua       https://github.com/lua/lua  tag v5.5.1  (MIT)
#   lib/littlefs  https://github.com/littlefs-project/littlefs tag v2.9.3 (BSD-3)
# The pico-sdk is located via PICO_SDK_PATH (environment or -D).

set(PICO_BOARD pico CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(butns-iii C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(III_DIR ${CMAKE_CURRENT_LIST_DIR}/lib/iii CACHE PATH "iii sources")
set(LUA_DIR ${CMAKE_CURRENT_LIST_DIR}/lib/lua CACHE PATH "Lua 5.5 sources")
set(LFS_DIR ${CMAKE_CURRENT_LIST_DIR}/lib/littlefs CACHE PATH "littlefs sources")

# --- embed iii's lib.lua -------------------------------------------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
add_custom_command(
    OUTPUT ${GEN_DIR}/lib_lua.c ${GEN_DIR}/lib_lua.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/tools/embed.py
            ${III_DIR}/lua/lib.lua ${GEN_DIR}
    DEPENDS ${III_DIR}/lua/lib.lua ${CMAKE_CURRENT_LIST_DIR}/tools/embed.py
)

# --- Lua (32 bit integers and floats, as used by iii) --------------------------
file(GLOB LUA_SOURCES ${LUA_DIR}/l*.c)
list(REMOVE_ITEM LUA_SOURCES ${LUA_DIR}/lua.c ${LUA_DIR}/ltests.c)
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC ${LUA_DIR})
target_compile_definitions(lua PUBLIC LUA_32BITS=1)
target_link_libraries(lua PRIVATE pico_stdlib)

# --- littlefs ------------------------------------------------------------------
add_library(lfs STATIC ${LFS_DIR}/lfs.c ${LFS_DIR}/lfs_util.c)
target_include_directories(lfs PUBLIC ${LFS_DIR})
target_compile_definitions(lfs PUBLIC LFS_NO_DEBUG LFS_NO_WARN LFS_NO_ERROR)
target_link_libraries(lfs PRIVATE pico_stdlib)

# --- firmware ------------------------------------------------------------------
add_executable(butns-iii
    src/main.c
    src/hw.c
    src/leds.c
    src/ws2812.c
    src/grid.c
    src/monome.c
    src/settings.c
    src/usb_descriptors.c
    ${III_DIR}/flash.c
    ${III_DIR}/fs.c
    ${III_DIR}/iii.c
    ${III_DIR}/metro.c
    ${III_DIR}/midi.c
    ${III_DIR}/repl.c
    ${III_DIR}/serial.c
    ${III_DIR}/util.c
    ${III_DIR}/vm.c
    ${GEN_DIR}/lib_lua.c
)

pico_generate_pio_header(butns-iii ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_include_directories(butns-iii PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${III_DIR}
    ${GEN_DIR}
)

target_compile_definitions(butns-iii PRIVATE PICO_DIVIDER_IN_RAM=1)

# reset the LED colour state whenever iii (re)starts its Lua VM
target_link_options(butns-iii PRIVATE -Wl,--wrap=vm_init)

target_link_libraries(butns-iii PRIVATE
    lua
    lfs
    pico_stdlib
    pico_multicore
    pico_unique_id
    hardware_pio
    hardware_dma
    hardware_flash
    hardware_sync
    hardware_watchdog
    tinyusb_device
    tinyusb_board
)

pico_enable_stdio_usb(butns-iii 0)
pico_enable_stdio_uart(butns-iii 0)
pico_add_extra_outputs(butns-iii)
