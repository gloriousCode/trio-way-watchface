#pragma once

#include <pebble.h>

typedef struct DialHighlights {
  uint8_t hour;
  uint8_t minute;
} DialHighlights;

// Keep the current hour and round the minute to its nearest five-minute marker.
DialHighlights numbered_highlights(const struct tm *now);

// Lay out the twelve upright, unboxed numerals around the display perimeter.
void numbered_layout(GRect bounds, bool round_display, GSize cell, GRect labels[12]);

// Draw the complete dial, using the two hand colours for selected numerals.
void draw_numbered_dial(GContext *ctx, GRect bounds, const struct tm *now,
                        GColor hour_colour, GColor minute_colour);
