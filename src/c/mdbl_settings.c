#include "mdbl_settings.h"

#include <string.h>

#define SETTINGS_KEY 0x5452494f
#define SETTINGS_VERSION 1
#define SETTINGS_DATE_TILES 1
#define SETTINGS_HANDS 2
#define SETTINGS_DATE_COMPLICATION 4
#define SETTINGS_TEXTURE 8
#define SETTINGS_FLAGS 15

WatchSettings capture_settings(const AppState *state) {
  return (WatchSettings){
    .version = SETTINGS_VERSION,
    .mode = state->mode,
    .flags = (state->show_date_tiles ? SETTINGS_DATE_TILES : 0) |
             (state->show_hands ? SETTINGS_HANDS : 0) |
             (state->show_date_complication ? SETTINGS_DATE_COMPLICATION : 0) |
             (state->show_texture ? SETTINGS_TEXTURE : 0),
    .colours = {
      state->bg_colour.argb, state->hour_colour.argb, state->minute_colour.argb,
      state->hour_hand_colour.argb, state->minute_hand_colour.argb,
      state->complication_bg_colour.argb, state->complication_border_colour.argb,
      state->complication_text_colour.argb
    }
  };
}

bool settings_equal(const WatchSettings *left, const WatchSettings *right) {
  return memcmp(left, right, sizeof(*left)) == 0;
}

bool load_settings(AppState *state) {
  WatchSettings settings;
  if (persist_get_size(SETTINGS_KEY) != (int)sizeof(settings) ||
      persist_read_data(SETTINGS_KEY, &settings, sizeof(settings)) != (int)sizeof(settings) ||
      settings.version != SETTINGS_VERSION || settings.mode > FACE_MODE_TICKS ||
      (settings.flags & ~SETTINGS_FLAGS) != 0) {
    return false;
  }
  for (size_t i = 0; i < ARRAY_LENGTH(settings.colours); i++) {
    if ((settings.colours[i] & 0xc0) != 0xc0) {
      return false;
    }
  }

  state->mode = (FaceMode)settings.mode;
  state->show_date_tiles = (settings.flags & SETTINGS_DATE_TILES) != 0 &&
                           state->mode != FACE_MODE_LARGE_DIGITAL;
  state->show_hands = (settings.flags & SETTINGS_HANDS) != 0;
  state->show_date_complication = (settings.flags & SETTINGS_DATE_COMPLICATION) != 0;
  state->show_texture = (settings.flags & SETTINGS_TEXTURE) != 0;
  state->bg_colour.argb = settings.colours[0];
  state->hour_colour.argb = settings.colours[1];
  state->minute_colour.argb = settings.colours[2];
  state->hour_hand_colour.argb = settings.colours[3];
  state->minute_hand_colour.argb = settings.colours[4];
  state->complication_bg_colour.argb = settings.colours[5];
  state->complication_border_colour.argb = settings.colours[6];
  state->complication_text_colour.argb = settings.colours[7];
  return true;
}

bool save_settings(const WatchSettings *settings) {
  return persist_write_data(SETTINGS_KEY, settings, sizeof(*settings)) == (int)sizeof(*settings);
}
