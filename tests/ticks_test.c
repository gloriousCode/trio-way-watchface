#include "mdbl_ticks.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct LineCall {
  GPoint start;
  GPoint end;
  GColor colour;
  uint8_t width;
} LineCall;
static LineCall s_lines[120];
static unsigned s_line_count;
static unsigned s_ray_calls;
static GColor s_colour;
static uint8_t s_width;

GPoint point_on_ray(GPoint centre, int32_t radius, int32_t angle) {
  s_ray_calls++;
  const double radians = (double)angle * 6.283185307179586 / TRIG_MAX_ANGLE;
  const int32_t sine = (int32_t)lround(sin(radians) * TRIG_MAX_RATIO);
  const int32_t cosine = (int32_t)lround(cos(radians) * TRIG_MAX_RATIO);
  return GPoint(centre.x + sine * radius / TRIG_MAX_RATIO, centre.y - cosine * radius / TRIG_MAX_RATIO);
}

bool bounds_equal(GRect a, GRect b) {
  return a.origin.x == b.origin.x && a.origin.y == b.origin.y &&
         a.size.w == b.size.w && a.size.h == b.size.h;
}
void graphics_context_set_stroke_color(GContext *ctx, GColor colour) {(void)ctx; s_colour = colour;}
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width) {(void)ctx; s_width = width;}
void graphics_draw_line(GContext *ctx, GPoint start, GPoint end) {
  (void)ctx;
  assert(s_line_count < ARRAY_LENGTH(s_lines));
  s_lines[s_line_count++] = (LineCall){start, end, s_colour, s_width};
}

static bool same_point(GPoint a, GPoint b) {return a.x == b.x && a.y == b.y;}

static double distance(GPoint a, GPoint b) {
  const double x = a.x - b.x;
  const double y = a.y - b.y;
  return sqrt(x * x + y * y);
}

static void assert_stroke_inside(GPoint point, GRect bounds, bool round, double half_width) {
  assert(point.x - half_width >= bounds.origin.x);
  assert(point.y - half_width >= bounds.origin.y);
  assert(point.x + half_width < bounds.origin.x + bounds.size.w);
  assert(point.y + half_width < bounds.origin.y + bounds.size.h);
  if (round) {
    const GPoint centre = GPoint(bounds.origin.x + bounds.size.w / 2, bounds.origin.y + bounds.size.h / 2);
    assert(distance(point, centre) + half_width <= bounds.size.w / 2.0);
  }
}

static void test_layout(void) {
  for (int platform = 0; platform < 2; platform++) {
    const GRect bounds = platform ? GRect(7, 11, 260, 260) : GRect(7, 11, 200, 228);
    const GPoint centre = GPoint(bounds.origin.x + bounds.size.w / 2, bounds.origin.y + bounds.size.h / 2);
    TickMark marks[60];
    ticks_layout(bounds, platform != 0, marks);
    unsigned hours = 0;
    for (unsigned index = 0; index < ARRAY_LENGTH(marks); index++) {
      const TickMark mark = marks[index];
      assert(mark.is_hour == (index % 5 == 0));
      hours += mark.is_hour;
      const double expected_length = mark.is_hour ? (platform ? 20 : 16) : (platform ? 6 : 5);
      assert(fabs(distance(mark.inner, mark.outer) - expected_length) <= 1.5);
      assert(distance(mark.inner, centre) < distance(mark.outer, centre));
      const double half_width = mark.is_hour ? 2.5 : 1.5;
      assert_stroke_inside(mark.inner, bounds, platform != 0, half_width);
      assert_stroke_inside(mark.outer, bounds, platform != 0, half_width);
      const int32_t angle = index * TRIG_MAX_ANGLE / 60;
      bool inner_on_ray = false;
      bool outer_on_ray = false;
      for (int radius = 0; radius <= bounds.size.w + bounds.size.h; radius++) {
        const GPoint point = point_on_ray(centre, radius, angle);
        inner_on_ray |= same_point(point, mark.inner);
        outer_on_ray |= same_point(point, mark.outer);
      }
      assert(inner_on_ray && outer_on_ray);
      for (unsigned other = index + 1; other < ARRAY_LENGTH(marks); other++) {
        const double other_half_width = marks[other].is_hour ? 2.5 : 1.5;
        assert(distance(mark.outer, marks[other].outer) > half_width + other_half_width + 1);
        assert(distance(mark.inner, marks[other].inner) > half_width + other_half_width + 1);
      }
    }
    assert(hours == 12);
    assert(marks[0].inner.x == centre.x && marks[0].outer.y < centre.y);
    assert(marks[15].inner.y == centre.y && marks[15].outer.x > centre.x);
    assert(marks[30].inner.x == centre.x && marks[30].outer.y > centre.y);
    assert(marks[45].inner.y == centre.y && marks[45].outer.x < centre.x);
    TickMark repeated[60];
    ticks_layout(bounds, platform != 0, repeated);
    for (unsigned i = 0; i < ARRAY_LENGTH(marks); i++) {
      assert(same_point(repeated[i].inner, marks[i].inner));
      assert(same_point(repeated[i].outer, marks[i].outer));
    }
    ticks_layout(GRect(0, 0, bounds.size.w, bounds.size.h), platform != 0, repeated);
    for (unsigned i = 0; i < ARRAY_LENGTH(marks); i++) {
      assert(repeated[i].outer.x + 7 == marks[i].outer.x && repeated[i].outer.y + 11 == marks[i].outer.y);
    }
  }
}

static void test_render_and_cache(void) {
#if defined(PBL_PLATFORM_GABBRO)
  GRect bounds = GRect(0,0,260,260);
  const bool round = true;
#else
  GRect bounds = GRect(0,0,200,228);
  const bool round = false;
#endif
  TickMark expected[60];
  ticks_layout(bounds, round, expected);
  s_ray_calls = 0;
  s_line_count = 0;
  draw_tick_dial(NULL, bounds);
  assert(s_ray_calls > 0 && s_line_count == 120);
  for (unsigned i = 0; i < 120; i++) {
    const TickMark mark = expected[i % 60];
    assert(same_point(s_lines[i].start, mark.inner) && same_point(s_lines[i].end, mark.outer));
    if (i < 60) {
      assert(s_lines[i].colour.argb == GColorBlack.argb);
      assert(s_lines[i].width == (mark.is_hour ? 5 : 3));
    } else {
      assert(s_lines[i].colour.argb == (mark.is_hour ? GColorWhite.argb : 0xea));
      assert(s_lines[i].width == (mark.is_hour ? 3 : 1));
    }
  }
  for (unsigned redraw = 0; redraw < 100; redraw++) {
    s_ray_calls = 0;
    s_line_count = 0;
    draw_tick_dial(NULL, bounds);
    assert(s_ray_calls == 0 && s_line_count == 120);
  }
  bounds.origin.x = 3;
  s_line_count = 0;
  draw_tick_dial(NULL, bounds);
  assert(s_ray_calls > 0 && s_line_count == 120);
  assert(s_lines[0].start.x == expected[0].inner.x + 3);
  s_line_count = 0;
  draw_tick_dial(NULL, GRect(0,0,0,0));
  assert(s_line_count == 0);
  ticks_layout(bounds, round, NULL);
}

int main(void) {
  test_layout();
  test_render_and_cache();
  puts("Tick dial tests passed: 60 marks, true rays, lengths, spacing, display clearance and cached redraws");
  return 0;
}
