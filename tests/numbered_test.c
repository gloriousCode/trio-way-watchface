#include "mdbl_numbered.h"
#include "mdbl.h"
#include "mdbl_digits.h"
#include "mdbl_hybrid.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct LabelCall {
  char text[3];
  GRect bounds;
  GColor colour;
  GColor outline;
} LabelCall;
typedef struct LineCall {
  GPoint start;
  GPoint end;
  uint8_t width;
  GColor colour;
} LineCall;
static LabelCall s_labels[12];
static LineCall s_lines[2];
static unsigned s_label_count;
static unsigned s_line_count;
static unsigned s_fill_count;
static unsigned s_style_count;
static uint8_t s_stroke_width;
static GColor s_stroke_colour;

int32_t normalise_angle(int32_t angle) {
  const int32_t value = angle % TRIG_MAX_ANGLE;
  return value < 0 ? value + TRIG_MAX_ANGLE : value;
}

GPoint point_on_ray(GPoint centre, int32_t radius, int32_t angle) {
  const double radians = (double)angle * 6.283185307179586 / TRIG_MAX_ANGLE;
  const int32_t sine = (int32_t)lround(sin(radians) * TRIG_MAX_RATIO);
  const int32_t cosine = (int32_t)lround(cos(radians) * TRIG_MAX_RATIO);
  return GPoint(centre.x + sine * radius / TRIG_MAX_RATIO, centre.y - cosine * radius / TRIG_MAX_RATIO);
}

void graphics_context_set_stroke_color(GContext *ctx, GColor colour) {(void)ctx; s_stroke_colour = colour;}
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width) {(void)ctx; s_stroke_width = width;}
void graphics_context_set_text_color(GContext *ctx, GColor colour) {(void)ctx; (void)colour;}
void graphics_draw_line(GContext *ctx, GPoint start, GPoint end) {
  (void)ctx;
  assert(s_line_count < ARRAY_LENGTH(s_lines));
  s_lines[s_line_count++] = (LineCall){start, end, s_stroke_width, s_stroke_colour};
}
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t radius, GCornerMask corners) {
  (void)ctx; (void)rect; (void)radius; (void)corners;
  s_fill_count++;
}
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect rect,
                        GTextOverflowMode overflow, GTextAlignment alignment, void *attributes) {
  (void)ctx; (void)text; (void)font; (void)rect; (void)overflow; (void)alignment; (void)attributes;
  assert(!"Native digit assets unexpectedly fell back to text");
}
GFont fonts_get_system_font(const char *key) {(void)key; return NULL;}
void digits_set_style(DigitStyle style) {assert(style == DIGIT_STYLE_SMALL); s_style_count++;}
bool digits_draw(GContext *ctx, const char *text, GRect bounds, GColor colour, GColor outline_colour) {
  (void)ctx;
  assert(s_label_count < ARRAY_LENGTH(s_labels));
  assert(strlen(text) > 0 && strlen(text) <= 2);
  LabelCall *call = &s_labels[s_label_count++];
  strcpy(call->text, text);
  call->bounds = bounds;
  call->colour = colour;
  call->outline = outline_colour;
  return true;
}

static void reset_calls(void) {
  s_label_count = 0;
  s_line_count = 0;
  s_fill_count = 0;
  s_style_count = 0;
}

static bool same_rect(GRect a, GRect b) {
  return a.origin.x == b.origin.x && a.origin.y == b.origin.y &&
         a.size.w == b.size.w && a.size.h == b.size.h;
}

static bool separated(GRect a, GRect b) {
  return a.origin.x + a.size.w + 2 <= b.origin.x || b.origin.x + b.size.w + 2 <= a.origin.x ||
         a.origin.y + a.size.h + 2 <= b.origin.y || b.origin.y + b.size.h + 2 <= a.origin.y;
}

static void check_bounds(GRect rect, GRect bounds, bool round) {
  assert(rect.origin.x >= bounds.origin.x + 3 && rect.origin.y >= bounds.origin.y + 3);
  assert(rect.origin.x + rect.size.w <= bounds.origin.x + bounds.size.w - 3);
  assert(rect.origin.y + rect.size.h <= bounds.origin.y + bounds.size.h - 3);
  if (round) {
    const int cx = bounds.origin.x + bounds.size.w / 2;
    const int cy = bounds.origin.y + bounds.size.h / 2;
    const int radius = bounds.size.w / 2 - 3;
    for (int corner = 0; corner < 4; corner++) {
      const int dx = rect.origin.x + (corner & 1 ? rect.size.w - 1 : 0) - cx;
      const int dy = rect.origin.y + (corner & 2 ? rect.size.h - 1 : 0) - cy;
      assert(dx * dx + dy * dy <= radius * radius);
    }
  }
}

static unsigned closest_minute_marker(unsigned minute) {
  unsigned closest = 0;
  unsigned minimum = 60;
  for (unsigned marker = 0; marker < 12; marker++) {
    unsigned distance = (minute + 60 - marker * 5) % 60;
    if (distance > 30) distance = 60 - distance;
    if (distance < minimum) {
      closest = marker;
      minimum = distance;
    }
  }
  return closest == 0 ? 12 : closest;
}

static void test_highlights_every_minute(void) {
  for (int hour = 0; hour < 24; hour++) {
    for (int minute = 0; minute < 60; minute++) {
      const struct tm now = {.tm_hour = hour, .tm_min = minute};
      const DialHighlights selected = numbered_highlights(&now);
      const unsigned expected_hour = hour == 0 ? 12 : hour > 12 ? hour - 12 : hour;
      assert(selected.hour == expected_hour);
      assert(selected.minute == closest_minute_marker((unsigned)minute));
    }
  }
  const struct tm now = {.tm_hour = 13, .tm_min = 37};
  const DialHighlights selected = numbered_highlights(&now);
  assert(selected.hour == 1 && selected.minute == 7);
}

static void test_layout_and_date_clearance(void) {
  for (int platform = 0; platform < 2; platform++) {
    const GRect bounds = platform ? GRect(0, 0, 260, 260) : GRect(0, 0, 200, 228);
    const GSize cell = platform ? GSize(20, 32) : GSize(18, 28);
    GRect labels[12];
    numbered_layout(bounds, platform != 0, cell, labels);
    const GPoint centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);
    const int minimum = bounds.size.w;
    const int side = minimum * 17 / 100;
    const GRect dates[] = {
      GRect(centre.x + minimum * 28 / 100 - side / 2, centre.y - side / 2, side, side),
      GRect(centre.x - 32, centre.y + minimum / 5, 64, 28)
    };
    for (unsigned i = 0; i < 12; i++) {
      check_bounds(labels[i], bounds, platform != 0);
      assert(labels[i].size.w == cell.w * (i >= 9 ? 2 : 1));
      GRect text = labels[i];
      text.size.h = cell.h;
      const GRect marker = GRect(text.origin.x + text.size.w / 2 - 7, text.origin.y + cell.h, 15, 5);
      check_bounds(marker, bounds, platform != 0);
      for (unsigned j = i + 1; j < 12; j++) {
        GRect other_text = labels[j];
        other_text.size.h = cell.h;
        if (!separated(text, other_text)) {
          fprintf(stderr, "Numbered labels collide: platform%d %u/%u: (%d,%d %dx%d) (%d,%d %dx%d)\n",
                   platform, i + 1, j + 1, labels[i].origin.x, labels[i].origin.y, labels[i].size.w,
                   labels[i].size.h, labels[j].origin.x, labels[j].origin.y, labels[j].size.w, labels[j].size.h);
          assert(false);
        }
      }
      for (unsigned j = 0; j < 12; j++) {
        if (i == j) continue;
        GRect other_text = labels[j];
        other_text.size.h = cell.h;
        assert(separated(marker, other_text));
      }
      for (size_t date = 0; date < ARRAY_LENGTH(dates); date++) {
        check_bounds(dates[date], bounds, platform != 0);
        assert(separated(labels[i], dates[date]));
      }
    }
    GRect shifted[12];
    numbered_layout(GRect(7, 11, bounds.size.w, bounds.size.h), platform != 0, cell, shifted);
    for (unsigned i = 0; i < 12; i++) {
      GRect expected = labels[i];
      expected.origin.x += 7;
      expected.origin.y += 11;
      assert(same_rect(expected, shifted[i]));
    }
  }
}

static void test_render_every_minute(void) {
#if defined(PBL_PLATFORM_GABBRO)
  const GRect bounds = GRect(0, 0, 260, 260);
  const GSize cell = GSize(20, 32);
  const bool round = true;
#else
  const GRect bounds = GRect(0, 0, 200, 228);
  const GSize cell = GSize(18, 28);
  const bool round = false;
#endif
  GRect labels[12];
  numbered_layout(bounds, round, cell, labels);
  const GColor dark = {.argb = 0xc5};
  const GColor light = {.argb = 0xfe};
  unsigned shared = 0;
  for (int time = 0; time < 24 * 60; time++) {
    const struct tm now = {.tm_hour = time / 60, .tm_min = time % 60};
    const DialHighlights selected = numbered_highlights(&now);
    reset_calls();
    draw_numbered_dial(NULL, bounds, &now, dark, light);
    assert(s_label_count == 12 && s_style_count == 1 && s_fill_count == 0);
    for (unsigned number = 1; number <= 12; number++) {
      const LabelCall call = s_labels[number - 1];
      char expected_text[3];
      snprintf(expected_text, sizeof(expected_text), "%u", number);
      assert(strcmp(call.text, expected_text) == 0);
      const GColor expected_colour = number == selected.hour ? dark : number == selected.minute ? light : GColorWhite;
      assert(call.colour.argb == expected_colour.argb);
      assert(call.outline.argb == (number == selected.hour ? GColorWhite.argb : GColorBlack.argb));
      GRect expected_bounds = labels[number - 1];
      expected_bounds.size.h = cell.h;
      assert(same_rect(call.bounds, expected_bounds)); // Every time retains fixed numeral positions.
    }
    assert(s_line_count == (selected.hour == selected.minute ? 2u : 0u));
    if (s_line_count) {
      shared++;
      assert(s_lines[0].width == 4 && s_lines[0].colour.argb == GColorBlack.argb);
      assert(s_lines[1].width == 2 && s_lines[1].colour.argb == light.argb);
      assert(s_lines[0].start.x == s_lines[1].start.x && s_lines[0].start.y == s_lines[1].start.y);
      assert(s_lines[0].end.x == s_lines[1].end.x && s_lines[0].end.y == s_lines[1].end.y);
      const GRect label = labels[selected.hour - 1];
      assert(s_lines[0].start.x - 2 >= label.origin.x);
      assert(s_lines[0].end.x + 2 < label.origin.x + label.size.w);
      assert(s_lines[0].start.y + 2 < label.origin.y + label.size.h);
    }
  }
  assert(shared > 0);
}

static bool inside_label_clearance(GPoint point, const GRect labels[12]) {
  for (unsigned i = 0; i < 12; i++) {
    const GRect label = labels[i];
    if (point.x >= label.origin.x - 5 && point.x < label.origin.x + label.size.w + 5 &&
        point.y >= label.origin.y - 5 && point.y < label.origin.y + label.size.h + 5) {
      return true;
    }
  }
  return false;
}

static void test_hand_clearance_every_minute(void) {
  unsigned shortened = 0;
  for (int platform = 0; platform < 2; platform++) {
    const GRect bounds = platform ? GRect(0, 0, 260, 260) : GRect(0, 0, 200, 228);
    const GSize cell = platform ? GSize(20, 32) : GSize(18, 28);
    const GPoint centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);
    GRect labels[12];
    numbered_layout(bounds, platform != 0, cell, labels);
    for (int time = 0; time < 24 * 60; time++) {
      const int32_t angles[] = {
        ((time / 60) % 12) * TRIG_MAX_ANGLE / 12 + (time % 60) * TRIG_MAX_ANGLE / 720,
        (time % 60) * TRIG_MAX_ANGLE / 60
      };
      const int lengths[] = {bounds.size.w * HOUR_HAND_LENGTH_PERCENT / 100, bounds.size.w * MINUTE_HAND_LENGTH_PERCENT / 100};
      for (unsigned hand = 0; hand < 2; hand++) {
        const GPoint end = dial_hand_end(centre, lengths[hand], angles[hand], labels, ARRAY_LENGTH(labels));
        int last_radius = -1;
        for (int radius = 0; radius <= lengths[hand]; radius++) {
          const GPoint point = point_on_ray(centre, radius, angles[hand]);
          if (point.x == end.x && point.y == end.y) last_radius = radius;
        }
        assert(last_radius > 0); // Remains on the exact original hand ray.
        for (int radius = 0; radius <= last_radius; radius++) {
          assert(!inside_label_clearance(point_on_ray(centre, radius, angles[hand]), labels));
        }
        if (last_radius < lengths[hand]) {
          shortened++;
          assert(inside_label_clearance(point_on_ray(centre, last_radius + 1, angles[hand]), labels));
        }
      }
    }
  }
  assert(shortened > 0); // Exercise collisions with wide double-digit numerals.
}

int main(void) {
  test_highlights_every_minute();
  test_layout_and_date_clearance();
  test_render_every_minute();
  test_hand_clearance_every_minute();
  puts("Numbered dial tests passed: all minutes, fixed positions, 12 labels, highlights, outlines and date clearance");
  return 0;
}
