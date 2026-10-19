#pragma once

#include "mdbl.h"

// Byte fields keep the persisted format independent of struct padding and pointers.
typedef struct WatchSettings {
  uint8_t version;
  uint8_t mode;
  uint8_t flags;
  uint8_t colours[8];
} WatchSettings;

WatchSettings capture_settings(const AppState *state);
bool settings_equal(const WatchSettings *left, const WatchSettings *right);
bool load_settings(AppState *state);
bool save_settings(const WatchSettings *settings);
