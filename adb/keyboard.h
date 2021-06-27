#pragma once

#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_MAX_PRESSED_KEYS 6

struct keyboard
{
  volatile uint8_t lshift : 1;
  volatile uint8_t rshift : 1;
  volatile uint8_t lctrl : 1;
  volatile uint8_t rctrl : 1;
  volatile uint8_t lalt : 1;
  volatile uint8_t ralt : 1;
  volatile uint8_t lgui : 1;
  volatile uint8_t rgui : 1;
  volatile uint8_t menu : 1;

  volatile uint8_t keys[KEYBOARD_MAX_PRESSED_KEYS];
};

void keyboard_init(struct keyboard *keyboard);

void keyboard_handle_code(struct keyboard *keyboard, uint8_t scancode);
