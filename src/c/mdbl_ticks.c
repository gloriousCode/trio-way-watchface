#include "mdbl_ticks.h"

#include "mdbl.h"

static bool point_inside_dial(GPoint point, GRect bounds, bool round_display) {
  // The longest tick has a five-pixel black stroke. Four pixels of inset
  // keeps its round cap inside the physical display, including corner rays.
  const int16_t inset = 4;
  if (point.x < bounds.origin.x + inset || point.y < bounds.origin.y + inset ||
      point.x >= bounds.origin.x + bounds.size.w - inset ||
      point.y >= bounds.origin.y + bounds.size.h - inset) {
    return false;
  }
  if (!round_display) {
    return true;
  }
  const int16_t minimum = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  const int32_t radius = minimum / 2 - inset;
  const int32_t dx = point.x - (bounds.origin.x + bounds.size.w / 2);
  const int32_t dy = point.y - (bounds.origin.y + bounds.size.h / 2);
  return dx * dx + dy * dy <= radius * radius;
}

void ticks_layout(GRect bounds, bool round_display, TickMark marks[TICK_MARK_COUNT]) {
  if (!marks) {
    return;
  }
  const GPoint centre = GPoint(bounds.origin.x + bounds.size.w / 2,
                               bounds.origin.y + bounds.size.h / 2);
  const int16_t minimum = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  for (int index = 0; index < TICK_MARK_COUNT; index++) {
    const bool is_hour = index % 5 == 0;
    marks[index] = (TickMark){.inner = centre, .outer = centre, .is_hour = is_hour};
    if (minimum < 16) {
      continue;
    }
    const int32_t angle = index * TRIG_MAX_ANGLE / TICK_MARK_COUNT;
    int16_t low = 0;
    int16_t high = bounds.size.w + bounds.size.h;
    while (low < high) {
      const int16_t middle = low + (high - low + 1) / 2;
      if (point_inside_dial(point_on_ray(centre, middle, angle), bounds, round_display)) {
        low = middle;
      } else {
        high = middle - 1;
      }
    }
    const int16_t length = minimum * (is_hour ? 80 : 25) / 1000;
    const int16_t inner_radius = low > length ? low - length : 0;
    marks[index].inner = point_on_ray(centre, inner_radius, angle);
    marks[index].outer = point_on_ray(centre, low, angle);
  }
}

void draw_tick_dial(GContext *ctx, GRect bounds) {
  static bool cache_ready;
  static GRect cached_bounds;
  static TickMark marks[TICK_MARK_COUNT];
#if defined(PBL_PLATFORM_GABBRO)
  const bool round_display = true;
#else
  const bool round_display = false;
#endif
  if (bounds.size.w < 16 || bounds.size.h < 16) {
    return;
  }
  if (!cache_ready || !bounds_equal(bounds, cached_bounds)) {
    ticks_layout(bounds, round_display, marks);
    cached_bounds = bounds;
    cache_ready = true;
  }
  // White hour strokes and LightGray (#aaaaaa) minute strokes stay readable
  // against coloured sectors; the black outline also supports pale palettes.
  const GColor minute_colour = {.argb = 0xea};
  graphics_context_set_stroke_color(ctx, GColorBlack);
  for (int index = 0; index < TICK_MARK_COUNT; index++) {
    graphics_context_set_stroke_width(ctx, marks[index].is_hour ? 5 : 3);
    graphics_draw_line(ctx, marks[index].inner, marks[index].outer);
  }
  for (int index = 0; index < TICK_MARK_COUNT; index++) {
    graphics_context_set_stroke_color(ctx, marks[index].is_hour ? GColorWhite : minute_colour);
    graphics_context_set_stroke_width(ctx, marks[index].is_hour ? 3 : 1);
    graphics_draw_line(ctx, marks[index].inner, marks[index].outer);
  }
}
