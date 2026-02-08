#ifndef YT_PLAYER_RENDER_H
#define YT_PLAYER_RENDER_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <mpv/client.h>
#include <mpv/render.h>
#include <notcurses/notcurses.h>

typedef struct yt_render
{
    mpv_render_context *ctx;
    struct ncplane *video_plane;
    uint8_t *shm_map[2];
    size_t shm_size;
    int shm_fd[2];
    uint32_t pixel_w;
    uint32_t pixel_h;
    size_t stride;
    uint32_t video_abs_y;
    uint32_t video_abs_x;
    uint32_t video_cell_cols;
    uint32_t video_cell_rows;
    uint64_t fps_epoch_ns;
    uint32_t frame_cunt;
    uint32_t fps;
    uint32_t esc_len[2];
    pthread_t render_thread;
    pthread_mutex_t stdout_mut;
    volatile uint8_t frame_ready;
    volatile uint8_t running;
    uint8_t back;
    uint8_t pudding[5];
    char esc_cmd[2][256];
} yt_render_t;

uint32_t yt_render_init(yt_render_t *render, mpv_handle *mpv, struct ncplane *parent, uint32_t cell_rows, uint32_t cell_cols);
void yt_render_shutdown(yt_render_t *render);
void yt_render_stdout_lock(yt_render_t *render);
void yt_render_stdout_unlock(yt_render_t *render);

#endif
