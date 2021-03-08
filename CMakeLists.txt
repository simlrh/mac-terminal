cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

pico_sdk_init()

add_executable(test)

pico_generate_pio_header(test ${CMAKE_CURRENT_LIST_DIR}/test.pio)

target_sources(test PRIVATE test.c crt.c)

target_link_libraries(test PRIVATE pico_stdlib hardware_pio hardware_dma)

pico_add_extra_outputs(test)

add_custom_command(TARGET test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_CURRENT_LIST_DIR}
)
