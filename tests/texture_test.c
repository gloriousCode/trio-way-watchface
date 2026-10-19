#include "mdbl_texture.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TextureFrame {
  uint8_t pixels[260][260];
  unsigned count;
  unsigned colour_calls;
} TextureFrame;

static TextureFrame s_actual;
static TextureFrame s_reference;
static TextureFrame *s_frame;
static GRect s_bounds;
static GColor s_colour;
static unsigned s_trig_calls;
static unsigned s_normalise_calls;
static unsigned s_compared;

int32_t sin_lookup(int32_t angle) {
  s_trig_calls++;
  return (int32_t)lround(sin(angle * 6.283185307179586 / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}
int32_t cos_lookup(int32_t angle) {
  s_trig_calls++;
  return (int32_t)lround(cos(angle * 6.283185307179586 / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}
int32_t normalise_angle(int32_t angle) {
  s_normalise_calls++;
  const int32_t wrapped = angle % TRIG_MAX_ANGLE;
  return wrapped < 0 ? wrapped + TRIG_MAX_ANGLE : wrapped;
}
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width) {(void)ctx; assert(width == 1);}
void graphics_context_set_stroke_color(GContext *ctx, GColor colour) {
  (void)ctx;
  s_colour = colour;
  s_frame->colour_calls++;
}
void graphics_draw_pixel(GContext *ctx, GPoint point) {
  (void)ctx;
  assert(point.x >= s_bounds.origin.x && point.y >= s_bounds.origin.y);
  assert(point.x < s_bounds.origin.x + s_bounds.size.w && point.y < s_bounds.origin.y + s_bounds.size.h);
  assert((s_colour.argb & 0xc0) == 0xc0);
  assert((point.x & 1) == 0 && (point.y & 1) == 0);
  uint8_t *pixel = &s_frame->pixels[point.y - s_bounds.origin.y][point.x - s_bounds.origin.x];
  assert(*pixel == 0);
  *pixel = s_colour.argb;
  s_frame->count++;
}

// The approved capture prototype is the visual oracle. This catches changes
// to facet ownership, shade levels, sample phase and the 64-pixel fade.
#include "../tools/texture_preview.inc"

static void render(TextureFrame *frame, GRect bounds, GPoint centre, const SectorRay rays[3], bool reference) {
  memset(frame, 0, sizeof(*frame));
  s_frame = frame;
  s_bounds = bounds;
  s_trig_calls = 0;
  s_normalise_calls = 0;
  if (reference) draw_texture_preview(NULL, bounds, centre, rays);
  else draw_texture(NULL, bounds, centre, rays);
}

static void compare_reference(GRect bounds, GPoint centre, const SectorRay rays[3]) {
  render(&s_reference, bounds, centre, rays, true);
  render(&s_actual, bounds, centre, rays, false);
  assert(s_trig_calls == 6 && s_normalise_calls == 3);
  assert(s_actual.count == s_reference.count);
  assert(memcmp(s_actual.pixels, s_reference.pixels, sizeof(s_actual.pixels)) == 0);
  assert(s_actual.colour_calls <= s_reference.colour_calls);
  assert(s_actual.count <= (unsigned)((bounds.size.w + 1) / 2) * (unsigned)((bounds.size.h + 1) / 2));
  s_compared++;
}

static void test_approved_output(void) {
  const int32_t angles[][3] = {
    {0, 16384, 32768}, {0, 0, 32768}, {0, 4096, 8192},
    {0, 32768, 32768}, {16384, 32768, 49152}, {2000, 30000, 60000}, {0, 0, 0}
  };
  const GRect bounds[] = {GRect(0,0,200,228), GRect(0,0,260,260), GRect(-3,-1,73,87), GRect(7,11,79,91)};
  for (size_t screen = 0; screen < ARRAY_LENGTH(bounds); screen++) {
    const GRect area = bounds[screen];
    const GPoint centre = GPoint(area.origin.x + area.size.w / 2, area.origin.y + area.size.h / 2);
    for (size_t fixture = 0; fixture < ARRAY_LENGTH(angles); fixture++) {
      const SectorRay rays[] = {
        {.angle = angles[fixture][0], .colour = {.argb = 0xff}},
        {.angle = angles[fixture][1], .colour = {.argb = 0xc0}},
        {.angle = angles[fixture][2], .colour = {.argb = 0xda}}
      };
      compare_reference(area, centre, rays);
      // A second invocation must reproduce the same stipple with no temporal noise.
      render(&s_actual, area, centre, rays, false);
      assert(memcmp(s_actual.pixels, s_reference.pixels, sizeof(s_actual.pixels)) == 0);
    }
  }
}

static void test_facets_and_fade(void) {
  const SectorRay rays[] = {
    {.angle = 0, .colour = {.argb = 0xff}},
    {.angle = TRIG_MAX_ANGLE / 4, .colour = {.argb = 0xc0}},
    {.angle = TRIG_MAX_ANGLE / 2, .colour = {.argb = 0xfc}}
  };
  render(&s_actual, GRect(0,0,200,228), GPoint(100,114), rays, false);
  assert(s_actual.pixels[100][104] == 0xea); // White darkens one native palette step.
  assert(s_actual.pixels[120][104] == 0xd5); // Black lightens one step.
  assert(s_actual.pixels[120][96] == 0xfd);  // Saturated red/green cap at three.
  assert(s_actual.pixels[100][164] == 0);    // Fade reaches zero at distance 64.
  assert(s_actual.pixels[120][42] == 0);     // Bayer level excludes a distant sample.
  assert(s_actual.count > 0 && s_actual.count < 200 * 228 / 4);
}

static void test_every_palette_colour(void) {
  for (unsigned colour = 0; colour < 64; colour++) {
    const SectorRay rays[] = {
      {.angle = 0, .colour = {.argb = 0xc0 | colour}},
      {.angle = TRIG_MAX_ANGLE / 4, .colour = {.argb = 0xc0 | colour}},
      {.angle = TRIG_MAX_ANGLE / 2, .colour = {.argb = 0xc0 | colour}}
    };
    compare_reference(GRect(0,0,16,16), GPoint(8,8), rays);
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        const uint8_t pixel = s_actual.pixels[y][x];
        if (!pixel) continue;
        for (unsigned shift = 0; shift <= 4; shift += 2) {
          const int difference = ((pixel >> shift) & 3) - (int)((colour >> shift) & 3);
          assert(abs(difference) <= 1);
        }
      }
    }
  }
}

static void test_empty_bounds(void) {
  const SectorRay rays[3] = {0};
  render(&s_actual, GRect(0,0,0,0), GPoint(0,0), rays, false);
  assert(s_actual.count == 0 && s_trig_calls == 0 && s_normalise_calls == 0);
}

int main(void) {
  test_approved_output();
  test_facets_and_fade();
  test_every_palette_colour();
  test_empty_bounds();
  printf("Texture tests passed: %u exact prototype comparisons, palette caps, facet ownership, fade and stable samples\n", s_compared);
  return 0;
}
