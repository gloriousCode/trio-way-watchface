#pragma once

#include <pebble.h>

typedef enum DigitStyle {
  DIGIT_STYLE_NONE = 0,
  DIGIT_STYLE_SMALL,
  DIGIT_STYLE_LARGE
} DigitStyle;

// Select a native glyph size. Resources are loaded on the first draw only.
void digits_set_style(DigitStyle style);

// Draw decimal digits centred within bounds without scaling; false permits a font fallback.
bool digits_draw(GContext *ctx, const char *text, GRect bounds, GColor colour, GColor outline_colour);

// Release the active style's masks and sub-bitmaps, including partial load failures.
void digits_release(void);
