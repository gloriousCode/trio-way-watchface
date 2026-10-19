#include "mdbl_digits.h"

#include <string.h>

typedef struct DigitMask {
  GBitmap *strip;
  GBitmap *glyph;
} DigitMask;

static DigitStyle s_style;
static bool s_load_attempted;
static DigitMask s_fill;
static DigitMask s_outline;
static GSize s_cell;

static void release_mask(DigitMask *mask) {
  if (mask->glyph) {
    gbitmap_destroy(mask->glyph);
    mask->glyph = NULL;
  }
  if (mask->strip) {
    gbitmap_destroy(mask->strip);
    mask->strip = NULL;
  }
}

void digits_release(void) {
  release_mask(&s_fill);
  release_mask(&s_outline);
  s_load_attempted = false;
  s_style = DIGIT_STYLE_NONE;
}

void digits_set_style(DigitStyle style) {
  if (s_style != style) {
    digits_release();
    s_style = style;
  }
}

static bool load_mask(DigitMask *mask, uint32_t resource_id) {
  mask->strip = gbitmap_create_with_resource(resource_id);
  if (!mask->strip || gbitmap_get_format(mask->strip) != GBitmapFormat2BitPalette) {
    release_mask(mask);
    return false;
  }
  // The sub-bitmap shares the full strip's pixels and palette. Its bounds select
  // each glyph at draw time; no allocation occurs inside the digit loop.
  mask->glyph = gbitmap_create_as_sub_bitmap(mask->strip, gbitmap_get_bounds(mask->strip));
  if (!mask->glyph) {
    release_mask(mask);
    return false;
  }
  return true;
}

static bool ensure_masks(void) {
  if (s_load_attempted) {
    return s_fill.glyph && s_outline.glyph;
  }
  if (s_style == DIGIT_STYLE_NONE) {
    return false;
  }
  s_load_attempted = true;
  const uint32_t fill_id = s_style == DIGIT_STYLE_LARGE ?
    RESOURCE_ID_DIGITS_LARGE_FILL : RESOURCE_ID_DIGITS_SMALL_FILL;
  const uint32_t outline_id = s_style == DIGIT_STYLE_LARGE ?
    RESOURCE_ID_DIGITS_LARGE_OUTLINE : RESOURCE_ID_DIGITS_SMALL_OUTLINE;
  if (!load_mask(&s_fill, fill_id) || !load_mask(&s_outline, outline_id)) {
    release_mask(&s_fill);
    release_mask(&s_outline);
    return false;
  }
  const GRect fill_bounds = gbitmap_get_bounds(s_fill.strip);
  const GRect outline_bounds = gbitmap_get_bounds(s_outline.strip);
  if (fill_bounds.size.w <= 0 || fill_bounds.size.w % 10 != 0 ||
      fill_bounds.size.w != outline_bounds.size.w ||
      fill_bounds.size.h != outline_bounds.size.h) {
    release_mask(&s_fill);
    release_mask(&s_outline);
    return false;
  }
  s_cell = GSize(fill_bounds.size.w / 10, fill_bounds.size.h);
  return true;
}

static void tint_mask(DigitMask *mask, GColor colour) {
  GColor *palette = gbitmap_get_palette(mask->strip);
  for (size_t i = 0; i < 4; i++) {
    // The resource converter may reorder palette entries. Preserve each entry's
    // original coverage instead of assuming an index-to-alpha relationship.
    palette[i].argb = (palette[i].argb & 0xc0) | (colour.argb & 0x3f);
  }
}

bool digits_draw(GContext *ctx, const char *text, GRect bounds, GColor colour, GColor outline_colour) {
  if (!text || !*text) {
    return false;
  }
  const size_t length = strlen(text);
  if (length > 4) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    if (text[i] < '0' || text[i] > '9') {
      return false;
    }
  }
  if (!ensure_masks() || (int16_t)(s_cell.w * length) > bounds.size.w ||
      s_cell.h > bounds.size.h) {
    return false;
  }
  tint_mask(&s_fill, colour);
  tint_mask(&s_outline, outline_colour);
  const int16_t left = bounds.origin.x + (bounds.size.w - s_cell.w * length) / 2;
  const int16_t top = bounds.origin.y + (bounds.size.h - s_cell.h) / 2;
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  for (size_t i = 0; i < length; i++) {
    const GRect source = GRect((text[i] - '0') * s_cell.w, 0, s_cell.w, s_cell.h);
    const GRect destination = GRect(left + i * s_cell.w, top, s_cell.w, s_cell.h);
    gbitmap_set_bounds(s_outline.glyph, source);
    gbitmap_set_bounds(s_fill.glyph, source);
    graphics_draw_bitmap_in_rect(ctx, s_outline.glyph, destination);
    graphics_draw_bitmap_in_rect(ctx, s_fill.glyph, destination);
  }
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  return true;
}
