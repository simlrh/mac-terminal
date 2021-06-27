#include <stdint.h>
#include <string.h>

#include "../terminal/keys.h"
#include "keyboard.h"

void keyboard_init(struct keyboard *keyboard) {

  keyboard->lshift = 0;
  keyboard->rshift = 0;
  keyboard->lctrl = 0;
  keyboard->rctrl = 0;
  keyboard->lalt = 0;
  keyboard->ralt = 0;
  keyboard->lgui = 0;
  keyboard->rgui = 0;
  keyboard->menu = 0;

  for (size_t i = 0; i < KEYBOARD_MAX_PRESSED_KEYS; ++i)
    keyboard->keys[i] = KEY_NONE;
}

static const uint8_t decoder[0x100] = {
    [0x00] = KEY_A,
    [0x0B] = KEY_B,
    [0x08] = KEY_C,
    [0x02] = KEY_D,
    [0x0E] = KEY_E,
    [0x03] = KEY_F,
    [0x05] = KEY_G,
    [0x04] = KEY_H,
    [0x22] = KEY_I,
    [0x26] = KEY_J,
    [0x28] = KEY_K,
    [0x25] = KEY_L,
    [0x2E] = KEY_M,
    [0x2D] = KEY_N,
    [0x1F] = KEY_O,
    [0x23] = KEY_P,
    [0x0C] = KEY_Q,
    [0x0F] = KEY_R,
    [0x01] = KEY_S,
    [0x11] = KEY_T,
    [0x20] = KEY_U,
    [0x09] = KEY_V,
    [0x0D] = KEY_W,
    [0x07] = KEY_X,
    [0x10] = KEY_Y,
    [0x06] = KEY_Z,
    [0x12] = KEY_1_EXCLAMATION_MARK,
    [0x13] = KEY_2_AT,
    [0x14] = KEY_3_NUMBER_SIGN,
    [0x15] = KEY_4_DOLLAR,
    [0x17] = KEY_5_PERCENT,
    [0x16] = KEY_6_CARET,
    [0x1A] = KEY_7_AMPERSAND,
    [0x1C] = KEY_8_ASTERISK,
    [0x19] = KEY_9_OPARENTHESIS,
    [0x1D] = KEY_0_CPARENTHESIS,
    [0x24] = KEY_ENTER,
    [0x35] = KEY_ESCAPE,
    [0x33] = KEY_BACKSPACE,
    [0x30] = KEY_TAB,
    [0x31] = KEY_SPACEBAR,
    [0x1B] = KEY_MINUS_UNDERSCORE,
    [0x18] = KEY_EQUAL_PLUS,
    [0x21] = KEY_OBRACKET_AND_OBRACE,
    [0x1E] = KEY_CBRACKET_AND_CBRACE,
    [0x2A] = KEY_BACKSLASH_VERTICAL_BAR,
    [0x29] = KEY_SEMICOLON_COLON,
    [0x27] = KEY_SINGLE_AND_DOUBLE_QUOTE,
    [0x32] = KEY_GRAVE_ACCENT_AND_TILDE,
    [0x2B] = KEY_COMMA_AND_LESS,
    [0x2F] = KEY_DOT_GREATER,
    [0x2C] = KEY_SLASH_QUESTION,
    [0x39] = KEY_CAPS_LOCK,
    [0x7A] = KEY_F1,
    [0x78] = KEY_F2,
    [0x63] = KEY_F3,
    [0x76] = KEY_F4,
    [0x60] = KEY_F5,
    [0x61] = KEY_F6,
    [0x62] = KEY_F7,
    [0x64] = KEY_F8,
    [0x65] = KEY_F9,
    [0x6D] = KEY_F10,
    [0x67] = KEY_F11,
    [0x6F] = KEY_F12,
    [0x47] = KEY_KEYPAD_NUM_LOCK_AND_CLEAR,
    [0x43] = KEY_KEYPAD_ASTERIKS,
    [0x4E] = KEY_KEYPAD_MINUS,
    [0x45] = KEY_KEYPAD_PLUS,
    [0x53] = KEY_KEYPAD_1_END,
    [0x54] = KEY_KEYPAD_2_DOWN_ARROW,
    [0x55] = KEY_KEYPAD_3_PAGEDN,
    [0x56] = KEY_KEYPAD_4_LEFT_ARROW,
    [0x57] = KEY_KEYPAD_5,
    [0x58] = KEY_KEYPAD_6_RIGHT_ARROW,
    [0x59] = KEY_KEYPAD_7_HOME,
    [0x5B] = KEY_KEYPAD_8_UP_ARROW,
    [0x5C] = KEY_KEYPAD_9_PAGEUP,
    [0x52] = KEY_KEYPAD_0_INSERT,
    [0x41] = KEY_KEYPAD_DECIMAL_SEPARATOR_DELETE,
    [0x72] = KEY_INSERT,
    [0x73] = KEY_HOME,
    [0x74] = KEY_PAGEUP,
    [0x75] = KEY_DELETE,
    [0x77] = KEY_END1,
    [0x79] = KEY_PAGEDOWN,
    [0x3C] = KEY_RIGHTARROW,
    [0x3B] = KEY_LEFTARROW,
    [0x3D] = KEY_DOWNARROW,
    [0x3E] = KEY_UPARROW,
    [0x4B] = KEY_KEYPAD_SLASH,
    [0x4C] = KEY_KEYPAD_ENTER,
    [0xFF] = KEY_NONE,
};

/*
    KEY_NONUS_BACK_SLASH_VERTICAL_BAR,
    KEY_SCROLL_LOCK,
*/

static void handle_keyup(struct keyboard *keyboard, uint8_t key) {
  if (key == KEY_NONE)
    return;

  for (size_t i = 0; i < KEYBOARD_MAX_PRESSED_KEYS; ++i)
    if (keyboard->keys[i] == key) {
      for (; i < KEYBOARD_MAX_PRESSED_KEYS - 1; ++i)
        keyboard->keys[i] = keyboard->keys[i + 1];

      keyboard->keys[KEYBOARD_MAX_PRESSED_KEYS - 1] = KEY_NONE;
      break;
    }
}

static void handle_keydown(struct keyboard *keyboard, uint8_t key) {
  if (key == KEY_NONE)
    return;

  for (size_t i = 0; i < KEYBOARD_MAX_PRESSED_KEYS; ++i)
    if (keyboard->keys[i] == key)
      return;

  for (size_t i = KEYBOARD_MAX_PRESSED_KEYS - 1; i > 0; --i)
    if (keyboard->keys[i - 1] != KEY_NONE)
      keyboard->keys[i] = keyboard->keys[i - 1];

  keyboard->keys[0] = key;
}

void keyboard_handle_code(struct keyboard *keyboard, uint8_t code) {
  if (code == KEY_NONE) {
    return;
  }

  bool keyup = code & 0b10000000;
  uint8_t keycode = code & 0b01111111;

  switch (keycode) {
    case 0x37: // LGUI
      keyboard->lgui = !keyup;
      break;
    case 0x38: // LSHIFT
      keyboard->lshift = !keyup;
      break;
    case 0x7B: // RSHIFT
      keyboard->rshift = !keyup;
      break;
    case 0x36: // LCTRL
      keyboard->lctrl = !keyup;
      break;
    case 0x3A: // LALT
      keyboard->lalt = !keyup;
      break;
    default:
      if (keyup) {
        handle_keyup(keyboard, decoder[keycode]);
      } else {
        handle_keydown(keyboard, decoder[keycode]);
      }
      break;
  }
}
