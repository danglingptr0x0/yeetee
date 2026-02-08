#ifndef YT_PLAYER_PLAYER_H
#define YT_PLAYER_PLAYER_H

#include <stdint.h>
#include <pthread.h>
#include <mpv/client.h>
#include <dangling/thread/spsc.h>

typedef enum yt_player_state
{
    YT_PLAYER_STOPPED = 0,
    YT_PLAYER_PLAYING,
    YT_PLAYER_PAUSED,
    YT_PLAYER_LOADING,
    YT_PLAYER_ERROR
} yt_player_state_t;

typedef struct yt_player_event
{
    yt_player_state_t state;
    double time_pos;
    double duration;
    uint8_t eof_reached;
    uint8_t pudding[3];
} yt_player_event_t;

typedef struct yt_player
{
    mpv_handle *mpv;
    ldg_spsc_queue_t event_q;
    pthread_t event_thread;
    volatile uint8_t running;
    uint8_t pudding[7];
} yt_player_t;

uint32_t yt_player_init(yt_player_t *player);
void yt_player_shutdown(yt_player_t *player);
uint32_t yt_player_load(yt_player_t *player, const char *url);
uint32_t yt_player_pause(yt_player_t *player);
uint32_t yt_player_resume(yt_player_t *player);
uint32_t yt_player_seek(yt_player_t *player, double secs);
uint32_t yt_player_stop(yt_player_t *player);
uint32_t yt_player_volume_set(yt_player_t *player, uint32_t volume);

#endif
