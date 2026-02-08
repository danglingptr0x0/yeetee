#ifndef YT_TUI_FEED_H
#define YT_TUI_FEED_H

#include <stdint.h>
#include <notcurses/notcurses.h>
#include <yeetee/yeetee.h>
#include <yeetee/tui/thumb.h>

#define YT_FEED_MAX_VISIBLE 24

typedef struct yt_feed_ctx
{
    yt_feed_t feed;
    yt_thumb_cache_t thumbs;
    struct ncplane *thumb_planes[YT_FEED_MAX_VISIBLE];
    uint32_t thumb_plane_cunt;
    uint32_t selected_idx;
    uint32_t scroll_offset;
    uint8_t loading;
    uint8_t pudding[3];
} yt_feed_ctx_t;

uint32_t yt_feed_ctx_init(yt_feed_ctx_t *ctx, uint32_t thumb_cache_max);
void yt_feed_ctx_shutdown(yt_feed_ctx_t *ctx);
uint32_t yt_feed_render(yt_feed_ctx_t *ctx, struct ncplane *plane);
uint32_t yt_feed_nav_up(yt_feed_ctx_t *ctx);
uint32_t yt_feed_nav_down(yt_feed_ctx_t *ctx);
const yt_video_t* yt_feed_selected_get(const yt_feed_ctx_t *ctx);
void yt_feed_thumb_planes_destroy(yt_feed_ctx_t *ctx);

#endif
