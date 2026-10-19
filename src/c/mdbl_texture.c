#include "mdbl_texture.h"

#include <stdlib.h>

typedef struct TextureFacet {
  GPoint direction;
  GColor shade;
  bool active;
  bool wide;
} TextureFacet;

static GColor texture_shade(GColor base) {
  const unsigned red = (base.argb >> 4) & 3;
  const unsigned green = (base.argb >> 2) & 3;
  const unsigned blue = base.argb & 3;
  const bool darken = red + green + blue > 6;
  const unsigned r = darken ? (red ? red - 1 : 0) : (red < 3 ? red + 1 : 3);
  const unsigned g = darken ? (green ? green - 1 : 0) : (green < 3 ? green + 1 : 3);
  const unsigned b = darken ? (blue ? blue - 1 : 0) : (blue < 3 ? blue + 1 : 3);
  return (GColor){.argb = 0xc0 | (r << 4) | (g << 2) | b};
}

void draw_texture(GContext *ctx, GRect bounds, GPoint centre, const SectorRay rays[3]) {
  if (bounds.size.w <= 0 || bounds.size.h <= 0) {
    return;
  }
  static const uint8_t bayer[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}
  };
  TextureFacet facets[3];
  for (unsigned i = 0; i < 3; i++) {
    const int32_t sweep = normalise_angle(rays[(i + 1) % 3].angle - rays[i].angle);
    facets[i] = (TextureFacet){
      .direction = GPoint(sin_lookup(rays[i].angle) / 2, -cos_lookup(rays[i].angle) / 2),
      .shade = texture_shade(rays[i].colour),
      .active = sweep != 0,
      .wide = sweep > TRIG_MAX_ANGLE / 2
    };
  }
  graphics_context_set_stroke_width(ctx, 1);
  bool colour_set = false;
  GColor last_colour = GColorBlack;
  // Coverage never exceeds four Bayer levels. Only even rows and columns
  // contain values below four, so skip the other 75% before any cross products.
  const int16_t first_y = bounds.origin.y + (bounds.origin.y & 1);
  const int16_t first_x = bounds.origin.x + (bounds.origin.x & 1);
  for (int16_t y = first_y; y < bounds.origin.y + bounds.size.h; y += 2) {
    const int32_t dy = y - centre.y;
    for (int16_t x = first_x; x < bounds.origin.x + bounds.size.w; x += 2) {
      const int32_t dx = x - centre.x;
      const uint8_t threshold = bayer[y & 3][x & 3];
      for (unsigned i = 0; i < 3; i++) {
        if (!facets[i].active) {
          continue;
        }
        const GPoint start = facets[i].direction;
        const GPoint end = facets[(i + 1) % 3].direction;
        const int32_t start_cross = start.x * dy - start.y * dx;
        const int32_t end_cross = dx * end.y - dy * end.x;
        const bool inside = facets[i].wide ? start_cross >= 0 || end_cross >= 0 :
          start_cross >= 0 && end_cross >= 0;
        if (!inside) {
          continue;
        }
        const int32_t distance = abs(start_cross) / (TRIG_MAX_RATIO / 2);
        const int density = distance < 64 ? (64 - distance + 15) / 16 : 0;
        if (threshold < density) {
          if (!colour_set || last_colour.argb != facets[i].shade.argb) {
            last_colour = facets[i].shade;
            graphics_context_set_stroke_color(ctx, last_colour);
            colour_set = true;
          }
          graphics_draw_pixel(ctx, GPoint(x, y));
        }
        break;
      }
    }
  }
}
