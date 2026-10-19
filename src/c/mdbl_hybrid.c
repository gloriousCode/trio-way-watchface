#include "mdbl_hybrid.h"

#include "mdbl.h"
#include "mdbl_digits.h"
#include "mdbl_complication_layout.h"

static GPoint badge_centre(GRect rect) {
  return GPoint(rect.origin.x + rect.size.w / 2, rect.origin.y + rect.size.h / 2);
}

static GRect badge_on_ray(GPoint centre, int16_t radius, int32_t angle, GSize size) {
  const GPoint point = point_on_ray(centre, radius, normalise_angle(angle));
  return GRect(point.x - size.w / 2, point.y - size.h / 2, size.w, size.h);
}

static bool badge_inside(GRect badge, GRect bounds, bool round_display) {
  const int16_t margin = 3;
  if (badge.origin.x < bounds.origin.x + margin || badge.origin.y < bounds.origin.y + margin ||
      badge.origin.x + badge.size.w > bounds.origin.x + bounds.size.w - margin ||
      badge.origin.y + badge.size.h > bounds.origin.y + bounds.size.h - margin) {
    return false;
  }
  if (!round_display) {
    return true;
  }
  const GPoint centre = badge_centre(bounds);
  const int32_t radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - margin;
  for (int corner = 0; corner < 4; corner++) {
    const int32_t x = badge.origin.x + (corner & 1 ? badge.size.w - 1 : 0) - centre.x;
    const int32_t y = badge.origin.y + (corner & 2 ? badge.size.h - 1 : 0) - centre.y;
    if (x * x + y * y > radius * radius) {
      return false;
    }
  }
  return true;
}

static bool badges_separated(GRect a, GRect b) {
  const int16_t gap = 2;
  return a.origin.x + a.size.w + gap <= b.origin.x ||
         b.origin.x + b.size.w + gap <= a.origin.x ||
         a.origin.y + a.size.h + gap <= b.origin.y ||
         b.origin.y + b.size.h + gap <= a.origin.y;
}

static bool clears_centre(GRect badge, GPoint centre) {
  const int32_t right = badge.origin.x + badge.size.w - 1;
  const int32_t bottom = badge.origin.y + badge.size.h - 1;
  const int32_t x = centre.x < badge.origin.x ? badge.origin.x - centre.x :
    (centre.x > right ? centre.x - right : 0);
  const int32_t y = centre.y < badge.origin.y ? badge.origin.y - centre.y :
    (centre.y > bottom ? centre.y - bottom : 0);
  return x * x + y * y >= 8 * 8;
}

GRect dial_label_on_edge(GRect bounds, GPoint centre, int32_t angle, GSize size, bool round_display) {
  int16_t low = 0;
  int16_t high = bounds.size.w + bounds.size.h;
  while (low < high) {
    const int16_t middle = low + (high - low + 1) / 2;
    if (badge_inside(badge_on_ray(centre, middle, angle, size), bounds, round_display)) {
      low = middle;
    } else {
      high = middle - 1;
    }
  }
  return badge_on_ray(centre, low, angle, size);
}

bool calculate_hybrid_layout(GRect bounds, int32_t hour, int32_t minute,
                             bool round_display, GSize digit_cell, GSize date_size,
                             HybridLayout *layout) {
  if (!layout || digit_cell.w <= 0 || digit_cell.h <= 0 || bounds.size.w <= 0 || bounds.size.h <= 0) {
    return false;
  }
  const GPoint centre = badge_centre(bounds);
  const GSize label_size = GSize(digit_cell.w * 2, digit_cell.h);
  int32_t delta = normalise_angle(minute - hour);
  if (delta > TRIG_MAX_ANGLE / 2) {
    delta -= TRIG_MAX_ANGLE;
  }
  const int direction = delta < 0 ? -1 : 1;
  // Both values stay on the rim. When their hands are close, spread the labels
  // equally along the perimeter; the hands retain the actual time directions.
  for (int degrees = 0; degrees <= 30; degrees++) {
    const int32_t adjustment = direction * degrees * TRIG_MAX_ANGLE / 360;
    const GRect hour_label = dial_label_on_edge(bounds, centre, hour - adjustment, label_size, round_display);
    const GRect minute_label = dial_label_on_edge(bounds, centre, minute + adjustment, label_size, round_display);
    if (badge_inside(hour_label, bounds, round_display) &&
        badge_inside(minute_label, bounds, round_display) &&
        clears_centre(hour_label, centre) && clears_centre(minute_label, centre) &&
        badges_separated(hour_label, minute_label)) {
      layout->hour_badge = hour_label;
      layout->minute_badge = minute_label;
      const GRect labels[] = {hour_label, minute_label};
      const int16_t dimension = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
      const GPoint hour_end = dial_hand_end(centre, dimension * HOUR_HAND_LENGTH_PERCENT / 100,
                                            hour, labels, ARRAY_LENGTH(labels));
      const GPoint minute_end = dial_hand_end(centre, dimension * MINUTE_HAND_LENGTH_PERCENT / 100,
                                              minute, labels, ARRAY_LENGTH(labels));
      layout->has_date_space = place_complication(bounds, round_display, date_size,
        hour_end, minute_end, true, labels, ARRAY_LENGTH(labels), &layout->date_badge);
      return true;
    }
  }
  return false;
}

void draw_dial_label(GContext *ctx, GRect label, const char *text, GColor colour) {
  // Opposing luminance keeps a coloured numeral legible across all three
  // sectors without bringing back an opaque tile behind it.
  const unsigned luminance = ((colour.argb >> 4) & 3) * 299 +
    ((colour.argb >> 2) & 3) * 587 + (colour.argb & 3) * 114;
  const GColor outline_colour = luminance < 1500 ? GColorWhite : GColorBlack;
  if (!digits_draw(ctx, text, label, colour, outline_colour)) {
    // Keep the allocation-failure fallback readable without a filled badge.
    GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    graphics_context_set_text_color(ctx, outline_colour);
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        GRect outline = label;
        outline.origin.x += dx;
        outline.origin.y += dy;
        graphics_draw_text(ctx, text, font, outline,
                           GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      }
    }
    graphics_context_set_text_color(ctx, colour);
    graphics_draw_text(ctx, text, font, label,
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

GPoint dial_hand_end(GPoint centre, int16_t length, int32_t angle, const GRect *labels, size_t label_count) {
  GPoint end = centre;
  // Leave a small gap before either transparent numeral rather than drawing
  // through its counters. This also covers labels displaced by a close pair.
  for (int radius = 1; radius <= length; radius++) {
    const GPoint next = point_on_ray(centre, radius, angle);
    for (size_t i = 0; i < label_count; i++) {
      const GRect label = labels[i];
      if (next.x >= label.origin.x - 5 && next.x < label.origin.x + label.size.w + 5 &&
          next.y >= label.origin.y - 5 && next.y < label.origin.y + label.size.h + 5) {
        return end;
      }
    }
    end = next;
  }
  return end;
}

bool draw_hybrid(GContext *ctx, GRect bounds, GPoint centre, int16_t min_dimension,
                 int32_t hour, int32_t minute, const struct tm *now, bool twelve_hour,
                 GColor hour_colour, GColor minute_colour, GSize date_size,
                 HybridLayout *layout) {
#if defined(PBL_PLATFORM_GABBRO)
  const GSize digit_cell = GSize(20, 32);
  const bool round_display = true;
#else
  const GSize digit_cell = GSize(18, 28);
  const bool round_display = false;
#endif
  if (!calculate_hybrid_layout(bounds, hour, minute, round_display, digit_cell, date_size, layout)) {
    return false;
  }
  const GRect labels[] = {layout->hour_badge, layout->minute_badge};
  const GPoint hour_end = dial_hand_end(centre, min_dimension * HOUR_HAND_LENGTH_PERCENT / 100, hour, labels, ARRAY_LENGTH(labels));
  const GPoint minute_end = dial_hand_end(centre, min_dimension * MINUTE_HAND_LENGTH_PERCENT / 100, minute, labels, ARRAY_LENGTH(labels));
  graphics_context_set_stroke_width(ctx, HOUR_HAND_STROKE + 2);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, centre, hour_end);
  graphics_context_set_stroke_width(ctx, MINUTE_HAND_STROKE + 2);
  graphics_draw_line(ctx, centre, minute_end);
  graphics_context_set_stroke_width(ctx, HOUR_HAND_STROKE);
  graphics_context_set_stroke_color(ctx, hour_colour);
  graphics_draw_line(ctx, centre, hour_end);
  graphics_context_set_stroke_width(ctx, MINUTE_HAND_STROKE);
  graphics_context_set_stroke_color(ctx, minute_colour);
  graphics_draw_line(ctx, centre, minute_end);
  char hh[3];
  char mm[3];
  strftime(hh, sizeof(hh), twelve_hour ? "%I" : "%H", now);
  strftime(mm, sizeof(mm), "%M", now);
  digits_set_style(DIGIT_STYLE_SMALL);
  draw_dial_label(ctx, layout->hour_badge, hh, hour_colour);
  draw_dial_label(ctx, layout->minute_badge, mm, minute_colour);
  return true;
}
