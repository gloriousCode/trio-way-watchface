// Capture-only prototype. Never included in a release build.
// Sparse ordered dithering suggests translucent facet shading on 64 colours.
static void draw_texture_preview(GContext *ctx, GRect bounds, GPoint centre, const SectorRay rays[3]) {
  static const uint8_t bayer[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}
  };
  GPoint directions[3];
  for (unsigned i = 0; i < 3; i++) {
    directions[i] = GPoint(sin_lookup(rays[i].angle) / 2, -cos_lookup(rays[i].angle) / 2);
  }
  graphics_context_set_stroke_width(ctx, 1);
  for (int16_t y = bounds.origin.y; y < bounds.origin.y + bounds.size.h; y++) {
    for (int16_t x = bounds.origin.x; x < bounds.origin.x + bounds.size.w; x++) {
      const int32_t dx = x - centre.x;
      const int32_t dy = y - centre.y;
      for (unsigned i = 0; i < 3; i++) {
        const unsigned next = (i + 1) % 3;
        const int32_t sweep = normalise_angle(rays[next].angle - rays[i].angle);
        if (!sweep) continue;
        const int32_t start_cross = directions[i].x * dy - directions[i].y * dx;
        const int32_t end_cross = dx * directions[next].y - dy * directions[next].x;
        const bool inside = sweep <= TRIG_MAX_ANGLE / 2 ?
          start_cross >= 0 && end_cross >= 0 : start_cross >= 0 || end_cross >= 0;
        if (!inside) continue;
        const int32_t distance = abs(start_cross) / (TRIG_MAX_RATIO / 2);
        // Peak coverage is 25%, fading to untouched colour over 64 pixels.
        const int density = distance < 64 ? (64 - distance + 15) / 16 : 0;
        if (bayer[y & 3][x & 3] < density) {
          const GColor base = rays[i].colour;
          const unsigned red = (base.argb >> 4) & 3;
          const unsigned green = (base.argb >> 2) & 3;
          const unsigned blue = base.argb & 3;
          const bool darken = red + green + blue > 6;
          const unsigned r = darken ? (red ? red - 1 : 0) : (red < 3 ? red + 1 : 3);
          const unsigned g = darken ? (green ? green - 1 : 0) : (green < 3 ? green + 1 : 3);
          const unsigned b = darken ? (blue ? blue - 1 : 0) : (blue < 3 ? blue + 1 : 3);
          graphics_context_set_stroke_color(ctx, (GColor){.argb = 0xc0 | (r << 4) | (g << 2) | b});
          graphics_draw_pixel(ctx, GPoint(x, y));
        }
        break;
      }
    }
  }
}
