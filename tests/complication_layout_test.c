#include "mdbl_complication_layout.h"
#include "mdbl.h"
#include "mdbl_digits.h"
#include "mdbl_hybrid.h"
#include "mdbl_numbered.h"
#include "mdbl_ticks.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AppState s_state;
static unsigned s_layouts;
static double s_minimum_tick_gap = 1000.0;

int32_t sin_lookup(int32_t angle) {
  return (int32_t)lround(sin(angle * 6.283185307179586 / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}
int32_t cos_lookup(int32_t angle) {
  return (int32_t)lround(cos(angle * 6.283185307179586 / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}

// The layout tests link the real hybrid/numbered/tick geometry; rendering is unused.
void graphics_context_set_stroke_color(GContext *ctx, GColor colour) {(void)ctx; (void)colour;}
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width) {(void)ctx; (void)width;}
void graphics_context_set_text_color(GContext *ctx, GColor colour) {(void)ctx; (void)colour;}
void graphics_draw_line(GContext *ctx, GPoint start, GPoint end) {(void)ctx; (void)start; (void)end;}
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t radius, GCornerMask corners) {
  (void)ctx; (void)rect; (void)radius; (void)corners;
}
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect rect,
                        GTextOverflowMode overflow, GTextAlignment alignment, void *attributes) {
  (void)ctx; (void)text; (void)font; (void)rect; (void)overflow; (void)alignment; (void)attributes;
}
GFont fonts_get_system_font(const char *key) {(void)key; return NULL;}
void digits_set_style(DigitStyle style) {(void)style;}
bool digits_draw(GContext *ctx, const char *text, GRect bounds, GColor colour, GColor outline) {
  (void)ctx; (void)text; (void)bounds; (void)colour; (void)outline;
  return true;
}

static double point_distance(GRect rect, double x, double y) {
  const double right = rect.origin.x + rect.size.w - 1;
  const double bottom = rect.origin.y + rect.size.h - 1;
  const double dx = fmax(fmax(rect.origin.x - x, x - right), 0);
  const double dy = fmax(fmax(rect.origin.y - y, y - bottom), 0);
  return hypot(dx, dy);
}

static void assert_valid(GRect rect, GRect bounds, bool round_display, GSize size,
                         GPoint hour, GPoint minute, bool hands, const GRect *avoid, size_t count) {
  assert(rect.size.w == size.w && rect.size.h == size.h);
  assert(rect.origin.x >= bounds.origin.x + 3 && rect.origin.y >= bounds.origin.y + 3);
  assert(rect.origin.x + rect.size.w <= bounds.origin.x + bounds.size.w - 3);
  assert(rect.origin.y + rect.size.h <= bounds.origin.y + bounds.size.h - 3);
  const GPoint centre = GPoint(bounds.origin.x + bounds.size.w / 2, bounds.origin.y + bounds.size.h / 2);
  assert(point_distance(rect, centre.x, centre.y) >= 8.0);
  if (round_display) {
    const double radius = fmin(bounds.size.w, bounds.size.h) / 2 - 3;
    for (unsigned corner = 0; corner < 4; corner++) {
      const double x = rect.origin.x + (corner & 1 ? rect.size.w - 1 : 0) - centre.x;
      const double y = rect.origin.y + (corner & 2 ? rect.size.h - 1 : 0) - centre.y;
      assert(hypot(x, y) <= radius);
    }
  }
  for (size_t i = 0; i < count; i++) {
    assert(rect.origin.x + rect.size.w + 3 <= avoid[i].origin.x ||
           avoid[i].origin.x + avoid[i].size.w + 3 <= rect.origin.x ||
           rect.origin.y + rect.size.h + 3 <= avoid[i].origin.y ||
           avoid[i].origin.y + avoid[i].size.h + 3 <= rect.origin.y);
  }
  if (hands) {
    const GPoint ends[] = {hour, minute};
    for (unsigned hand = 0; hand < 2; hand++) {
      for (unsigned step = 0; step <= 128; step++) {
        const double t = step / 128.0;
        const double x = centre.x + t * (ends[hand].x - centre.x);
        const double y = centre.y + t * (ends[hand].y - centre.y);
        assert(point_distance(rect, x, y) > 6.0);
      }
    }
  }
}

static void assert_tick_clearance(GRect rect, const TickMark marks[TICK_MARK_COUNT]) {
  // Check the actual stroked segments independently of their conservative
  // bounding rectangles, including both round end caps.
  for (unsigned tick = 0; tick < TICK_MARK_COUNT; tick++) {
    const double stroke_radius = marks[tick].is_hour ? 2.5 : 1.5;
    for (unsigned step = 0; step <= 32; step++) {
      const double t = step / 32.0;
      const double x = marks[tick].inner.x + t * (marks[tick].outer.x - marks[tick].inner.x);
      const double y = marks[tick].inner.y + t * (marks[tick].outer.y - marks[tick].inner.y);
      const double gap = point_distance(rect, x, y) - stroke_radius;
      assert(gap >= 3.0);
      if (gap < s_minimum_tick_gap) s_minimum_tick_gap = gap;
    }
  }
}

static void test_every_minute(void) {
  const GRect screens[] = {GRect(0, 0, 200, 228), GRect(0, 0, 260, 260)};
  for (unsigned screen = 0; screen < ARRAY_LENGTH(screens); screen++) {
    const GRect bounds = screens[screen];
    const bool round = screen == 1;
    const int dimension = bounds.size.w;
    const GPoint centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);
    const GSize cell = screen == 0 ? GSize(18, 28) : GSize(20, 32);
    const GSize sizes[] = {GSize(64, 28), screen == 0 ? GSize(34, 34) : GSize(44, 44)};
    GRect numbered[12];
    numbered_layout(bounds, round, cell, numbered);
    TickMark marks[TICK_MARK_COUNT];
    GRect tick_bounds[TICK_MARK_COUNT];
    ticks_layout(bounds, round, marks);
    for (size_t i = 0; i < TICK_MARK_COUNT; i++) {
      const int x = marks[i].inner.x < marks[i].outer.x ? marks[i].inner.x : marks[i].outer.x;
      const int y = marks[i].inner.y < marks[i].outer.y ? marks[i].inner.y : marks[i].outer.y;
      // Mirror the renderer's inputs, then verify physical strokes separately.
      tick_bounds[i] = GRect(x - 1, y - 1,
        abs(marks[i].outer.x - marks[i].inner.x) + 3,
        abs(marks[i].outer.y - marks[i].inner.y) + 3);
    }
    for (int time = 0; time < 1440; time++) {
      const struct tm now = {.tm_hour = time / 60, .tm_min = time % 60};
      const int32_t hour_angle_value = hour_angle(&now);
      const int32_t minute_angle_value = minute_angle(&now);
      HybridLayout hybrid;
      assert(calculate_hybrid_layout(bounds, hour_angle_value, minute_angle_value,
                                      round, cell, GSize(0, 0), &hybrid));
      const GRect hybrid_labels[] = {hybrid.hour_badge, hybrid.minute_badge};
      for (unsigned mode = 0; mode < 4; mode++) {
        const GRect *avoid = mode == 0 ? NULL : mode == 1 ? hybrid_labels : mode == 2 ? numbered : tick_bounds;
        const size_t avoid_count = mode == 0 ? 0 : mode == 1 ? 2 : mode == 2 ? 12 : TICK_MARK_COUNT;
        // Unclamped endpoints are more conservative than the rendered endpoints.
        const GPoint hour = point_on_ray(centre, dimension * 24 / 100, hour_angle_value);
        const GPoint minute = point_on_ray(centre, dimension * 42 / 100, minute_angle_value);
        for (unsigned size = 0; size < ARRAY_LENGTH(sizes); size++) {
          GRect result;
          if (!place_complication(bounds, round, sizes[size], hour, minute, true,
                                  avoid, avoid_count, &result)) {
            fprintf(stderr, "No date slot: platform=%u mode=%u size=%u time=%02d:%02d\n",
                    screen, mode, size, time / 60, time % 60);
            assert(false);
          }
          assert_valid(result, bounds, round, sizes[size], hour, minute, true, avoid, avoid_count);
          if (mode == 3) assert_tick_clearance(result, marks);
          s_layouts++;
          if (mode == 3) {
            // Exercise the renderer's actual clamped endpoints as well as the
            // longer conservative endpoints above, and the no-hands setting.
            const GPoint drawn_hour = dial_hand_end(centre, dimension * HOUR_HAND_LENGTH_PERCENT / 100,
                                                    hour_angle_value, avoid, avoid_count);
            const GPoint drawn_minute = dial_hand_end(centre, dimension * MINUTE_HAND_LENGTH_PERCENT / 100,
                                                      minute_angle_value, avoid, avoid_count);
            for (unsigned shown = 0; shown < 2; shown++) {
              assert(place_complication(bounds, round, sizes[size], drawn_hour, drawn_minute,
                                         shown, avoid, avoid_count, &result));
              assert_valid(result, bounds, round, sizes[size], drawn_hour, drawn_minute,
                            shown, avoid, avoid_count);
              assert_tick_clearance(result, marks);
              s_layouts++;
            }
          }
        }
      }
    }
  }
}

static void test_preferences_and_rejections(void) {
  const GRect bounds = GRect(10, 20, 200, 228);
  const GPoint centre = GPoint(110, 134);
  const GSize size = GSize(64, 28);
  GRect result;
  assert(place_complication(bounds, false, size, centre, centre, false, NULL, 0, &result));
  assert(result.origin.x == 78 && result.origin.y == 172); // Bottom, radius 52.
  assert_valid(result, bounds, false, size, centre, centre, false, NULL, 0);
  const GRect bottom = result;
  assert(place_complication(bounds, false, size, centre, centre, false, &bottom, 1, &result));
  assert(result.origin.x == 130 && result.origin.y == 120); // Next choice is right.

  const GPoint downward = GPoint(110, 218);
  assert(place_complication(bounds, false, size, centre, downward, true, NULL, 0, &result));
  assert(result.origin.x == 130 && result.origin.y == 120);
  assert_valid(result, bounds, false, size, centre, downward, true, NULL, 0);
  assert(place_complication(bounds, false, size, centre, downward, false, NULL, 0, &result));
  assert(result.origin.x == bottom.origin.x && result.origin.y == bottom.origin.y);

  const GRect sentinel = GRect(1, 2, 3, 4);
  result = sentinel;
  const GRect blocker = bounds;
  assert(!place_complication(bounds, false, size, centre, centre, false, &blocker, 1, &result));
  assert(memcmp(&result, &sentinel, sizeof(result)) == 0);
  assert(!place_complication(bounds, false, GSize(0, 28), centre, centre, false, NULL, 0, &result));
  assert(!place_complication(bounds, false, GSize(200, 228), centre, centre, false, NULL, 0, &result));
  assert(!place_complication(bounds, false, size, centre, centre, false, NULL, 1, &result));
  assert(!place_complication(bounds, false, size, centre, centre, false, NULL, 0, NULL));
}

static void test_representative_hand_directions(void) {
  const struct tm times[] = {{.tm_hour = 3, .tm_min = 15}, {.tm_hour = 6, .tm_min = 30},
                             {.tm_hour = 9, .tm_min = 45}, {.tm_hour = 10, .tm_min = 9}};
  const GRect bounds = GRect(0, 0, 200, 228);
  const GPoint centre = GPoint(100, 114);
  const GSize size = GSize(64, 28);
  for (size_t i = 0; i < ARRAY_LENGTH(times); i++) {
    const GPoint hour = point_on_ray(centre, 48, hour_angle(&times[i]));
    const GPoint minute = point_on_ray(centre, 84, minute_angle(&times[i]));
    GRect result;
    assert(place_complication(bounds, false, size, hour, minute, true, NULL, 0, &result));
    assert_valid(result, bounds, false, size, hour, minute, true, NULL, 0);
    if (i == 1) {
      assert(result.origin.x == 120 && result.origin.y == 100); // Avoid both downward hands.
    } else {
      assert(result.origin.x == 68 && result.origin.y == 152); // Clear bottom position.
    }
  }
}

int main(void) {
  test_preferences_and_rejections();
  test_representative_hand_directions();
  test_every_minute();
  printf("Complication layout tests passed: %u layouts, both displays/sizes, four dial modes, "
         "hand/label/tick clearance (minimum tick gap %.2f px)\n", s_layouts, s_minimum_tick_gap);
  return 0;
}
