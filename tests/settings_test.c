#include "mdbl_settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static WatchSettings s_stored;
static int s_stored_size;
static int s_read_result;
static int s_write_result;
static unsigned s_read_count;
static unsigned s_write_count;
static unsigned s_cases;

int persist_get_size(uint32_t key) {
  assert(key == 0x5452494f);
  return s_stored_size;
}

int persist_read_data(uint32_t key, void *buffer, size_t size) {
  assert(key == 0x5452494f);
  assert(size == sizeof(s_stored));
  s_read_count++;
  if (s_read_result > 0) {
    memcpy(buffer, &s_stored, (size_t)s_read_result);
  }
  return s_read_result;
}

int persist_write_data(uint32_t key, const void *buffer, size_t size) {
  assert(key == 0x5452494f);
  assert(size == sizeof(s_stored));
  s_write_count++;
  if (s_write_result == (int)size) {
    memcpy(&s_stored, buffer, size);
    s_stored_size = (int)size;
  }
  return s_write_result;
}

static void reset_storage(WatchSettings settings) {
  s_stored = settings;
  s_stored_size = sizeof(settings);
  s_read_result = sizeof(settings);
  s_write_result = sizeof(settings);
  s_read_count = 0;
  s_write_count = 0;
}

static AppState sample_state(void) {
  AppState state = {0};
  state.mode = FACE_MODE_DIGITAL;
  state.show_date_tiles = true;
  state.show_hands = true;
  state.show_date_complication = true;
  state.bg_colour.argb = 0xc0;
  state.hour_colour.argb = 0xc1;
  state.minute_colour.argb = 0xd2;
  state.hour_hand_colour.argb = 0xe3;
  state.minute_hand_colour.argb = 0xf4;
  state.complication_bg_colour.argb = 0xc5;
  state.complication_border_colour.argb = 0xd6;
  state.complication_text_colour.argb = 0xff;
  return state;
}

static void test_roundtrip(void) {
  for (unsigned mode = FACE_MODE_HANDS; mode <= FACE_MODE_TICKS; mode++) {
    for (unsigned flags = 0; flags < 32; flags++) {
      AppState source = sample_state();
      source.mode = (FaceMode)mode;
      source.show_date_tiles = (flags & 1) != 0;
      source.show_hands = (flags & 2) != 0;
      source.show_date_complication = (flags & 4) != 0;
      source.show_texture = (flags & 8) != 0;
      source.twelve_hour = (flags & 16) != 0;
      WatchSettings settings = capture_settings(&source);
      assert(settings.version == 1);
      assert(settings.mode == mode);
      assert(settings.flags == flags);
      reset_storage(settings);
      assert(save_settings(&settings));
      assert(s_write_count == 1);

      AppState target = {0};
      // Loading configuration must preserve runtime state and geometry.
      target.now.tm_year = 126;
      target.now.tm_min = 37;
      target.cached_radius = 170;
      target.cache_ready = true;
      assert(load_settings(&target));
      WatchSettings restored = capture_settings(&target);
      if (mode == FACE_MODE_LARGE_DIGITAL) {
        settings.flags &= (uint8_t)~1;
      }
      assert(settings_equal(&settings, &restored));
      assert(target.now.tm_year == 126);
      assert(target.now.tm_min == 37);
      assert(target.cached_radius == 170);
      assert(target.cache_ready);
      assert(target.show_texture == ((flags & 8) != 0));
      assert(target.twelve_hour == ((flags & 16) != 0));
      assert(s_read_count == 1);
      assert(s_write_count == 1); // Restoring settings does not write flash.
      s_cases++;
    }
  }
}

static void assert_rejected_unchanged(void) {
  AppState target = sample_state();
  AppState before;
  memcpy(&before, &target, sizeof(before));
  assert(!load_settings(&target));
  assert(memcmp(&before, &target, sizeof(target)) == 0);
  assert(s_write_count == 0);
  s_cases++;
}

static void test_invalid_storage(void) {
  const AppState source = sample_state();
  const WatchSettings valid = capture_settings(&source);
  const int bad_sizes[] = {-1, 0, 1, sizeof(valid) - 1, sizeof(valid) + 1};
  for (size_t i = 0; i < ARRAY_LENGTH(bad_sizes); i++) {
    reset_storage(valid);
    s_stored_size = bad_sizes[i];
    assert_rejected_unchanged();
    assert(s_read_count == 0);
  }
  for (int length = -1; length < (int)sizeof(valid); length++) {
    reset_storage(valid);
    s_read_result = length;
    assert_rejected_unchanged();
  }
  const uint8_t bad_versions[] = {0, 2, 255};
  for (size_t i = 0; i < ARRAY_LENGTH(bad_versions); i++) {
    reset_storage(valid);
    s_stored.version = bad_versions[i];
    assert_rejected_unchanged();
  }
  reset_storage(valid);
  s_stored.mode = FACE_MODE_TICKS + 1;
  assert_rejected_unchanged();
  reset_storage(valid);
  s_stored.mode = 255;
  assert_rejected_unchanged();
  for (unsigned bit = 5; bit < 8; bit++) {
    reset_storage(valid);
    s_stored.flags |= (uint8_t)(1u << bit);
    assert_rejected_unchanged();
  }
  for (size_t colour = 0; colour < ARRAY_LENGTH(valid.colours); colour++) {
    for (unsigned alpha = 0; alpha < 3; alpha++) {
      reset_storage(valid);
      s_stored.colours[colour] = (uint8_t)((alpha << 6) | 0x3f);
      assert_rejected_unchanged();
    }
  }
}

static void test_legacy_texture_default(void) {
  const AppState source = sample_state();
  const WatchSettings original = capture_settings(&source);
  for (unsigned flags = 0; flags < 8; flags++) {
    reset_storage(original);
    s_stored.flags = flags;
    AppState target = sample_state();
    target.show_texture = true;
    assert(load_settings(&target));
    assert(!target.show_texture);
    assert(s_write_count == 0);
    s_cases++;
  }
  AppState disabled = sample_state();
  WatchSettings without_texture = capture_settings(&disabled);
  disabled.show_texture = true;
  const WatchSettings with_texture = capture_settings(&disabled);
  assert((without_texture.flags ^ with_texture.flags) == 8);
  assert(!settings_equal(&without_texture, &with_texture));
  assert(with_texture.version == without_texture.version);
}

static void test_legacy_clock_default(void) {
  const AppState source = sample_state();
  for (unsigned flags = 0; flags < 16; flags++) {
    reset_storage(capture_settings(&source));
    s_stored.flags = flags;
    AppState target = sample_state();
    target.twelve_hour = true;
    assert(load_settings(&target));
    assert(!target.twelve_hour);
    assert(s_write_count == 0);
    s_cases++;
  }
}

static void test_settings_comparison(void) {
  const AppState source = sample_state();
  const WatchSettings initial = capture_settings(&source);
  WatchSettings changed = initial;
  assert(settings_equal(&initial, &changed));
  for (size_t i = 0; i < sizeof(changed); i++) {
    changed = initial;
    ((uint8_t *)&changed)[i] ^= 1;
    assert(!settings_equal(&initial, &changed));
    s_cases++;
  }
}

static void test_write_errors(void) {
  const AppState source = sample_state();
  const WatchSettings settings = capture_settings(&source);
  for (int result = -1; result < (int)sizeof(settings); result++) {
    reset_storage(settings);
    s_write_result = result;
    assert(!save_settings(&settings));
    assert(s_write_count == 1);
    s_cases++;
  }
}

int main(void) {
  assert(sizeof(WatchSettings) == 11);
  // Previously persisted modes keep their numeric identities as modes are added.
  assert(FACE_MODE_HANDS == 0);
  assert(FACE_MODE_DIGITAL == 1);
  assert(FACE_MODE_LARGE_DIGITAL == 2);
  assert(FACE_MODE_NUMBERED == 3);
  assert(FACE_MODE_TICKS == 4);
  test_roundtrip();
  test_legacy_texture_default();
  test_legacy_clock_default();
  test_invalid_storage();
  test_settings_comparison();
  test_write_errors();
  printf("Settings tests passed: %u cases\n", s_cases);
  return 0;
}
