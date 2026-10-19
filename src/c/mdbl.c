#include <pebble.h>
#include <string.h>

#include "mdbl.h"
#include "mdbl_settings.h"
#include "message_keys.auto.h"

AppState s_state;

void update_now(void) {
  time_t now_secs = time(NULL);
  struct tm *local = localtime(&now_secs);
  if (local) {
    s_state.now = *local;
  } else {
    memset(&s_state.now, 0, sizeof(s_state.now));
  }
}

void init_state(void) {
  update_now();

  s_state.mode = FACE_MODE_HANDS;
  s_state.show_date_tiles = false;
  s_state.bg_colour = GColorFromRGB(85, 85, 85);
  s_state.hour_colour = GColorBlack;
  s_state.minute_colour = GColorFromRGB(170, 170, 170);
  s_state.hour_hand_colour = GColorWhite;
  s_state.minute_hand_colour = GColorFromRGB(255, 170, 85);
  s_state.complication_bg_colour = GColorBlack;
  s_state.complication_border_colour = GColorFromRGB(170, 170, 170);
  s_state.complication_text_colour = GColorWhite;
  s_state.show_hands = true;
  s_state.show_texture = false;
  s_state.twelve_hour = false;
  s_state.show_date_complication = true;
  s_state.cache_ready = false;

  load_settings(&s_state);
  apply_debug_state(&s_state);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  (void)units_changed;
  s_state.now = *tick_time;
  apply_debug_state(&s_state);
  if (s_state.canvas) {
    layer_mark_dirty(s_state.canvas);
  }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  (void)context;
  if (should_ignore_config_messages()) {
    return;
  }
  const WatchSettings previous = capture_settings(&s_state);

  Tuple *bg = dict_find(iter, MESSAGE_KEY_BG_COLOUR);
  Tuple *hour = dict_find(iter, MESSAGE_KEY_HOUR_COLOUR);
  Tuple *minute = dict_find(iter, MESSAGE_KEY_MINUTE_COLOUR);
  Tuple *mode = dict_find(iter, MESSAGE_KEY_FACE_MODE);
  Tuple *date = dict_find(iter, MESSAGE_KEY_SHOW_DATE_TILES);
  Tuple *date_complication = dict_find(iter, MESSAGE_KEY_SHOW_DATE_COMPLICATION);
  Tuple *comp_bg_colour = dict_find(iter, MESSAGE_KEY_C_BG_COLOUR);
  Tuple *comp_border_colour = dict_find(iter, MESSAGE_KEY_C_BORDER_COLOUR);
  Tuple *comp_text_colour = dict_find(iter, MESSAGE_KEY_C_TEXT_COLOUR);
  Tuple *hour_hand_colour = dict_find(iter, MESSAGE_KEY_H_HAND_COLOUR);
  Tuple *minute_hand_colour = dict_find(iter, MESSAGE_KEY_M_HAND_COLOUR);
  Tuple *show_hands = dict_find(iter, MESSAGE_KEY_SHOW_HANDS);
  Tuple *show_texture = dict_find(iter, MESSAGE_KEY_SHOW_TEXTURE);
  Tuple *twelve_hour = dict_find(iter, MESSAGE_KEY_TWELVE_HOUR);
  Tuple *hand_colour = dict_find(iter, MESSAGE_KEY_HAND_COLOUR);

  if (bg && bg->type == TUPLE_CSTRING) {
    parse_hex_colour(bg->value->cstring, s_state.bg_colour, &s_state.bg_colour);
  }

  if (hour && hour->type == TUPLE_CSTRING) {
    parse_hex_colour(hour->value->cstring, s_state.hour_colour, &s_state.hour_colour);
  }

  if (minute && minute->type == TUPLE_CSTRING) {
    parse_hex_colour(minute->value->cstring, s_state.minute_colour, &s_state.minute_colour);
  }

  if (hour_hand_colour && hour_hand_colour->type == TUPLE_CSTRING) {
    parse_hex_colour(hour_hand_colour->value->cstring, s_state.hour_hand_colour, &s_state.hour_hand_colour);
  }

  if (minute_hand_colour && minute_hand_colour->type == TUPLE_CSTRING) {
    parse_hex_colour(minute_hand_colour->value->cstring, s_state.minute_hand_colour, &s_state.minute_hand_colour);
  }

  if (comp_bg_colour && comp_bg_colour->type == TUPLE_CSTRING) {
    parse_hex_colour(comp_bg_colour->value->cstring, s_state.complication_bg_colour, &s_state.complication_bg_colour);
  }

  if (comp_border_colour && comp_border_colour->type == TUPLE_CSTRING) {
    parse_hex_colour(comp_border_colour->value->cstring, s_state.complication_border_colour, &s_state.complication_border_colour);
  }

  if (comp_text_colour && comp_text_colour->type == TUPLE_CSTRING) {
    parse_hex_colour(comp_text_colour->value->cstring, s_state.complication_text_colour, &s_state.complication_text_colour);
  }

  if (mode && mode->type == TUPLE_CSTRING) {
    s_state.mode = parse_mode(mode->value->cstring);
    if (s_state.mode == FACE_MODE_LARGE_DIGITAL) {
      s_state.show_date_tiles = false;
    }
  }

  if (date && (date->type == TUPLE_INT || date->type == TUPLE_UINT) &&
      s_state.mode != FACE_MODE_LARGE_DIGITAL) {
    s_state.show_date_tiles = date->value->int32 == 1;
  }

  if (date_complication && (date_complication->type == TUPLE_INT || date_complication->type == TUPLE_UINT)) {
    s_state.show_date_complication = date_complication->value->int32 == 1;
  }

  if (show_hands && (show_hands->type == TUPLE_INT || show_hands->type == TUPLE_UINT)) {
    s_state.show_hands = show_hands->value->int32 == 1;
  }

  if (show_texture && (show_texture->type == TUPLE_INT || show_texture->type == TUPLE_UINT)) {
    s_state.show_texture = show_texture->value->int32 == 1;
  }

  if (twelve_hour && (twelve_hour->type == TUPLE_INT || twelve_hour->type == TUPLE_UINT)) {
    s_state.twelve_hour = twelve_hour->value->int32 == 1;
  }

  if (hand_colour && (hand_colour->type == TUPLE_INT || hand_colour->type == TUPLE_UINT)) {
    const GColor legacy_colour = hand_colour->value->int32 == 1 ? GColorWhite : GColorBlack;

    if (!hour_hand_colour || hour_hand_colour->type != TUPLE_CSTRING) {
      s_state.hour_hand_colour = legacy_colour;
    }
    if (!minute_hand_colour || minute_hand_colour->type != TUPLE_CSTRING) {
      s_state.minute_hand_colour = legacy_colour;
    }
  }

  if (s_state.mode == FACE_MODE_LARGE_DIGITAL) {
    s_state.show_date_tiles = false;
  }

  const WatchSettings current = capture_settings(&s_state);
  if (settings_equal(&previous, &current)) {
    return;
  }
  if (!save_settings(&current)) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Could not persist watchface settings");
  }
  update_now();
  if (s_state.canvas) {
    layer_mark_dirty(s_state.canvas);
  }
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_state.canvas = layer_create(bounds);
  layer_set_update_proc(s_state.canvas, update_canvas);
  layer_add_child(root, s_state.canvas);
}

static void window_unload(Window *window) {
  (void)window;
  layer_destroy(s_state.canvas);
  s_state.canvas = NULL;
}

static void init(void) {
  init_state();
  s_state.window = window_create();
  window_set_background_color(s_state.window, GColorBlack);
  window_set_window_handlers(s_state.window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(s_state.window, true);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(512, 64);
  if (s_state.canvas) {
    layer_mark_dirty(s_state.canvas);
  }
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  release_render_resources();
  window_destroy(s_state.window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
