#ifndef YT_TUI_THUMB_H
#define YT_TUI_THUMB_H

#include <stdint.h>
#include <notcurses/notcurses.h>
#include <yeetee/yeetee.h>

typedef struct yt_thumb_entry
{
    char video_id[YT_VIDEO_ID_MAX];
    struct ncvisual *vis;
    uint64_t last_used;
    uint8_t loaded;
    uint8_t pudding[7];
} yt_thumb_entry_t;

typedef struct yt_thumb_cache
{
    yt_thumb_entry_t *entries;
    uint32_t capacity;
    uint32_t cunt;
} yt_thumb_cache_t;

uint32_t yt_thumb_cache_init(yt_thumb_cache_t *cache, uint32_t capacity);
void yt_thumb_cache_shutdown(yt_thumb_cache_t *cache);
struct ncvisual* yt_thumb_cache_get(yt_thumb_cache_t *cache, const char *video_id);
uint32_t yt_thumb_cache_put(yt_thumb_cache_t *cache, const char *video_id, struct ncvisual *vis);
uint32_t yt_thumb_fetch(const char *url, const char *cache_dir, const char *video_id, struct ncvisual **vis_out);

#endif
