#include "mdbl_digits.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct GContext { GCompOp mode; };
struct GBitmap {
  bool live;
  unsigned children;
  uint32_t resource_id;
  GBitmapFormat format;
  GRect bounds;
  GColor palette[4];
  GBitmap *parent;
};

typedef struct DrawCall {
  uint32_t resource_id;
  GRect source;
  GRect destination;
  GColor palette[4];
} DrawCall;

static GBitmap s_bitmaps[32];
static DrawCall s_draws[16];
static unsigned s_create_count;
static unsigned s_destroy_count;
static unsigned s_live_count;
static unsigned s_draw_count;
static unsigned s_fail_create;
static uint32_t s_bad_format_resource;
static unsigned s_bad_dimensions;
static GContext s_context;

static GBitmap *allocate_bitmap(void) {
  s_create_count++;
  if (s_create_count == s_fail_create) {
    return NULL;
  }
  for (size_t i = 0; i < ARRAY_LENGTH(s_bitmaps); i++) {
    if (!s_bitmaps[i].live) {
      memset(&s_bitmaps[i], 0, sizeof(s_bitmaps[i]));
      s_bitmaps[i].live = true;
      s_live_count++;
      return &s_bitmaps[i];
    }
  }
  assert(!"Unexpected bitmap allocation growth");
  return NULL;
}

GBitmap *gbitmap_create_with_resource(uint32_t resource_id) {
  assert(resource_id >= RESOURCE_ID_DIGITS_SMALL_FILL && resource_id <= RESOURCE_ID_DIGITS_LARGE_OUTLINE);
  GBitmap *bitmap = allocate_bitmap();
  if (!bitmap) {
    return NULL;
  }
  bitmap->resource_id = resource_id;
  bitmap->format = resource_id == s_bad_format_resource ? GBitmapFormat8Bit : GBitmapFormat2BitPalette;
  bool large = resource_id >= RESOURCE_ID_DIGITS_LARGE_FILL;
  bitmap->bounds = GRect(0, 0, large ? 380 : 180, large ? 64 : 28);
  // Deliberately scramble coverage order to model SDK palette optimisation.
  const uint8_t alpha[] = {0x80, 0x00, 0xc0, 0x40};
  for (size_t i = 0; i < 4; i++) {
    bitmap->palette[i].argb = alpha[i] | 0x3f;
  }
  if (resource_id == RESOURCE_ID_DIGITS_SMALL_FILL) {
    if (s_bad_dimensions == 1) bitmap->bounds.size.w = 0;
    if (s_bad_dimensions == 2) bitmap->bounds.size.w = 179;
    if (s_bad_dimensions == 3) bitmap->bounds.size.w = 190;
    if (s_bad_dimensions == 4) bitmap->bounds.size.h = 29;
  }
  return bitmap;
}

GBitmap *gbitmap_create_as_sub_bitmap(const GBitmap *base_bitmap, GRect bounds) {
  assert(base_bitmap->live && !base_bitmap->parent);
  GBitmap *bitmap = allocate_bitmap();
  if (bitmap) {
    bitmap->parent = (GBitmap *)base_bitmap;
    bitmap->parent->children++;
    bitmap->bounds = bounds;
    bitmap->resource_id = base_bitmap->resource_id;
  }
  return bitmap;
}

void gbitmap_destroy(GBitmap *bitmap) {
  assert(bitmap && bitmap->live && bitmap->children == 0);
  if (bitmap->parent) {
    assert(bitmap->parent->live && bitmap->parent->children > 0);
    bitmap->parent->children--;
  }
  bitmap->live = false;
  s_live_count--;
  s_destroy_count++;
}

GBitmapFormat gbitmap_get_format(const GBitmap *bitmap) {
  assert(bitmap->live);
  return bitmap->format;
}

GRect gbitmap_get_bounds(const GBitmap *bitmap) {
  assert(bitmap->live);
  return bitmap->bounds;
}

void gbitmap_set_bounds(GBitmap *bitmap, GRect bounds) {
  assert(bitmap->live && bitmap->parent && bitmap->parent->live);
  assert(bounds.origin.x >= 0 && bounds.origin.y >= 0);
  assert(bounds.origin.x + bounds.size.w <= bitmap->parent->bounds.size.w);
  assert(bounds.origin.y + bounds.size.h <= bitmap->parent->bounds.size.h);
  bitmap->bounds = bounds;
}

GColor *gbitmap_get_palette(const GBitmap *bitmap) {
  assert(bitmap->live);
  return (GColor *)(bitmap->parent ? bitmap->parent->palette : bitmap->palette);
}

void graphics_context_set_compositing_mode(GContext *ctx, GCompOp mode) {
  ctx->mode = mode;
}

void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *bitmap, GRect bounds) {
  assert(ctx->mode == GCompOpSet);
  assert(bitmap->live && bitmap->parent && bitmap->parent->live);
  assert(s_draw_count < ARRAY_LENGTH(s_draws));
  DrawCall *call = &s_draws[s_draw_count++];
  call->resource_id = bitmap->resource_id;
  call->source = bitmap->bounds;
  call->destination = bounds;
  memcpy(call->palette, gbitmap_get_palette(bitmap), sizeof(call->palette));
}

static void reset(void) {
  digits_release();
  assert(s_live_count == 0);
  s_create_count = 0;
  s_destroy_count = 0;
  s_draw_count = 0;
  s_fail_create = 0;
  s_bad_format_resource = 0;
  s_bad_dimensions = 0;
  s_context.mode = GCompOpAssign;
}

static bool draw(const char *text, GRect bounds, uint8_t colour) {
  s_draw_count = 0;
  return digits_draw(&s_context, text, bounds, (GColor){.argb = colour}, GColorBlack);
}

static void test_lazy_load_and_mode_switch(void) {
  reset();
  assert(!draw("10", GRect(0, 0, 200, 100), 0xff));
  digits_set_style(DIGIT_STYLE_SMALL);
  assert(s_create_count == 0);
  assert(draw("10", GRect(0, 0, 200, 100), 0xff));
  assert(s_create_count == 4 && s_live_count == 4);
  for (unsigned i = 0; i < 100; i++) {
    digits_set_style(DIGIT_STYLE_SMALL);
    assert(draw("08", GRect(0, 0, 200, 100), 0xd5));
  }
  assert(s_create_count == 4 && s_destroy_count == 0);
  digits_set_style(DIGIT_STYLE_LARGE);
  assert(s_live_count == 0 && s_destroy_count == 4 && s_create_count == 4);
  assert(draw("23", GRect(0, 0, 200, 100), 0xff));
  assert(s_create_count == 8 && s_live_count == 4);
  assert(s_draws[0].resource_id == RESOURCE_ID_DIGITS_LARGE_OUTLINE);
  assert(s_draws[0].destination.size.w == 38 && s_draws[0].destination.size.h == 64);
  digits_set_style(DIGIT_STYLE_NONE);
  assert(s_live_count == 0 && s_destroy_count == 8);
  assert(!draw("23", GRect(0, 0, 200, 100), 0xff));
  digits_release();
  assert(s_destroy_count == 8);
}

static void test_tint_and_layout(void) {
  reset();
  digits_set_style(DIGIT_STYLE_SMALL);
  assert(draw("09", GRect(10, 20, 101, 51), 0xea));
  assert(s_draw_count == 4 && s_context.mode == GCompOpAssign);
  const uint8_t alpha[] = {0x80, 0x00, 0xc0, 0x40};
  for (unsigned i = 0; i < 4; i++) {
    const DrawCall *call = &s_draws[i];
    assert(call->source.origin.x == (i / 2 == 0 ? 0 : 9 * 18));
    assert(call->source.origin.y == 0);
    assert(call->destination.origin.x == 42 + (int)(i / 2) * 18);
    assert(call->destination.origin.y == 31);
    assert(call->destination.size.w == 18 && call->destination.size.h == 28);
    assert(call->resource_id == (i % 2 ? RESOURCE_ID_DIGITS_SMALL_FILL : RESOURCE_ID_DIGITS_SMALL_OUTLINE));
    for (unsigned entry = 0; entry < 4; entry++) {
      assert(call->palette[entry].argb == (alpha[entry] | (i % 2 ? 0x2a : 0)));
    }
  }
  assert(draw("1234", GRect(0, 0, 72, 28), 0xc5));
  assert(s_draw_count == 8);
  for (unsigned i = 0; i < 8; i++) {
    assert(s_draws[i].source.origin.x == ((int)i / 2 + 1) * 18);
    assert(s_draws[i].destination.origin.x == (int)i / 2 * 18);
    assert(s_draws[i].destination.origin.y == 0);
    for (unsigned entry = 0; entry < 4; entry++) {
      assert((s_draws[i].palette[entry].argb & 0xc0) == alpha[entry]);
    }
  }
  s_draw_count = 0;
  assert(digits_draw(&s_context, "37", GRect(0,0,36,28), (GColor){.argb = 0xe4}, GColorWhite));
  assert(s_draw_count == 4 && s_create_count == 4);
  for (unsigned i = 0; i < 4; i++) {
    for (unsigned entry = 0; entry < 4; entry++) {
      assert(s_draws[i].palette[entry].argb == (alpha[entry] | (i % 2 ? 0x24 : 0x3f)));
    }
  }
}

static void test_invalid_inputs(void) {
  reset();
  digits_set_style(DIGIT_STYLE_SMALL);
  const char *invalid[] = {NULL, "", "1:2", "12345", "-1", "9a", "/", ":"};
  for (size_t i = 0; i < ARRAY_LENGTH(invalid); i++) {
    assert(!draw(invalid[i], GRect(0, 0, 200, 100), 0xff));
    assert(s_create_count == 0 && s_draw_count == 0);
  }
  const GRect bounds[] = {GRect(0,0,35,28), GRect(0,0,36,27), GRect(0,0,0,0), GRect(0,0,-1,100)};
  for (size_t i = 0; i < ARRAY_LENGTH(bounds); i++) {
    assert(!draw("10", bounds[i], 0xff));
    assert(s_draw_count == 0);
  }
  assert(draw("10", GRect(0,0,36,28), 0xff));
  assert(s_create_count == 4);
}

static void test_failure_cleanup(void) {
  for (unsigned failure = 1; failure <= 4; failure++) {
    reset();
    s_fail_create = failure;
    digits_set_style(DIGIT_STYLE_SMALL);
    assert(!draw("10", GRect(0,0,200,100), 0xff));
    assert(s_live_count == 0 && s_destroy_count == failure - 1);
    const unsigned attempts = s_create_count;
    assert(!draw("10", GRect(0,0,200,100), 0xff));
    assert(s_create_count == attempts); // Do not retry allocations every minute.
    digits_set_style(DIGIT_STYLE_NONE);
    digits_set_style(DIGIT_STYLE_SMALL);
    assert(draw("10", GRect(0,0,200,100), 0xff));
  }
  for (uint32_t resource = RESOURCE_ID_DIGITS_SMALL_FILL; resource <= RESOURCE_ID_DIGITS_SMALL_OUTLINE; resource++) {
    reset();
    s_bad_format_resource = resource;
    digits_set_style(DIGIT_STYLE_SMALL);
    assert(!draw("10", GRect(0,0,200,100), 0xff));
    assert(s_live_count == 0);
  }
  for (unsigned malformed = 1; malformed <= 4; malformed++) {
    reset();
    s_bad_dimensions = malformed;
    digits_set_style(DIGIT_STYLE_SMALL);
    assert(!draw("10", GRect(0,0,200,100), 0xff));
    assert(s_live_count == 0 && s_destroy_count == 4);
  }
}

int main(void) {
  test_lazy_load_and_mode_switch();
  test_tint_and_layout();
  test_invalid_inputs();
  test_failure_cleanup();
  reset();
  puts("Digit rendering tests passed: lifecycle, allocation failures, alpha, layout and validation");
  return 0;
}
