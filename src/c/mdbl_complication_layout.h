#pragma once

#include <pebble.h>

// Place one date widget clear of the centre, dial labels and both outlined hands.
// Returns false without changing out when no safe candidate exists.
bool place_complication(GRect bounds, bool round_display, GSize size,
                        GPoint hour_end, GPoint minute_end, bool show_hands,
                        const GRect *avoid, size_t avoid_count, GRect *out);
