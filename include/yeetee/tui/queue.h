#ifndef YT_TUI_QUEUE_H
#define YT_TUI_QUEUE_H

#include <stdint.h>
#include <notcurses/notcurses.h>
#include <yeetee/yeetee.h>

#define YT_QUEUE_MAX 256

typedef struct yt_queue
{
    yt_video_t items[YT_QUEUE_MAX];
    uint32_t cunt;
    uint32_t current_idx;
    uint32_t selected_idx;
    uint32_t scroll_offset;
} yt_queue_t;

uint32_t yt_queue_init(yt_queue_t *q);
uint32_t yt_queue_push(yt_queue_t *q, const yt_video_t *video);
uint32_t yt_queue_clear(yt_queue_t *q);
const yt_video_t* yt_queue_current_get(const yt_queue_t *q);
uint32_t yt_queue_next(yt_queue_t *q);
uint32_t yt_queue_prev(yt_queue_t *q);
uint32_t yt_queue_shuffle(yt_queue_t *q);
uint32_t yt_queue_render(yt_queue_t *q, struct ncplane *plane);

#endif
