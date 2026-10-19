#include "mdbl.h"

#include <stdlib.h>
#include <string.h>

static uint8_t snap_channel(uint8_t value) {
  static const uint8_t steps[] = {0, 85, 170, 255};
  uint8_t best = steps[0];
  uint8_t best_distance = (uint8_t)abs((int)value - (int)best);

  for (size_t i = 1; i < ARRAY_LENGTH(steps); i++) {
    const uint8_t distance = (uint8_t)abs((int)value - (int)steps[i]);
    if (distance < best_distance) {
      best_distance = distance;
      best = steps[i];
    }
  }

  return best;
}

static int8_t parse_hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return (int8_t)(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return (int8_t)(10 + (c - 'a'));
  }
  if (c >= 'A' && c <= 'F') {
    return (int8_t)(10 + (c - 'A'));
  }
  return -1;
}

static bool parse_hex_byte(const char *text, uint8_t *out) {
  if (!text || !out) {
    return false;
  }

  const int8_t hi = parse_hex_nibble(text[0]);
  const int8_t lo = parse_hex_nibble(text[1]);
  if (hi < 0 || lo < 0) {
    return false;
  }

  *out = (uint8_t)((hi << 4) | lo);
  return true;
}

bool parse_hex_colour(const char *text, GColor fallback, GColor *out) {
  if (!text || !out) {
    return false;
  }

  while (*text == ' ') {
    text++;
  }

  if (*text == '#') {
    text++;
  }

  if (strlen(text) < 6) {
    *out = fallback;
    return false;
  }

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  if (!parse_hex_byte(&text[0], &r) ||
      !parse_hex_byte(&text[2], &g) ||
      !parse_hex_byte(&text[4], &b)) {
    *out = fallback;
    return false;
  }

  *out = GColorFromRGB(
    snap_channel(r),
    snap_channel(g),
    snap_channel(b)
  );
  return true;
}

FaceMode parse_mode(const char *text) {
  if (!text) {
    return FACE_MODE_HANDS;
  }

  if (strcmp(text, "digital") == 0) {
    return FACE_MODE_DIGITAL;
  }

  if (strcmp(text, "largedigital") == 0) {
    return FACE_MODE_LARGE_DIGITAL;
  }

  if (strcmp(text, "numbered") == 0) {
    return FACE_MODE_NUMBERED;
  }

  if (strcmp(text, "ticks") == 0) {
    return FACE_MODE_TICKS;
  }

  return FACE_MODE_HANDS;
}
