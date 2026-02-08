#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <dangling/net/curl.h>
#include <notcurses/notcurses.h>
#include <yeetee/core/err.h>
#include <yeetee/tui/thumb.h>

#define YT_THUMB_PATH_MAX 1024

uint32_t yt_thumb_cache_init(yt_thumb_cache_t *cache, uint32_t capacity)
{
    if (LDG_UNLIKELY(!cache)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(capacity == 0)) { return LDG_ERR_FUNC_ARG_INVALID; }

    cache->entries = malloc(capacity * sizeof(yt_thumb_entry_t));
    if (LDG_UNLIKELY(!cache->entries)) { return LDG_ERR_ALLOC_NULL; }

    memset(cache->entries, 0, capacity * sizeof(yt_thumb_entry_t));
    cache->capacity = capacity;
    cache->cunt = 0;

    return LDG_ERR_AOK;
}

void yt_thumb_cache_shutdown(yt_thumb_cache_t *cache)
{
    if (LDG_UNLIKELY(!cache)) { return; }

    if (LDG_UNLIKELY(!cache->entries)) { return; }

    uint32_t i = 0;
    for (; i < cache->cunt; i++) { if (cache->entries[i].loaded)
        {
            ncvisual_destroy(cache->entries[i].vis);
            cache->entries[i].vis = NULL;
        }
    }

    free(cache->entries);
    cache->entries = NULL;
    cache->cunt = 0;
    cache->capacity = 0;
}

struct ncvisual* yt_thumb_cache_get(yt_thumb_cache_t *cache, const char *video_id)
{
    if (LDG_UNLIKELY(!cache)) { return NULL; }

    if (LDG_UNLIKELY(!video_id)) { return NULL; }

    uint32_t i = 0;
    for (; i < cache->cunt; i++) { if (cache->entries[i].loaded && strncmp(cache->entries[i].video_id, video_id, YT_VIDEO_ID_MAX) == 0)
        {
            cache->entries[i].last_used = (uint64_t)time(NULL);
            return cache->entries[i].vis;
        }
    }

    return NULL;
}

uint32_t yt_thumb_cache_put(yt_thumb_cache_t *cache, const char *video_id, struct ncvisual *vis)
{
    if (LDG_UNLIKELY(!cache)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!video_id)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!vis)) { return LDG_ERR_FUNC_ARG_NULL; }

    yt_thumb_entry_t *slot = NULL;

    if (cache->cunt < cache->capacity)
    {
        slot = &cache->entries[cache->cunt];
        cache->cunt++;
    }
    else
    {
        uint64_t oldest = UINT64_MAX;
        uint32_t oldest_idx = 0;
        uint32_t i = 0;

        for (; i < cache->cunt; i++) { if (cache->entries[i].last_used < oldest)
            {
                oldest = cache->entries[i].last_used;
                oldest_idx = i;
            }
        }

        slot = &cache->entries[oldest_idx];
        if (slot->loaded) { ncvisual_destroy(slot->vis); }
    }

    strncpy(slot->video_id, video_id, YT_VIDEO_ID_MAX - 1);
    slot->video_id[YT_VIDEO_ID_MAX - 1] = LDG_STR_TERM;
    slot->vis = vis;
    slot->loaded = 1;
    slot->last_used = (uint64_t)time(NULL);

    return LDG_ERR_AOK;
}

uint32_t yt_thumb_fetch(const char *url, const char *cache_dir, const char *video_id, struct ncvisual **vis_out)
{
    if (LDG_UNLIKELY(!url)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!cache_dir)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!video_id)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!vis_out)) { return LDG_ERR_FUNC_ARG_NULL; }

    char path[YT_THUMB_PATH_MAX];
    snprintf(path, sizeof(path), "%s/thumb_%s.jpg", cache_dir, video_id);

    ldg_curl_easy_ctx_t curl_ctx;
    uint32_t ret = ldg_curl_easy_ctx_create(&curl_ctx);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ldg_curl_resp_t resp;
    ldg_curl_resp_init(&resp);

    ret = ldg_curl_easy_get(&curl_ctx, url, NULL, &resp);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        ldg_curl_resp_free(&resp);
        ldg_curl_easy_ctx_destroy(&curl_ctx);
        return ret;
    }

    FILE *fp = fopen(path, "wb");
    if (LDG_UNLIKELY(!fp))
    {
        ldg_curl_resp_free(&resp);
        ldg_curl_easy_ctx_destroy(&curl_ctx);
        return LDG_ERR_IO_OPEN;
    }

    size_t written = fwrite(resp.data, 1, resp.size, fp);
    fclose(fp);

    if (LDG_UNLIKELY(written != resp.size))
    {
        unlink(path);
        ldg_curl_resp_free(&resp);
        ldg_curl_easy_ctx_destroy(&curl_ctx);
        return LDG_ERR_IO_WRITE;
    }

    ldg_curl_resp_free(&resp);
    ldg_curl_easy_ctx_destroy(&curl_ctx);

    *vis_out = ncvisual_from_file(path);
    unlink(path);

    if (LDG_UNLIKELY(!*vis_out)) { return YT_ERR_TUI_RENDER; }

    return LDG_ERR_AOK;
}
