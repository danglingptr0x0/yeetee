#ifndef YT_API_YTDLP_H
#define YT_API_YTDLP_H

#include <stdint.h>
#include <yeetee/yeetee.h>

uint32_t yt_ytdlp_stream_url_get(const char *video_id, yt_stream_set_t *streams);
uint32_t yt_ytdlp_search(const char *query, uint32_t max_results, yt_feed_t *feed);

#endif
