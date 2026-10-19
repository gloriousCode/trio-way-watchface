#include "mdbl.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

AppState s_state;
static const double s_tau = 6.28318530717958647692;
static unsigned s_sample_count;

int32_t sin_lookup(int32_t angle) {
  return (int32_t)lround(sin(angle * s_tau / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}

int32_t cos_lookup(int32_t angle) {
  return (int32_t)lround(cos(angle * s_tau / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}

static bool polygon_contains(const GPoint *points, uint32_t count, double x, double y) {
  bool inside = false;
  for (uint32_t i = 0, j = count ? count - 1 : 0; i < count; j = i++) {
    const GPoint a = points[i];
    const GPoint b = points[j];
    if ((a.y > y) != (b.y > y) &&
        x < (double)(b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) {
      inside = !inside;
    }
  }
  return inside;
}

static bool away_from_rays(double x, double y, GPoint centre, const SectorRay rays[3]) {
  for (unsigned i = 0; i < 3; i++) {
    const double angle = rays[i].angle * s_tau / TRIG_MAX_ANGLE;
    const double distance = fabs((x - centre.x) * cos(angle) +
                                 (y - centre.y) * sin(angle));
    // The bounded Bresenham intersection is rounded to a whole pixel.
    // Exclude both sides of shared boundaries from the interior-coverage test.
    if (distance < 2.0) {
      return false;
    }
  }
  return true;
}

static void assert_vertices(const GPoint *points, uint32_t count, GRect bounds, GPoint centre) {
  assert(count == 0 || (count >= 3 && count <= 7));
  if (count == 0) return;
  assert(points[0].x == centre.x && points[0].y == centre.y);
  int32_t doubled_area = 0;
  for (uint32_t i = 0; i < count; i++) {
    const GPoint a = points[i];
    const GPoint b = points[(i + 1) % count];
    assert(a.x >= bounds.origin.x && a.x < bounds.origin.x + bounds.size.w);
    assert(a.y >= bounds.origin.y && a.y < bounds.origin.y + bounds.size.h);
    if (i > 0) {
      assert(a.x == bounds.origin.x || a.x == bounds.origin.x + bounds.size.w - 1 ||
             a.y == bounds.origin.y || a.y == bounds.origin.y + bounds.size.h - 1);
    }
    doubled_area += (int32_t)a.x * b.y - (int32_t)a.y * b.x;
  }
  assert(doubled_area > 0);
}

static void test_all_minute_sectors(void) {
  const GSize screens[] = {GSize(200, 228), GSize(260, 260)};
  for (size_t screen = 0; screen < ARRAY_LENGTH(screens); screen++) {
    const GRect bounds = GRect(-1, -1, screens[screen].w + 2, screens[screen].h + 2);
    const GPoint centre = GPoint(screens[screen].w / 2, screens[screen].h / 2);
    for (int minute = 0; minute < 1440; minute++) {
      const struct tm now = {.tm_hour = minute / 60, .tm_min = minute % 60};
      const int32_t hour = hour_angle(&now);
      const int32_t minute_hand = minute_angle(&now);
      SectorRay rays[] = {
        {.angle = hour, .colour = {.argb = 1}},
        {.angle = minute_hand, .colour = {.argb = 2}},
        {.angle = opposite_split_angle(hour, minute_hand), .colour = {.argb = 3}}
      };
      sort_rays(rays, ARRAY_LENGTH(rays));
      assert(rays[0].angle <= rays[1].angle && rays[1].angle <= rays[2].angle);
      GPoint polygons[3][7];
      uint32_t counts[3];
      for (unsigned i = 0; i < 3; i++) {
        counts[i] = sector_polygon(bounds, centre, rays[i].angle, rays[(i + 1) % 3].angle,
                                   polygons[i]);
        assert_vertices(polygons[i], counts[i], bounds, centre);
      }
      for (int y = 2; y < screens[screen].h - 2; y += 7) {
        for (int x = 2; x < screens[screen].w - 2; x += 7) {
          const double px = x + 0.37;
          const double py = y + 0.61;
          if (!away_from_rays(px, py, centre, rays)) continue;
          double angle = atan2(px - centre.x, centre.y - py) * TRIG_MAX_ANGLE / s_tau;
          if (angle < 0) angle += TRIG_MAX_ANGLE;
          unsigned expected = 2;
          for (unsigned i = 0; i < 3; i++) {
            if (angle >= rays[i].angle) expected = i;
          }
          unsigned coverage = 0;
          for (unsigned i = 0; i < 3; i++) {
            const bool contains = polygon_contains(polygons[i], counts[i], px, py);
            coverage += contains;
            if (contains != (i == expected)) {
              fprintf(stderr, "Wrong sector: screen=%zu minute=%d point=(%f,%f) sector=%u expected=%u\n",
                      screen, minute, px, py, i, expected);
              assert(false);
            }
          }
          assert(coverage == 1);
          s_sample_count++;
        }
      }
    }
  }
}

static int32_t degrees(int angle) {
  return angle * TRIG_MAX_ANGLE / 360;
}

static void test_sector_edge_cases(void) {
  const GRect bounds = GRect(10, 20, 200, 228);
  const GPoint centre = GPoint(110, 134);
  GPoint points[7];
  assert(sector_polygon(bounds, centre, 0, 0, points) == 0);
  assert(sector_polygon(bounds, centre, -TRIG_MAX_ANGLE, TRIG_MAX_ANGLE, points) == 0);
  assert(sector_polygon(bounds, centre, 0, 1, points) == 0); // Same pixel intersection.
  assert(sector_polygon(GRect(0, 0, 0, 20), centre, 0, degrees(90), points) == 0);
  assert(sector_polygon(bounds, GPoint(0, 0), 0, degrees(90), points) == 0);

  uint32_t count = sector_polygon(bounds, centre, degrees(350), degrees(10), points);
  assert_vertices(points, count, bounds, centre);
  assert(polygon_contains(points, count, 110.3, 30.2));
  assert(!polygon_contains(points, count, 110.3, 230.2));
  count = sector_polygon(bounds, centre, degrees(10), degrees(350), points);
  assert(count == 7); // Traverse all four corners for the complementary wide sector.
  assert_vertices(points, count, bounds, centre);
  assert(!polygon_contains(points, count, 110.3, 30.2));
  assert(polygon_contains(points, count, 110.3, 230.2));

  GPoint normalised[7];
  const uint32_t wrapped_count = sector_polygon(bounds, centre, degrees(10) - TRIG_MAX_ANGLE,
                                                degrees(350) + TRIG_MAX_ANGLE, normalised);
  assert(count == wrapped_count);
  assert(memcmp(points, normalised, count * sizeof(GPoint)) == 0);
}

static void test_angles_and_geometry_cache(void) {
  // Reference dial: 10:09 puts the hour slightly beyond 10 and minute before 2.
  const struct tm reference = {.tm_hour = 10, .tm_min = 9};
  assert(fabs(hour_angle(&reference) * 360.0 / TRIG_MAX_ANGLE - 304.5) < 0.01);
  assert(fabs(minute_angle(&reference) * 360.0 / TRIG_MAX_ANGLE - 54.0) < 0.01);
  const int32_t values[] = {INT32_MIN, -131073, -65536, -1, 0, 1, 65535, 65536, INT32_MAX};
  for (size_t i = 0; i < ARRAY_LENGTH(values); i++) {
    int64_t expected = values[i] % (int64_t)TRIG_MAX_ANGLE;
    if (expected < 0) expected += TRIG_MAX_ANGLE;
    assert(normalise_angle(values[i]) == expected);
  }
  for (int minute = 0; minute < 1440; minute++) {
    const struct tm now = {.tm_hour = minute / 60, .tm_min = minute % 60};
    const double expected_hour = (minute % 720) * (double)TRIG_MAX_ANGLE / 720;
    const double expected_minute = (minute % 60) * (double)TRIG_MAX_ANGLE / 60;
    // The production hour formula rounds the hour and minute terms separately.
    assert(fabs(hour_angle(&now) - expected_hour) < 2.0);
    assert(fabs(minute_angle(&now) - expected_minute) < 1.0);
  }
  assert(opposite_split_angle(0, 0) == TRIG_MAX_ANGLE / 2);
  assert(opposite_split_angle(0, TRIG_MAX_ANGLE / 4) == 5 * TRIG_MAX_ANGLE / 8);
  assert(opposite_split_angle(3 * TRIG_MAX_ANGLE / 4, TRIG_MAX_ANGLE / 4) ==
         TRIG_MAX_ANGLE / 2);
  assert(opposite_split_angle(7 * TRIG_MAX_ANGLE / 8, TRIG_MAX_ANGLE / 8) ==
         TRIG_MAX_ANGLE / 2);
  assert(opposite_split_angle(TRIG_MAX_ANGLE / 8, 7 * TRIG_MAX_ANGLE / 8) ==
         TRIG_MAX_ANGLE / 2);
  const GPoint centre = GPoint(100, 114);
  const GPoint expected[] = {GPoint(100, 64), GPoint(150, 114), GPoint(100, 164), GPoint(50, 114)};
  for (unsigned i = 0; i < 4; i++) {
    const GPoint point = point_on_ray(centre, 50, i * TRIG_MAX_ANGLE / 4);
    assert(point.x == expected[i].x && point.y == expected[i].y);
  }
  const GRect screens[] = {GRect(0, 0, 200, 228), GRect(0, 0, 260, 260)};
  for (size_t i = 0; i < ARRAY_LENGTH(screens); i++) {
    rebuild_geometry_cache(screens[i]);
    assert(s_state.cache_ready && bounds_equal(s_state.cached_bounds, screens[i]));
    assert(s_state.cached_centre.x == screens[i].size.w / 2);
    assert(s_state.cached_centre.y == screens[i].size.h / 2);
    assert(s_state.cached_min_dimension == screens[i].size.w);
    const double distance = hypot(screens[i].size.w / 2, screens[i].size.h / 2);
    assert(s_state.cached_radius > distance);
    assert(s_state.cached_radius <= distance + 4);
  }
  assert(!bounds_equal(screens[0], screens[1]));
}

static void test_sort_rays(void) {
  const unsigned permutations[][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                      {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  for (size_t p = 0; p < ARRAY_LENGTH(permutations); p++) {
    SectorRay rays[3];
    for (unsigned i = 0; i < 3; i++) {
      const unsigned source = permutations[p][i];
      rays[i] = (SectorRay){.angle = (int32_t)source * 100, .colour = {.argb = source + 1}};
    }
    sort_rays(rays, 3);
    for (unsigned i = 0; i < 3; i++) {
      assert(rays[i].angle == (int32_t)i * 100 && rays[i].colour.argb == i + 1);
    }
  }
  SectorRay tied[] = {{.angle = 0, .colour = {.argb = 1}},
                      {.angle = 0, .colour = {.argb = 2}},
                      {.angle = 32768, .colour = {.argb = 3}}};
  sort_rays(tied, ARRAY_LENGTH(tied));
  assert(tied[0].colour.argb == 1 && tied[1].colour.argb == 2);
  sort_rays(NULL, 0);
}

int main(void) {
  test_angles_and_geometry_cache();
  test_sort_rays();
  test_sector_edge_cases();
  test_all_minute_sectors();
  printf("Geometry tests passed: 2880 minute layouts, %u interior samples, wrap and degenerate sectors\n",
         s_sample_count);
  return 0;
}
