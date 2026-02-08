#ifndef YT_AUTH_TOKEN_H
#define YT_AUTH_TOKEN_H

#include <stdint.h>
#include <dangling/net/curl.h>
#include <yeetee/yeetee.h>

uint32_t yt_token_store(const yt_token_t *token, const char *data_dir);
uint32_t yt_token_load(yt_token_t *token, const char *data_dir);
uint32_t yt_token_refresh(ldg_curl_easy_ctx_t *curl, const char *client_id, const char *client_secret, yt_token_t *token);
uint32_t yt_token_expired_is(const yt_token_t *token);

#endif
