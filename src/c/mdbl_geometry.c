#include "mdbl.h"

#include <stdlib.h>

int32_t normalise_angle(int32_t angle) {
  int32_t wrapped = angle % TRIG_MAX_ANGLE;
  if (wrapped < 0) {
    wrapped += TRIG_MAX_ANGLE;
  }
  return wrapped;
}

int32_t hour_angle(const struct tm *now) {
  const int32_t hour12 = now->tm_hour % 12;
  const int32_t minute_progress = (int32_t)(((int64_t)now->tm_min * TRIG_MAX_ANGLE) / (12 * 60));
  return normalise_angle((int32_t)((hour12 * TRIG_MAX_ANGLE) / 12) + minute_progress);
}

int32_t minute_angle(const struct tm *now) {
  return normalise_angle((int32_t)(((int64_t)now->tm_min * TRIG_MAX_ANGLE) / 60));
}

int32_t opposite_split_angle(int32_t hour, int32_t minute) {
  int32_t delta = normalise_angle(minute - hour);
  if (delta > (TRIG_MAX_ANGLE / 2)) {
    delta -= TRIG_MAX_ANGLE;
  }

  const int32_t midpoint = normalise_angle(hour + (delta / 2));
  return normalise_angle(midpoint + (TRIG_MAX_ANGLE / 2));
}

GPoint point_on_ray(GPoint centre, int32_t radius, int32_t angle) {
  return GPoint(
    centre.x + (int16_t)(sin_lookup(angle) * radius / TRIG_MAX_RATIO),
    centre.y - (int16_t)(cos_lookup(angle) * radius / TRIG_MAX_RATIO)
  );
}

bool bounds_equal(GRect a, GRect b) {
  return a.origin.x == b.origin.x &&
         a.origin.y == b.origin.y &&
         a.size.w == b.size.w &&
         a.size.h == b.size.h;
}

static int16_t isqrt16(int32_t value) {
  if (value <= 0) {
    return 0;
  }

  int32_t x = value;
  int32_t y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + (value / x)) / 2;
  }

  return (int16_t)x;
}

void rebuild_geometry_cache(GRect bounds) {
  s_state.cached_bounds = bounds;
  s_state.cached_centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  s_state.cached_min_dimension = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  const int16_t half_w = bounds.size.w / 2;
  const int16_t half_h = bounds.size.h / 2;
  const int32_t corner_distance_sq = (int32_t)(half_w * half_w) + (int32_t)(half_h * half_h);
  s_state.cached_radius = (int16_t)(isqrt16(corner_distance_sq) + 4);
  s_state.cache_ready = true;
}

void sort_rays(SectorRay *rays, size_t count) {
  for (size_t i = 0; i < count; i++) {
    for (size_t j = i + 1; j < count; j++) {
      if (rays[j].angle < rays[i].angle) {
        const SectorRay swap = rays[i];
        rays[i] = rays[j];
        rays[j] = swap;
      }
    }
  }
}

static bool point_in_bounds(GPoint point, GRect bounds) {
  return point.x >= bounds.origin.x &&
         point.y >= bounds.origin.y &&
         point.x < bounds.origin.x + bounds.size.w &&
         point.y < bounds.origin.y + bounds.size.h;
}

static BoundaryEdge detect_edge(GPoint point, GRect bounds) {
  const int16_t top = bounds.origin.y;
  const int16_t right = bounds.origin.x + bounds.size.w - 1;
  const int16_t bottom = bounds.origin.y + bounds.size.h - 1;

  if (point.y <= top) {
    return EDGE_TOP;
  }
  if (point.x >= right) {
    return EDGE_RIGHT;
  }
  if (point.y >= bottom) {
    return EDGE_BOTTOM;
  }
  return EDGE_LEFT;
}

static GPoint edge_terminal_corner(BoundaryEdge edge, GRect bounds) {
  const int16_t left = bounds.origin.x;
  const int16_t top = bounds.origin.y;
  const int16_t right = bounds.origin.x + bounds.size.w - 1;
  const int16_t bottom = bounds.origin.y + bounds.size.h - 1;

  switch (edge) {
    case EDGE_TOP:
      return GPoint(right, top);
    case EDGE_RIGHT:
      return GPoint(right, bottom);
    case EDGE_BOTTOM:
      return GPoint(left, bottom);
    case EDGE_LEFT:
    default:
      return GPoint(left, top);
  }
}

static GPoint ray_hit_bounds(GPoint centre, int32_t angle, GRect bounds, BoundaryEdge *edge) {
  const int16_t max_dim = bounds.size.w > bounds.size.h ? bounds.size.w : bounds.size.h;
  const GPoint far = point_on_ray(centre, max_dim * 4, angle);

  int16_t x0 = centre.x;
  int16_t y0 = centre.y;
  const int16_t x1 = far.x;
  const int16_t y1 = far.y;

  int16_t dx = (int16_t)abs(x1 - x0);
  int16_t sx = x0 < x1 ? 1 : -1;
  int16_t dy = (int16_t)-abs(y1 - y0);
  int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;

  GPoint last_in = centre;
  while (true) {
    const GPoint current = GPoint(x0, y0);
    if (point_in_bounds(current, bounds)) {
      last_in = current;
    } else {
      break;
    }

    if (x0 == x1 && y0 == y1) {
      break;
    }

    const int16_t e2 = (int16_t)(2 * err);
    if (e2 >= dy) {
      err = (int16_t)(err + dy);
      x0 = (int16_t)(x0 + sx);
    }
    if (e2 <= dx) {
      err = (int16_t)(err + dx);
      y0 = (int16_t)(y0 + sy);
    }
  }

  if (edge) {
    *edge = detect_edge(last_in, bounds);
  }
  return last_in;
}

// Produce a clockwise screen-bounded sector; equal rays have no filled area.
uint32_t sector_polygon(GRect bounds, GPoint centre, int32_t start_angle,
                        int32_t end_angle, GPoint points[7]) {
  const int32_t start = normalise_angle(start_angle);
  const int32_t end = normalise_angle(end_angle);
  if (start == end || bounds.size.w <= 0 || bounds.size.h <= 0 ||
      !point_in_bounds(centre, bounds)) {
    return 0;
  }
  BoundaryEdge start_edge;
  BoundaryEdge end_edge;
  const GPoint start_point = ray_hit_bounds(centre, start, bounds, &start_edge);
  const GPoint end_point = ray_hit_bounds(centre, end, bounds, &end_edge);
  uint32_t count = 0;
  points[count++] = centre;
  points[count++] = start_point;
  BoundaryEdge edge = start_edge;
  // A near-full-circle sector can begin and end on the same screen edge.
  bool full_boundary = start_edge == end_edge &&
                       normalise_angle(end - start) > TRIG_MAX_ANGLE / 2;
  while ((edge != end_edge || full_boundary) && count < 6) {
    points[count++] = edge_terminal_corner(edge, bounds);
    edge = (BoundaryEdge)((edge + 1) % 4);
    full_boundary = false;
  }
  points[count++] = end_point;
  int32_t doubled_area = 0;
  for (uint32_t i = 0; i < count; i++) {
    const GPoint a = points[i];
    const GPoint b = points[(i + 1) % count];
    doubled_area += (int32_t)a.x * b.y - (int32_t)a.y * b.x;
  }
  return doubled_area == 0 ? 0 : count;
}
