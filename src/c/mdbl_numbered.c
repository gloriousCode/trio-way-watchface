#include "mdbl_numbered.h"

#include "mdbl.h"
#include "mdbl_digits.h"
#include "mdbl_hybrid.h"

#include <stdio.h>

DialHighlights numbered_highlights(const struct tm *now) {
  const uint8_t hour = now->tm_hour % 12;
  const uint8_t minute = ((now->tm_min + 2) / 5) % 12;
  return (DialHighlights){.hour = hour ? hour : 12, .minute = minute ? minute : 12};
}

void numbered_layout(GRect bounds, bool round_display, GSize cell, GRect labels[12]) {
  const GPoint centre = GPoint(bounds.origin.x + bounds.size.w / 2,
                               bounds.origin.y + bounds.size.h / 2);
  for (int number = 1; number <= 12; number++) {
    const GSize size = GSize(cell.w * (number >= 10 ? 2 : 1), cell.h + 6);
    labels[number - 1] = dial_label_on_edge(bounds, centre,
      normalise_angle(number * TRIG_MAX_ANGLE / 12), size, round_display);
  }
}

void draw_numbered_dial(GContext *ctx, GRect bounds, const struct tm *now,
                        GColor hour_colour, GColor minute_colour) {
#if defined(PBL_PLATFORM_GABBRO)
  const GSize cell = GSize(20, 32);
  const bool round_display = true;
#else
  const GSize cell = GSize(18, 28);
  const bool round_display = false;
#endif
  GRect labels[12];
  numbered_layout(bounds, round_display, cell, labels);
  const DialHighlights selected = numbered_highlights(now);
  digits_set_style(DIGIT_STYLE_SMALL);
  for (int number = 1; number <= 12; number++) {
    const bool hour_selected = number == selected.hour;
    const bool minute_selected = number == selected.minute;
    const GColor colour = hour_selected ? hour_colour : minute_selected ? minute_colour : GColorWhite;
    GRect text_rect = labels[number - 1];
    text_rect.size.h = cell.h;
    char text[3];
    snprintf(text, sizeof(text), "%d", number);
    draw_dial_label(ctx, text_rect, text, colour);
    if (hour_selected && minute_selected) {
      // Preserve both colours when the hands select the same numeral.
      const GPoint start = GPoint(text_rect.origin.x + text_rect.size.w / 2 - 5,
                                  text_rect.origin.y + cell.h + 2);
      const GPoint end = GPoint(start.x + 10, start.y);
      graphics_context_set_stroke_width(ctx, 4);
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_line(ctx, start, end);
      graphics_context_set_stroke_width(ctx, 2);
      graphics_context_set_stroke_color(ctx, minute_colour);
      graphics_draw_line(ctx, start, end);
    }
  }
}
