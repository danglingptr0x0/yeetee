#ifndef YT_API_INNERTUBE_H
#define YT_API_INNERTUBE_H

#include <stdint.h>
#include <dangling/net/curl.h>
#include <yeetee/yeetee.h>

#define YT_INNERTUBE_BASE_URL "https://www.youtube.com/youtubei/v1"
#define YT_INNERTUBE_API_KEY "AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8"
#define YT_INNERTUBE_WEB_CLIENT_NAME "WEB"
#define YT_INNERTUBE_WEB_CLIENT_VERSION "2.20250131.01.00"
#define YT_INNERTUBE_TV_CLIENT_NAME "TVHTML5"
#define YT_INNERTUBE_TV_CLIENT_VERSION "7.20241126.00.00"
#define YT_INNERTUBE_BROWSE_HOME "FEwhat_to_watch"

typedef struct yt_innertube_ctx
{
    ldg_curl_easy_ctx_t curl;
    char access_token[YT_TOKEN_ACCESS_MAX];
} yt_innertube_ctx_t;

uint32_t yt_innertube_ctx_init(yt_innertube_ctx_t *ctx, const char *access_token);
void yt_innertube_ctx_shutdown(yt_innertube_ctx_t *ctx);
uint32_t yt_innertube_browse(yt_innertube_ctx_t *ctx, const char *browse_id, yt_feed_t *feed);
uint32_t yt_innertube_search(yt_innertube_ctx_t *ctx, const char *query, yt_feed_t *feed);
uint32_t yt_innertube_player(yt_innertube_ctx_t *ctx, const char *video_id, yt_stream_set_t *streams);

#endif
