cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(mac_terminal C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

pico_sdk_init()

add_executable(mac_terminal)
target_sources(
  mac_terminal
  PRIVATE
  main.c
  crt/crt.c
  adb/adb.c
  adb/keyboard.c
  fonts/font.c
  terminal/screen.c
  terminal/terminal.c
  terminal/terminal_config.c
  terminal/terminal_config_ui.c
  terminal/terminal_keyboard.c
  terminal/terminal_screen.c
  terminal/terminal_uart.c
)

pico_generate_pio_header(mac_terminal ${CMAKE_CURRENT_LIST_DIR}/crt/crt.pio)
pico_generate_pio_header(mac_terminal ${CMAKE_CURRENT_LIST_DIR}/adb/adb.pio)
add_custom_target(font_data
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/fonts/generate.py -b ${CMAKE_CURRENT_SOURCE_DIR}/fonts/erusfontbold.bdf -o ${CMAKE_CURRENT_SOURCE_DIR}/fonts/font_data.h ${CMAKE_CURRENT_SOURCE_DIR}/fonts/erusfont.bdf
  COMMENT "Generating font file..."
)
add_dependencies(mac_terminal font_data)

target_link_libraries(mac_terminal PRIVATE pico_stdlib hardware_pio hardware_dma hardware_timer hardware_uart hardware_irq)

pico_add_extra_outputs(mac_terminal)

add_custom_command(TARGET mac_terminal POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_CURRENT_LIST_DIR}
)

add_custom_command(TARGET mac_terminal POST_BUILD
  COMMAND openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -c "program ${CMAKE_BINARY_DIR}/mac_terminal.elf verify reset exit"
)
