#pragma once

// Minimal host declarations for the focused unit tests, not an SDK replacement.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

typedef struct Window Window;
typedef struct Layer Layer;
typedef struct GContext GContext;
typedef void *GFont;
typedef union GColor {
  uint8_t argb;
} GColor;
typedef struct GPoint {
  int16_t x;
  int16_t y;
} GPoint;
typedef struct GSize {
  int16_t w;
  int16_t h;
} GSize;
typedef struct GRect {
  GPoint origin;
  GSize size;
} GRect;

#define GPoint(x_value, y_value) ((GPoint){(x_value), (y_value)})
#define GSize(width, height) ((GSize){(width), (height)})
#define GRect(x_value, y_value, width, height) ((GRect){GPoint(x_value, y_value), GSize(width, height)})
#define GColorBlack ((GColor){.argb = 0xc0})
#define GColorWhite ((GColor){.argb = 0xff})
#define TRIG_MAX_ANGLE 65536
#define TRIG_MAX_RATIO 65535
int32_t sin_lookup(int32_t angle);
int32_t cos_lookup(int32_t angle);
#define FONT_KEY_GOTHIC_24_BOLD "gothic24bold"
typedef struct GEdgeInsets { int16_t top, right, bottom, left; } GEdgeInsets;
#define GEdgeInsets(value) ((GEdgeInsets){value, value, value, value})
typedef enum GCornerMask { GCornersAll = 15 } GCornerMask;
typedef enum GTextOverflowMode { GTextOverflowModeWordWrap } GTextOverflowMode;
typedef enum GTextAlignment { GTextAlignmentCenter } GTextAlignment;

GRect grect_inset(GRect rect, GEdgeInsets insets);
void graphics_context_set_fill_color(GContext *ctx, GColor colour);
void graphics_context_set_stroke_color(GContext *ctx, GColor colour);
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width);
void graphics_context_set_text_color(GContext *ctx, GColor colour);
void graphics_draw_line(GContext *ctx, GPoint start, GPoint end);
void graphics_draw_pixel(GContext *ctx, GPoint point);
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t radius, GCornerMask corners);
void graphics_draw_text(GContext *ctx, const char *text, GFont font, GRect rect,
                        GTextOverflowMode overflow, GTextAlignment alignment, void *attributes);
GFont fonts_get_system_font(const char *key);

typedef struct GBitmap GBitmap;
typedef enum GBitmapFormat {
  GBitmapFormat1Bit,
  GBitmapFormat8Bit,
  GBitmapFormat1BitPalette,
  GBitmapFormat2BitPalette
} GBitmapFormat;
typedef enum GCompOp { GCompOpAssign, GCompOpSet } GCompOp;

enum {
  RESOURCE_ID_DIGITS_SMALL_FILL = 100,
  RESOURCE_ID_DIGITS_SMALL_OUTLINE,
  RESOURCE_ID_DIGITS_LARGE_FILL,
  RESOURCE_ID_DIGITS_LARGE_OUTLINE
};

GBitmap *gbitmap_create_with_resource(uint32_t resource_id);
GBitmap *gbitmap_create_as_sub_bitmap(const GBitmap *base_bitmap, GRect bounds);
void gbitmap_destroy(GBitmap *bitmap);
GBitmapFormat gbitmap_get_format(const GBitmap *bitmap);
GRect gbitmap_get_bounds(const GBitmap *bitmap);
void gbitmap_set_bounds(GBitmap *bitmap, GRect bounds);
GColor *gbitmap_get_palette(const GBitmap *bitmap);
void graphics_context_set_compositing_mode(GContext *ctx, GCompOp mode);
void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *bitmap, GRect bounds);

int persist_get_size(uint32_t key);
int persist_read_data(uint32_t key, void *buffer, size_t size);
int persist_write_data(uint32_t key, const void *buffer, size_t size);
