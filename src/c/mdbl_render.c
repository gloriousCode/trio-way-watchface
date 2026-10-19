#include "mdbl.h"
#include <stdlib.h>
#include "mdbl_digits.h"
#include "mdbl_hybrid.h"
#include "mdbl_numbered.h"
#include "mdbl_ticks.h"
#include "mdbl_texture.h"
#include "mdbl_complication_layout.h"


#if defined(PBL_PLATFORM_GABBRO)
#define DIGITAL_TILE_TOP_PERCENT 18
#else
#define DIGITAL_TILE_TOP_PERCENT 11
#endif

void release_render_resources(void) {
  digits_release();
}

static void draw_thick_line(GContext *ctx, GPoint start, GPoint end, uint8_t stroke, GColor colour) {
  graphics_context_set_stroke_color(ctx, colour);
  graphics_context_set_stroke_width(ctx, stroke);
  graphics_draw_line(ctx, start, end);
}

static void draw_text_with_outline(
  GContext *ctx,
  const char *text,
  GFont font,
  GRect rect,
  GColor text_colour,
  GColor outline_colour,
  int16_t outline_thickness
);

static void fill_sector_polygon(GContext *ctx, GRect bounds, GPoint centre, int32_t start_angle, int32_t end_angle, GColor colour) {
  GPoint points[7];
  const uint32_t count = sector_polygon(bounds, centre, start_angle, end_angle, points);
  if (count == 0) {
    return;
  }

  // GPath is a public value type: these points live until drawing finishes.
  GPath path = {
    .num_points = count,
    .points = points
  };

  graphics_context_set_fill_color(ctx, colour);
  gpath_draw_filled(ctx, &path);
}

static void draw_sector_seams(GContext *ctx, GPoint centre, int32_t radius, int32_t hour, int32_t minute, int32_t split) {
  const GPoint hour_edge = point_on_ray(centre, radius, hour);
  const GPoint minute_edge = point_on_ray(centre, radius, minute);
  const GPoint split_edge = point_on_ray(centre, radius, split);

  draw_thick_line(ctx, centre, split_edge, SECTOR_STROKE, s_state.bg_colour);
  draw_thick_line(ctx, centre, hour_edge, SECTOR_STROKE, s_state.hour_colour);
  draw_thick_line(ctx, centre, minute_edge, SECTOR_STROKE, s_state.minute_colour);
}

static void draw_date_badge(GContext *ctx, GRect tile, bool weekday);

static void draw_hands(GContext *ctx, GRect bounds, GPoint centre, int16_t min_dimension, int32_t hour, int32_t minute) {
  const int16_t hour_length = min_dimension * HOUR_HAND_LENGTH_PERCENT / 100;
  const int16_t minute_length = min_dimension * MINUTE_HAND_LENGTH_PERCENT / 100;
  const bool round_display = PBL_IF_ROUND_ELSE(true, false);
  GRect labels[TICK_MARK_COUNT];
  size_t label_count = 0;
  if (s_state.mode == FACE_MODE_NUMBERED) {
    numbered_layout(bounds, round_display, PBL_IF_ROUND_ELSE(GSize(20, 32), GSize(18, 28)), labels);
    label_count = 12;
  } else if (s_state.mode == FACE_MODE_TICKS) {
    TickMark marks[TICK_MARK_COUNT];
    ticks_layout(bounds, round_display, marks);
    label_count = TICK_MARK_COUNT;
    for (size_t i = 0; i < label_count; i++) {
      const int16_t x = marks[i].inner.x < marks[i].outer.x ? marks[i].inner.x : marks[i].outer.x;
      const int16_t y = marks[i].inner.y < marks[i].outer.y ? marks[i].inner.y : marks[i].outer.y;
      const int16_t w = abs(marks[i].outer.x - marks[i].inner.x) + 1;
      const int16_t h = abs(marks[i].outer.y - marks[i].inner.y) + 1;
      labels[i] = GRect(x - 1, y - 1, w + 2, h + 2);
    }
  }
  const GPoint hour_end = dial_hand_end(centre, hour_length, hour, labels, label_count);
  const GPoint minute_end = dial_hand_end(centre, minute_length, minute, labels, label_count);
  const int16_t side = min_dimension * 17 / 100;
  const GSize date_size = s_state.show_date_tiles ? GSize(64, 28) :
    s_state.show_date_complication ? GSize(side, side) : GSize(0, 0);
  GRect date;
  if (place_complication(bounds, round_display, date_size, hour_end, minute_end,
                          s_state.show_hands, labels, label_count, &date)) {
    draw_date_badge(ctx, date, s_state.show_date_tiles);
  }
  if (!s_state.show_hands) return;
  const uint8_t outline_width = HOUR_HAND_STROKE + 2;
  const uint8_t minute_outline_width = MINUTE_HAND_STROKE + 2;

  draw_thick_line(ctx, centre, hour_end, outline_width, GColorBlack);
  draw_thick_line(ctx, centre, minute_end, minute_outline_width, GColorBlack);
  draw_thick_line(ctx, centre, hour_end, HOUR_HAND_STROKE, s_state.hour_hand_colour);
  draw_thick_line(ctx, centre, minute_end, MINUTE_HAND_STROKE, s_state.minute_hand_colour);
}

static void draw_centre_cap(GContext *ctx, GPoint centre) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, centre, 4);
  graphics_context_set_fill_color(ctx, s_state.minute_hand_colour);
  graphics_fill_circle(ctx, centre, 2);
}

static void draw_digital(GContext *ctx, GRect bounds) {
  static char time_text[5];
  strftime(time_text, sizeof(time_text), "%H%M", &s_state.now);

  const int16_t tile_width = bounds.size.w * 19 / 100;
  const int16_t tile_height = bounds.size.h * 16 / 100;
  const int16_t gap = bounds.size.w * 15 / 1000;
  const int16_t total_width = (tile_width * 4) + (gap * 3);
  const int16_t left = (bounds.size.w - total_width) / 2;
  // Round gabbro needs extra top padding to keep tiles comfortably inside the arc.
  const int16_t top = bounds.size.h * DIGITAL_TILE_TOP_PERCENT / 100;
  GFont font = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);

  for (int i = 0; i < 4; i++) {
    const GRect tile = GRect(left + (i * (tile_width + gap)), top, tile_width, tile_height);
    const GColor digit_colour = i < 2 ? s_state.hour_colour : s_state.minute_colour;
    char text[2] = {time_text[i], '\0'};

    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, tile, 4, GCornersAll);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, grect_inset(tile, GEdgeInsets(1)), 3, GCornersAll);

    if (!digits_draw(ctx, text, tile, digit_colour, GColorBlack)) {
      draw_text_with_outline(ctx, text, font, tile, digit_colour, GColorBlack, 1);
    }
  }
}

static void draw_text_with_outline(
  GContext *ctx,
  const char *text,
  GFont font,
  GRect rect,
  GColor text_colour,
  GColor outline_colour,
  int16_t outline_thickness
) {
  graphics_context_set_text_color(ctx, outline_colour);
  // Translate a fixed box: changing its width shifts centred glyphs unevenly.
  for (int16_t dy = -outline_thickness; dy <= outline_thickness; dy++) {
    for (int16_t dx = -outline_thickness; dx <= outline_thickness; dx++) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      GRect offset = rect;
      offset.origin.x += dx;
      offset.origin.y += dy;
      graphics_draw_text(ctx, text, font, offset, GTextOverflowModeWordWrap,
                         GTextAlignmentCenter, NULL);
    }
  }

  graphics_context_set_text_color(ctx, text_colour);
  graphics_draw_text(ctx, text, font, rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void draw_large_digital(GContext *ctx, GRect bounds) {
  char hh[3];
  char mm[3];
  strftime(hh, sizeof(hh), "%H", &s_state.now);
  strftime(mm, sizeof(mm), "%M", &s_state.now);

  const GRect top_text = GRect(0, 0, bounds.size.w, bounds.size.h * 49 / 100);
  const GRect bottom_text = GRect(0, bounds.size.h * 50 / 100, bounds.size.w, bounds.size.h * 49 / 100);
  GFont fallback_font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  if (!digits_draw(ctx, hh, top_text, s_state.hour_hand_colour, GColorBlack)) {
    draw_text_with_outline(ctx, hh, fallback_font, top_text, s_state.hour_hand_colour, GColorBlack, 1);
  }
  if (!digits_draw(ctx, mm, bottom_text, s_state.minute_hand_colour, GColorBlack)) {
    draw_text_with_outline(ctx, mm, fallback_font, bottom_text, s_state.minute_hand_colour, GColorBlack, 1);
  }
}

static void draw_date_complication(GContext *ctx, GPoint centre, int16_t min_dimension) {
  const int16_t side = (int16_t)(min_dimension * 17 / 100);
  const int16_t comp_centre_x = centre.x + (int16_t)(min_dimension * 28 / 100);
  const int16_t x = (int16_t)(comp_centre_x - side / 2);
  const int16_t y = (int16_t)(centre.y - side / 2);
  const GRect tile = GRect(x, y, side, side);
  draw_date_badge(ctx, tile, false);
}

static void draw_date_badge(GContext *ctx, GRect tile, bool weekday) {
  char date[8];
  strftime(date, sizeof(date), weekday ? "%a %d" : "%d", &s_state.now);
  const uint16_t radius = (uint16_t)(tile.size.h / 4);

  graphics_context_set_fill_color(ctx, s_state.complication_bg_colour);
  graphics_fill_rect(ctx, tile, radius, GCornersAll);

  graphics_context_set_stroke_color(ctx, s_state.complication_border_colour);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_round_rect(ctx, tile, radius);

  GFont font = fonts_get_system_font(weekday ? FONT_KEY_GOTHIC_18_BOLD :
    PBL_IF_ROUND_ELSE(FONT_KEY_GOTHIC_28_BOLD, FONT_KEY_GOTHIC_24_BOLD));
  const GRect text_bounds = grect_inset(tile, GEdgeInsets(2));
  const GSize text_size = graphics_text_layout_get_content_size(
    date,
    font,
    text_bounds,
    GTextOverflowModeWordWrap,
    GTextAlignmentCenter
  );
  const int16_t text_y = text_bounds.origin.y + ((text_bounds.size.h - text_size.h) / 2);
  const GRect text_rect = GRect(text_bounds.origin.x, text_y, text_bounds.size.w, text_size.h);

  graphics_context_set_text_color(ctx, s_state.complication_text_colour);
  graphics_draw_text(ctx, date, font, text_rect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

void update_canvas(Layer *layer, GContext *ctx) {
  graphics_context_set_antialiased(ctx, true);
  digits_set_style(s_state.mode == FACE_MODE_LARGE_DIGITAL ? DIGIT_STYLE_LARGE :
                   (s_state.mode == FACE_MODE_DIGITAL || s_state.mode == FACE_MODE_NUMBERED) ? DIGIT_STYLE_SMALL : DIGIT_STYLE_NONE);
  const GRect bounds = layer_get_bounds(layer);
  const GRect paint_bounds = GRect(bounds.origin.x - 1, bounds.origin.y - 1, bounds.size.w + 2, bounds.size.h + 2);

  if (!s_state.cache_ready || !bounds_equal(bounds, s_state.cached_bounds)) {
    rebuild_geometry_cache(bounds);
  }

  const GPoint centre = s_state.cached_centre;
  const int16_t radius = s_state.cached_radius;
  const int32_t hour = hour_angle(&s_state.now);
  const int32_t minute = minute_angle(&s_state.now);
  const int32_t split = opposite_split_angle(hour, minute);
  SectorRay rays[3] = {
    {.angle = hour, .colour = s_state.hour_colour},
    {.angle = minute, .colour = s_state.minute_colour},
    {.angle = split, .colour = s_state.bg_colour}
  };
  sort_rays(rays, ARRAY_LENGTH(rays));

  graphics_context_set_fill_color(ctx, s_state.bg_colour);
  graphics_fill_rect(ctx, paint_bounds, 0, GCornerNone);

  for (size_t i = 0; i < ARRAY_LENGTH(rays); i++) {
    const SectorRay current = rays[i];
    const SectorRay next = rays[(i + 1) % ARRAY_LENGTH(rays)];
    // The base fill already covers the background sector, including coincident hands.
    if (gcolor_equal(current.colour, s_state.bg_colour) || current.angle == next.angle) {
      continue;
    }
    fill_sector_polygon(ctx, paint_bounds, centre, current.angle, next.angle, current.colour);
  }
  draw_sector_seams(ctx, centre, radius, hour, minute, split);
  if (s_state.show_texture) {
    draw_texture(ctx, bounds, centre, rays);
  }

  if (s_state.mode == FACE_MODE_DIGITAL) {
    const int16_t side = s_state.cached_min_dimension * 17 / 100;
    const GSize date_size = s_state.show_date_tiles ? GSize(64, 28) :
      s_state.show_date_complication ? GSize(side, side) : GSize(0, 0);
    HybridLayout layout;
    if (draw_hybrid(ctx, bounds, centre, s_state.cached_min_dimension, hour, minute,
                    &s_state.now, s_state.hour_hand_colour, s_state.minute_hand_colour,
                    date_size, &layout)) {
      if (layout.has_date_space) {
        draw_date_badge(ctx, layout.date_badge, s_state.show_date_tiles);
      }
    } else {
      draw_digital(ctx, bounds);
    }
  } else if (s_state.mode == FACE_MODE_LARGE_DIGITAL) {
    draw_large_digital(ctx, bounds);
  } else if (s_state.mode == FACE_MODE_NUMBERED) {
    draw_numbered_dial(ctx, bounds, &s_state.now,
                       s_state.hour_hand_colour, s_state.minute_hand_colour);
  } else if (s_state.mode == FACE_MODE_TICKS) {
    draw_tick_dial(ctx, bounds);
  }

  if (s_state.mode == FACE_MODE_LARGE_DIGITAL && s_state.show_date_complication) {
    draw_date_complication(ctx, centre, s_state.cached_min_dimension);
  }

  if (s_state.mode == FACE_MODE_HANDS || s_state.mode == FACE_MODE_NUMBERED || s_state.mode == FACE_MODE_TICKS) {
    draw_hands(ctx, bounds, centre, s_state.cached_min_dimension, hour, minute);
    if (s_state.show_hands) draw_centre_cap(ctx, centre);
  } else {
    draw_centre_cap(ctx, centre);
  }
}
