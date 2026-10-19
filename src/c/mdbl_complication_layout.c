#include "mdbl_complication_layout.h"

#include "mdbl.h"

static bool inside_display(GRect rect, GRect bounds, GPoint centre, bool round_display) {
  if (rect.origin.x < bounds.origin.x + 3 || rect.origin.y < bounds.origin.y + 3 ||
      rect.origin.x + rect.size.w > bounds.origin.x + bounds.size.w - 3 ||
      rect.origin.y + rect.size.h > bounds.origin.y + bounds.size.h - 3) {
    return false;
  }
  if (round_display) {
    const int32_t radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 3;
    for (unsigned corner = 0; corner < 4; corner++) {
      const int32_t x = rect.origin.x + (corner & 1 ? rect.size.w - 1 : 0) - centre.x;
      const int32_t y = rect.origin.y + (corner & 2 ? rect.size.h - 1 : 0) - centre.y;
      if (x * x + y * y > radius * radius) return false;
    }
  }
  return true;
}

static bool clear_of_centre(GRect rect, GPoint centre) {
  const int32_t right = rect.origin.x + rect.size.w - 1;
  const int32_t bottom = rect.origin.y + rect.size.h - 1;
  const int32_t dx = centre.x < rect.origin.x ? rect.origin.x - centre.x :
    centre.x > right ? centre.x - right : 0;
  const int32_t dy = centre.y < rect.origin.y ? rect.origin.y - centre.y :
    centre.y > bottom ? centre.y - bottom : 0;
  return dx * dx + dy * dy >= 64;
}

static bool separated(GRect a, GRect b) {
  return b.size.w <= 0 || b.size.h <= 0 ||
         a.origin.x + a.size.w + 3 <= b.origin.x ||
         b.origin.x + b.size.w + 3 <= a.origin.x ||
         a.origin.y + a.size.h + 3 <= b.origin.y ||
         b.origin.y + b.size.h + 3 <= a.origin.y;
}

static bool intersects_hand(GRect rect, GPoint start, GPoint end) {
  // Six pixels cover the widest outline's radius plus a visible gap, including
  // round end caps. Axis-aligned expansion is deliberately conservative.
  const int32_t left = rect.origin.x - 6;
  const int32_t right = rect.origin.x + rect.size.w - 1 + 6;
  const int32_t top = rect.origin.y - 6;
  const int32_t bottom = rect.origin.y + rect.size.h - 1 + 6;
  if ((start.x < left && end.x < left) || (start.x > right && end.x > right) ||
      (start.y < top && end.y < top) || (start.y > bottom && end.y > bottom)) {
    return false;
  }
  // Separating-axis test for a segment and a rectangle: x/y were checked above;
  // the segment's normal is the only remaining possible separating axis.
  bool positive = false;
  bool negative = false;
  for (unsigned corner = 0; corner < 4; corner++) {
    const int32_t x = corner & 1 ? right : left;
    const int32_t y = corner & 2 ? bottom : top;
    const int64_t side = (int64_t)(end.x - start.x) * (y - start.y) -
                         (int64_t)(end.y - start.y) * (x - start.x);
    if (side == 0) return true;
    positive |= side > 0;
    negative |= side < 0;
  }
  return positive && negative;
}

bool place_complication(GRect bounds, bool round_display, GSize size,
                        GPoint hour_end, GPoint minute_end, bool show_hands,
                        const GRect *avoid, size_t avoid_count, GRect *out) {
  if (!out || (avoid_count && !avoid) || bounds.size.w <= 6 || bounds.size.h <= 6 ||
      size.w <= 0 || size.h <= 0 || size.w > bounds.size.w - 6 || size.h > bounds.size.h - 6) {
    return false;
  }
  const GPoint centre = GPoint(bounds.origin.x + bounds.size.w / 2,
                               bounds.origin.y + bounds.size.h / 2);
  const int16_t dimension = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  // Bottom first, then right/left/top; cardinal choices precede diagonals.
  static const uint8_t directions[] = {
    16, 8, 24, 0, 12, 20, 4, 28, 14, 18, 6, 10, 22, 26, 2, 30,
    15, 17, 7, 9, 23, 25, 1, 31, 13, 19, 5, 11, 21, 27, 3, 29
  };
  static const uint8_t radii[] = {26, 22, 30, 18, 34, 38, 14, 42};
  for (size_t radius = 0; radius < ARRAY_LENGTH(radii); radius++) {
    for (size_t direction = 0; direction < ARRAY_LENGTH(directions); direction++) {
      const GPoint position = point_on_ray(centre, dimension * radii[radius] / 100,
                                           directions[direction] * TRIG_MAX_ANGLE / 32);
      const GRect candidate = GRect(position.x - size.w / 2, position.y - size.h / 2, size.w, size.h);
      if (!inside_display(candidate, bounds, centre, round_display) ||
          !clear_of_centre(candidate, centre) ||
          (show_hands && (intersects_hand(candidate, centre, hour_end) ||
                          intersects_hand(candidate, centre, minute_end)))) {
        continue;
      }
      bool clear = true;
      for (size_t i = 0; i < avoid_count; i++) {
        if (!separated(candidate, avoid[i])) {
          clear = false;
          break;
        }
      }
      if (clear) {
        *out = candidate;
        return true;
      }
    }
  }
  return false;
}
