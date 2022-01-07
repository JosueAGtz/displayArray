cmake_minimum_required(VERSION 3.19)

include(pico_sdk_import.cmake)

project(DisplayAClock)
file(GLOB SOURCES "*.c")

pico_sdk_init()

add_executable(DisplayAClock main.c ${SOURCES})

pico_generate_pio_header(DisplayAClock ${CMAKE_CURRENT_LIST_DIR}/SPILCD.pio)

target_link_libraries(DisplayAClock 
pico_stdlib
hardware_gpio
hardware_pio
hardware_interp
hardware_rtc
)

pico_add_extra_outputs(DisplayAClock)