#ifndef YT_AUTH_OAUTH_H
#define YT_AUTH_OAUTH_H

#include <stdint.h>
#include <dangling/net/curl.h>
#include <yeetee/yeetee.h>

#define YT_OAUTH_USER_CODE_MAX 32
#define YT_OAUTH_DEVICE_CODE_MAX 512
#define YT_OAUTH_VERIFY_URL_MAX 256

typedef struct yt_oauth_device_resp
{
    char device_code[YT_OAUTH_DEVICE_CODE_MAX];
    char user_code[YT_OAUTH_USER_CODE_MAX];
    char verify_url[YT_OAUTH_VERIFY_URL_MAX];
    uint32_t interval;
    uint32_t expires_in;
} yt_oauth_device_resp_t;

uint32_t yt_oauth_device_code_req(ldg_curl_easy_ctx_t *curl, const char *client_id, yt_oauth_device_resp_t *resp);
uint32_t yt_oauth_poll(ldg_curl_easy_ctx_t *curl, const char *client_id, const char *client_secret, const char *device_code, uint32_t interval, yt_token_t *token);

#endif
