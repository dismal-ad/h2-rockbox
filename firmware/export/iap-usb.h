#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "config.h"

static inline void iap_on_track_time_position(uint32_t pos_ms) {
    (void)pos_ms;
}
static inline void iap_on_track_playback_index(uint32_t index, bool track_ready) {
    (void)index;
    (void)track_ready;
}
static inline void iap_on_tracks_count(uint32_t count) {
    (void)count;
}
static inline void iap_on_play_status(int status) {
    (void)status;
}
static inline void iap_on_volume(int volume) {
    (void)volume;
}
static inline void iap_on_shuffle_state(bool state) {
    (void)state;
}
static inline void iap_on_repeat_state(int state) {
    (void)state;
}
