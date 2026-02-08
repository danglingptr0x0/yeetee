#ifndef YT_API_JSON_H
#define YT_API_JSON_H

#include <stdint.h>
#include <yeetee/yeetee.h>

uint32_t yt_json_feed_parse(const char *data, size_t len, yt_feed_t *feed);
uint32_t yt_json_search_parse(const char *data, size_t len, yt_feed_t *feed);
uint32_t yt_json_player_parse(const char *data, size_t len, yt_stream_set_t *streams);
uint32_t yt_json_token_parse(const char *data, size_t len, yt_token_t *token);
uint32_t yt_json_device_code_parse(const char *data, size_t len, char *device_code, size_t dc_len, char *user_code, size_t uc_len, char *verify_url, size_t vu_len, uint32_t *interval, uint32_t *expires_in);

#endif
