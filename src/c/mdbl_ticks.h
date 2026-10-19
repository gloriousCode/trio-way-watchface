#pragma once

#include <pebble.h>

#define TICK_MARK_COUNT 60

typedef struct TickMark {
  GPoint inner;
  GPoint outer;
  bool is_hour;
} TickMark;

// Twelve longer hour marks and forty-eight shorter minute marks on true clock rays.
void ticks_layout(GRect bounds, bool round_display, TickMark marks[TICK_MARK_COUNT]);

// Draw neutral outlined ticks; cache their fixed geometry until the bounds change.
void draw_tick_dial(GContext *ctx, GRect bounds);
