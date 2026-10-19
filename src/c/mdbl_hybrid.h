#pragma once

#include <pebble.h>

typedef struct HybridLayout {
  GRect hour_badge;
  GRect minute_badge;
  GRect date_badge;
  bool has_date_space;
} HybridLayout;

// Compute upright, separated rim labels with full rectangular or circular clearance.
bool calculate_hybrid_layout(GRect bounds, int32_t hour, int32_t minute,
                             bool round_display, GSize digit_cell, GSize date_size,
                             HybridLayout *layout);

// Draw two analogue hands with their corresponding upright digital values.
bool draw_hybrid(GContext *ctx, GRect bounds, GPoint centre, int16_t min_dimension,
                 int32_t hour, int32_t minute, const struct tm *now,
                 GColor hour_colour, GColor minute_colour, GSize date_size,
                 HybridLayout *layout);

// Shared dial helpers keep the same edge clearance and outlined numeral style.
GRect dial_label_on_edge(GRect bounds, GPoint centre, int32_t angle, GSize size, bool round_display);
void draw_dial_label(GContext *ctx, GRect label, const char *text, GColor colour);

// Stop a hand before any unboxed numeral so its counters stay unobstructed.
GPoint dial_hand_end(GPoint centre, int16_t length, int32_t angle, const GRect *labels, size_t label_count);
