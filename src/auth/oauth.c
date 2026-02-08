#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <dangling/net/curl.h>

#include <yeetee/yeetee.h>
#include <yeetee/core/err.h>
#include <yeetee/auth/oauth.h>
#include <yeetee/auth/token.h>
#include <yeetee/api/json.h>

#define OAUTH_DEVICE_URL "https://oauth2.googleapis.com/device/code"
#define OAUTH_TOKEN_URL "https://oauth2.googleapis.com/token"
#define OAUTH_SCOPE "https://www.googleapis.com/auth/youtube"

#define OAUTH_BODY_MAX 2048
#define OAUTH_DEFAULT_MAX 300
#define OAUTH_SLOWDOWN_ADD 5

// oauth

uint32_t yt_oauth_device_code_req(ldg_curl_easy_ctx_t *curl, const char *client_id, yt_oauth_device_resp_t *resp)
{
    if (LDG_UNLIKELY(!curl)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!client_id)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!resp)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char body[OAUTH_BODY_MAX] = LDG_ARR_ZERO_INIT;
    struct curl_slist *headers = 0x0;
    ldg_curl_resp_t http_resp = LDG_STRUCT_ZERO_INIT;

    ldg_curl_resp_init(&http_resp);

    snprintf(body, sizeof(body), "client_id=%s&scope=%s", client_id, OAUTH_SCOPE);

    ret = ldg_curl_headers_append(&headers, "Content-Type: application/x-www-form-urlencoded");
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ret = ldg_curl_easy_post(curl, OAUTH_DEVICE_URL, body, headers, &http_resp);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        ldg_curl_headers_destroy(&headers);
        ldg_curl_resp_free(&http_resp);
        return YT_ERR_AUTH_DEVICE_CODE;
    }

    ret = yt_json_device_code_parse(http_resp.data, http_resp.size, resp->device_code, sizeof(resp->device_code), resp->user_code, sizeof(resp->user_code), resp->verify_url, sizeof(resp->verify_url), &resp->interval, &resp->expires_in);

    ldg_curl_headers_destroy(&headers);
    ldg_curl_resp_free(&http_resp);
    return ret;
}

uint32_t yt_oauth_poll(ldg_curl_easy_ctx_t *curl, const char *client_id, const char *client_secret, const char *device_code, uint32_t interval, yt_token_t *token)
{
    if (LDG_UNLIKELY(!curl)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!client_id)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!client_secret)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!device_code)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!token)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char body[OAUTH_BODY_MAX] = LDG_ARR_ZERO_INIT;
    struct curl_slist *headers = 0x0;
    ldg_curl_resp_t http_resp = LDG_STRUCT_ZERO_INIT;
    uint32_t elapsed = 0;
    uint32_t poll_interval = interval;
    uint32_t max_secs = OAUTH_DEFAULT_MAX;

    snprintf(body, sizeof(body), "client_id=%s&client_secret=%s&device_code=%s"
        "&grant_type=urn:ietf:params:oauth:grant-type:device_code", client_id, client_secret, device_code);

    ret = ldg_curl_headers_append(&headers, "Content-Type: application/x-www-form-urlencoded");
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    while (elapsed < max_secs)
    {
        ldg_curl_resp_init(&http_resp);

        ret = ldg_curl_easy_post(curl, OAUTH_TOKEN_URL, body, headers, &http_resp);
        if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
        {
            ldg_curl_resp_free(&http_resp);
            sleep(poll_interval);
            elapsed += poll_interval;
            continue;
        }

        ret = yt_json_token_parse(http_resp.data, http_resp.size, token);
        if (ret == LDG_ERR_AOK)
        {
            ldg_curl_resp_free(&http_resp);
            ldg_curl_headers_destroy(&headers);
            return LDG_ERR_AOK;
        }

        if (http_resp.data && strstr(http_resp.data, "slow_down")) { poll_interval += OAUTH_SLOWDOWN_ADD; }

        ldg_curl_resp_free(&http_resp);
        sleep(poll_interval);
        elapsed += poll_interval;
    }

    ldg_curl_headers_destroy(&headers);
    return YT_ERR_AUTH_POLL;
}
