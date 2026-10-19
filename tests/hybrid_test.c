#include "mdbl_hybrid.h"
#include "mdbl_digits.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static char s_values[2][3];
static unsigned s_digit_calls;
static unsigned s_fill_calls;
static GPoint s_hand_ends[4];
static unsigned s_line_calls;

int32_t normalise_angle(int32_t angle) {
  const int32_t value = angle % TRIG_MAX_ANGLE;
  return value < 0 ? value + TRIG_MAX_ANGLE : value;
}

GPoint point_on_ray(GPoint centre, int32_t radius, int32_t angle) {
  const double radians = (double)angle * 6.283185307179586 / TRIG_MAX_ANGLE;
  // Match the SDK lookup's fixed-point and the production integer truncation.
  const int32_t sine = (int32_t)lround(sin(radians) * 65535);
  const int32_t cosine = (int32_t)lround(cos(radians) * 65535);
  return GPoint(centre.x + sine * radius / 65535, centre.y - cosine * radius / 65535);
}

GRect grect_inset(GRect rect, GEdgeInsets insets) {
  return GRect(rect.origin.x + insets.left, rect.origin.y + insets.top,
               rect.size.w - insets.left - insets.right, rect.size.h - insets.top - insets.bottom);
}
void graphics_context_set_fill_color(GContext *ctx, GColor colour) {(void)ctx; (void)colour;}
void graphics_context_set_stroke_color(GContext *ctx, GColor colour) {(void)ctx; (void)colour;}
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width) {(void)ctx; assert(width >= 4);}
void graphics_context_set_text_color(GContext *ctx, GColor colour) {(void)ctx; (void)colour;}
void graphics_draw_line(GContext *ctx, GPoint start, GPoint end) {
  (void)ctx; (void)start;
  assert(s_line_calls < 4);
  s_hand_ends[s_line_calls++] = end;
}
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t radius, GCornerMask corners) {
  (void)ctx; (void)rect; (void)radius; (void)corners;
  s_fill_calls++;
}
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect rect,
                        GTextOverflowMode overflow, GTextAlignment alignment, void *attributes) {
  (void)ctx; (void)text; (void)font; (void)rect; (void)overflow; (void)alignment; (void)attributes;
}
GFont fonts_get_system_font(const char *key) {(void)key; return NULL;}
void digits_set_style(DigitStyle style) {assert(style == DIGIT_STYLE_SMALL);}
bool digits_draw(GContext *ctx, const char *text, GRect bounds, GColor colour, GColor outline_colour) {
  (void)ctx; (void)bounds;
  assert(outline_colour.argb == (colour.argb == GColorWhite.argb ? GColorBlack.argb : GColorWhite.argb));
  assert(s_digit_calls < 2 && strlen(text) == 2);
  strcpy(s_values[s_digit_calls++], text);
  return true;
}

static bool separated(GRect a, GRect b) {
  return a.origin.x + a.size.w + 2 <= b.origin.x || b.origin.x + b.size.w + 2 <= a.origin.x ||
         a.origin.y + a.size.h + 2 <= b.origin.y || b.origin.y + b.size.h + 2 <= a.origin.y;
}

static void check_bounds(GRect rect, GRect bounds, bool round) {
  assert(rect.origin.x >= bounds.origin.x + 3 && rect.origin.y >= bounds.origin.y + 3);
  assert(rect.origin.x + rect.size.w <= bounds.origin.x + bounds.size.w - 3);
  assert(rect.origin.y + rect.size.h <= bounds.origin.y + bounds.size.h - 3);
  const int cx = bounds.origin.x + bounds.size.w / 2;
  const int cy = bounds.origin.y + bounds.size.h / 2;
  if (round) {
    for (int corner = 0; corner < 4; corner++) {
      const int dx = rect.origin.x + (corner & 1 ? rect.size.w - 1 : 0) - cx;
      const int dy = rect.origin.y + (corner & 2 ? rect.size.h - 1 : 0) - cy;
      assert(dx * dx + dy * dy <= 127 * 127);
    }
  }
  for (int y = rect.origin.y; y < rect.origin.y + rect.size.h; y++) {
    for (int x = rect.origin.x; x < rect.origin.x + rect.size.w; x++) {
      assert((x - cx) * (x - cx) + (y - cy) * (y - cy) >= 8 * 8);
    }
  }
}

static void test_every_minute(void) {
  unsigned cases = 0;
  unsigned adjusted_labels = 0;
  for (int platform = 0; platform < 2; platform++) {
    const GRect bounds = platform ? GRect(7, 11, 260, 260) : GRect(7, 11, 200, 228);
    const GSize cell = platform ? GSize(20, 32) : GSize(18, 28);
    const GSize dates[] = {GSize(0, 0), GSize(platform ? 44 : 34, platform ? 44 : 34), GSize(64, 28)};
    for (int minute = 0; minute < 24 * 60; minute++) {
      const int hour_angle = ((minute / 60) % 12) * TRIG_MAX_ANGLE / 12 +
        (minute % 60) * TRIG_MAX_ANGLE / 720;
      const int minute_angle = (minute % 60) * TRIG_MAX_ANGLE / 60;
      for (size_t date = 0; date < ARRAY_LENGTH(dates); date++) {
        HybridLayout layout;
        if (!calculate_hybrid_layout(bounds, hour_angle, minute_angle, platform != 0, cell, dates[date], &layout)) {
          fprintf(stderr, "No hybrid layout: platform%d %02d:%02d\n", platform, minute / 60, minute % 60);
          assert(false);
        }
        check_bounds(layout.hour_badge, bounds, platform != 0);
        check_bounds(layout.minute_badge, bounds, platform != 0);
        assert(separated(layout.hour_badge, layout.minute_badge));
        assert(layout.hour_badge.size.w == 2 * cell.w && layout.hour_badge.size.h == cell.h);
        const GPoint centre = GPoint(bounds.origin.x + bounds.size.w / 2, bounds.origin.y + bounds.size.h / 2);
        const GRect badges[] = {layout.hour_badge, layout.minute_badge};
        const int32_t angles[] = {hour_angle, minute_angle};
        for (size_t hand = 0; hand < ARRAY_LENGTH(badges); hand++) {
          const GPoint endpoint = GPoint(badges[hand].origin.x + badges[hand].size.w / 2,
                                          badges[hand].origin.y + badges[hand].size.h / 2);
          bool on_ray = false;
          for (int radius = 0; radius <= bounds.size.w + bounds.size.h; radius++) {
            const GPoint expected = point_on_ray(centre, radius, angles[hand]);
            if (expected.x == endpoint.x && expected.y == endpoint.y) {
              on_ray = true;
              break;
            }
          }
          int separation = normalise_angle(minute_angle - hour_angle);
          if (separation > TRIG_MAX_ANGLE / 2) separation = TRIG_MAX_ANGLE - separation;
          if (separation >= TRIG_MAX_ANGLE / 6) assert(on_ray);
          if (!on_ray) {
            adjusted_labels++;
          }
          // Both labels must remain on the perimeter, even when the hands
          // coincide. Check edge proximity independently of placement code.
          const GRect label = badges[hand];
          if (platform) {
            int maximum_squared = 0;
            for (int corner = 0; corner < 4; corner++) {
              const int dx = label.origin.x + (corner & 1 ? label.size.w - 1 : 0) - centre.x;
              const int dy = label.origin.y + (corner & 2 ? label.size.h - 1 : 0) - centre.y;
              const int squared = dx * dx + dy * dy;
              if (squared > maximum_squared) maximum_squared = squared;
            }
            assert(maximum_squared >= 125 * 125);
          } else {
            assert(label.origin.x <= bounds.origin.x + 5 ||
                   label.origin.y <= bounds.origin.y + 5 ||
                   label.origin.x + label.size.w >= bounds.origin.x + bounds.size.w - 5 ||
                   label.origin.y + label.size.h >= bounds.origin.y + bounds.size.h - 5);
          }
          const double actual = atan2(endpoint.x - centre.x, centre.y - endpoint.y);
          const double expected = (double)angles[hand] * 6.283185307179586 / TRIG_MAX_ANGLE;
          const double difference = remainder(actual - expected, 6.283185307179586);
          assert(fabs(difference) < 31 * 6.283185307179586 / 360);
        }
        assert(layout.has_date_space == (date != 0));
        if (date) {
          check_bounds(layout.date_badge, bounds, platform != 0);
          assert(separated(layout.date_badge, layout.hour_badge));
          assert(separated(layout.date_badge, layout.minute_badge));
        }
        cases++;
      }
    }
  }
  printf("Hybrid layouts passed: %u cases, both platforms, all minutes and date options\n", cases);
  printf("Hybrid rim labels separated along the perimeter: %u\n", adjusted_labels);
  assert(adjusted_labels > 0);
}

static void test_invalid_bounds(void) {
  HybridLayout layout;
  assert(!calculate_hybrid_layout(GRect(0,0,200,228),0,0,false,GSize(18,28),GSize(0,0),NULL));
  assert(!calculate_hybrid_layout(GRect(0,0,200,228),0,0,false,GSize(0,28),GSize(0,0),&layout));
  assert(!calculate_hybrid_layout(GRect(0,0,0,228),0,0,false,GSize(18,28),GSize(0,0),&layout));
  assert(!calculate_hybrid_layout(GRect(0,0,20,20),0,0,false,GSize(18,28),GSize(0,0),&layout));
}

static void test_hour_and_minute_values(void) {
  const int times[][2] = {{13,37}, {0,0}, {23,59}, {10,10}, {12,0}, {11,59}, {0,1}};
  for (unsigned format = 0; format < 2; format++) {
    for (size_t i = 0; i < ARRAY_LENGTH(times); i++) {
      const struct tm now = {.tm_hour = times[i][0], .tm_min = times[i][1]};
      const int angles[] = {(now.tm_hour % 12) * TRIG_MAX_ANGLE / 12 + now.tm_min * TRIG_MAX_ANGLE / 720,
                            now.tm_min * TRIG_MAX_ANGLE / 60};
      HybridLayout layout;
      s_digit_calls = s_line_calls = s_fill_calls = 0;
      const GPoint centre = GPoint(100,114);
      assert(draw_hybrid(NULL,GRect(0,0,200,228),centre,200,angles[0],angles[1],
                         &now,format != 0,GColorWhite,GColorBlack,GSize(34,34),&layout));
      char hh[3], mm[3];
      const unsigned displayed_hour = format ? (now.tm_hour % 12 ? now.tm_hour % 12 : 12) : now.tm_hour;
      snprintf(hh, sizeof(hh), "%02u", displayed_hour);
      strftime(mm, sizeof(mm), "%M", &now);
      assert(layout.has_date_space && s_fill_calls == 0);
      assert(s_digit_calls == 2 && strcmp(s_values[0],hh) == 0 && strcmp(s_values[1],mm) == 0);
      assert(s_line_calls == 4);
      for (int hand = 0; hand < 2; hand++) {
        bool on_ray = false;
        for (int radius = 1; radius <= (hand ? 84 : 48); radius++) {
          const GPoint expected = point_on_ray(centre, radius, angles[hand]);
          if (expected.x == s_hand_ends[hand].x && expected.y == s_hand_ends[hand].y) on_ray = true;
        }
        assert(on_ray);
        assert(s_hand_ends[hand].x == s_hand_ends[hand + 2].x &&
               s_hand_ends[hand].y == s_hand_ends[hand + 2].y);
        const GRect labels[] = {layout.hour_badge, layout.minute_badge};
        for (size_t label = 0; label < ARRAY_LENGTH(labels); label++) {
          assert(s_hand_ends[hand].x < labels[label].origin.x - 5 ||
                 s_hand_ends[hand].x >= labels[label].origin.x + labels[label].size.w + 5 ||
                 s_hand_ends[hand].y < labels[label].origin.y - 5 ||
                 s_hand_ends[hand].y >= labels[label].origin.y + labels[label].size.h + 5);
        }
      }
    }
  }
}

int main(void) {
  test_every_minute();
  test_invalid_bounds();
  test_hour_and_minute_values();
  return 0;
}
