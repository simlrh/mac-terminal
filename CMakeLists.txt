cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(mac_terminal C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

pico_sdk_init()

add_executable(mac_terminal)

pico_generate_pio_header(mac_terminal ${CMAKE_CURRENT_LIST_DIR}/crt/crt.pio)

target_sources(mac_terminal PRIVATE main.c crt/crt.c)

target_link_libraries(mac_terminal PRIVATE pico_stdlib hardware_pio hardware_dma)

pico_add_extra_outputs(mac_terminal)

add_custom_command(TARGET mac_terminal POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_CURRENT_LIST_DIR}
)
