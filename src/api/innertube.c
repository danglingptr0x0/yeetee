#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <syslog.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <dangling/net/curl.h>

#include <yeetee/yeetee.h>
#include <yeetee/core/err.h>
#include <yeetee/api/innertube.h>
#include <yeetee/api/json.h>

#define INNERTUBE_BODY_MAX    4096
#define INNERTUBE_URL_MAX     512
#define INNERTUBE_HEADER_MAX  2560
#define INNERTUBE_WEB_UA      "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
#define INNERTUBE_TV_UA       "Mozilla/5.0 (SMART-TV; Linux; Tizen 5.0) AppleWebKit/537.36 (KHTML, like Gecko) SamsungBrowser/2.2 Chrome/63.0.3239.84 TV Safari/537.36"

// innertube

static uint32_t build_innertube_body(const char *param, uint8_t is_search, char *buff, size_t buff_len)
{
    int written = 0;

    if (is_search) { written = snprintf(buff, buff_len, "{\"context\":{\"client\":{\"clientName\":\"%s\",""\"clientVersion\":\"%s\",\"hl\":\"en\",\"gl\":\"US\"}},""\"query\":\"%s\"}", YT_INNERTUBE_TV_CLIENT_NAME, YT_INNERTUBE_TV_CLIENT_VERSION, param); }
    else{ written = snprintf(buff, buff_len, "{\"context\":{\"client\":{\"clientName\":\"%s\",""\"clientVersion\":\"%s\",\"hl\":\"en\",\"gl\":\"US\"}},""\"browseId\":\"%s\"}", YT_INNERTUBE_TV_CLIENT_NAME, YT_INNERTUBE_TV_CLIENT_VERSION, param); }

    if (LDG_UNLIKELY(written < 0 || (size_t)written >= buff_len)) { return LDG_ERR_OVERFLOW; }

    return LDG_ERR_AOK;
}

static uint32_t build_player_body(const char *video_id, char *buff, size_t buff_len)
{
    int written = 0;

    written = snprintf(buff, buff_len, "{\"context\":{\"client\":{\"clientName\":\"%s\","
        "\"clientVersion\":\"%s\",\"hl\":\"en\",\"gl\":\"US\"}},"
        "\"videoId\":\"%s\"}", YT_INNERTUBE_TV_CLIENT_NAME, YT_INNERTUBE_TV_CLIENT_VERSION, video_id);

    if (LDG_UNLIKELY(written < 0 || (size_t)written >= buff_len)) { return LDG_ERR_OVERFLOW; }

    return LDG_ERR_AOK;
}

static uint32_t build_innertube_headers(const char *access_token, uint8_t is_web, struct curl_slist **headers_out)
{
    uint32_t ret = LDG_ERR_AOK;
    char auth_hdr[INNERTUBE_HEADER_MAX] = LDG_ARR_ZERO_INIT;

    *headers_out = NULL;

    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", access_token);

    ret = ldg_curl_headers_append(headers_out, auth_hdr);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ret = ldg_curl_headers_append(headers_out, "Content-Type: application/json");
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        ldg_curl_headers_destroy(headers_out);
        return ret;
    }

    if (is_web)
    {
        ret = ldg_curl_headers_append(headers_out, "User-Agent: " INNERTUBE_WEB_UA);
        if (ret != LDG_ERR_AOK) { ldg_curl_headers_destroy(headers_out); return ret; }

        ret = ldg_curl_headers_append(headers_out, "Origin: https://www.youtube.com");
        if (ret != LDG_ERR_AOK) { ldg_curl_headers_destroy(headers_out); return ret; }

        ret = ldg_curl_headers_append(headers_out, "Referer: https://www.youtube.com/");
        if (ret != LDG_ERR_AOK) { ldg_curl_headers_destroy(headers_out); return ret; }

        ret = ldg_curl_headers_append(headers_out, "X-Youtube-Client-Name: 1");
        if (ret != LDG_ERR_AOK) { ldg_curl_headers_destroy(headers_out); return ret; }

        ret = ldg_curl_headers_append(headers_out, "X-Youtube-Client-Version: " YT_INNERTUBE_TV_CLIENT_VERSION);
        if (ret != LDG_ERR_AOK) { ldg_curl_headers_destroy(headers_out); return ret; }
    }
    else
    {
        ret = ldg_curl_headers_append(headers_out, "User-Agent: " INNERTUBE_TV_UA);
        if (ret != LDG_ERR_AOK) { ldg_curl_headers_destroy(headers_out); return ret; }
    }

    return LDG_ERR_AOK;
}

uint32_t yt_innertube_ctx_init(yt_innertube_ctx_t *ctx, const char *access_token)
{
    if (LDG_UNLIKELY(!ctx)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!access_token)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;

    memset(ctx, 0, sizeof(*ctx));

    ret = ldg_curl_easy_ctx_create(&ctx->curl);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return YT_ERR_API_INNERTUBE; }

    snprintf(ctx->access_token, sizeof(ctx->access_token), "%s", access_token);

    return LDG_ERR_AOK;
}

void yt_innertube_ctx_shutdown(yt_innertube_ctx_t *ctx)
{
    if (!ctx) { return; }

    ldg_curl_easy_ctx_destroy(&ctx->curl);
}

uint32_t yt_innertube_browse(yt_innertube_ctx_t *ctx, const char *browse_id, yt_feed_t *feed)
{
    if (LDG_UNLIKELY(!ctx)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!browse_id)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!feed)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char body[INNERTUBE_BODY_MAX] = LDG_ARR_ZERO_INIT;
    struct curl_slist *headers = NULL;
    ldg_curl_resp_t http_resp = LDG_STRUCT_ZERO_INIT;

    ret = build_innertube_body(browse_id, 0, body, sizeof(body));
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    syslog(LOG_DEBUG, "innertube browse; body: %.256s", body);

    ret = build_innertube_headers(ctx->access_token, 0, &headers);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ldg_curl_resp_init(&http_resp);

    ret = ldg_curl_easy_post(&ctx->curl, YT_INNERTUBE_BASE_URL "/browse", body, headers, &http_resp);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        syslog(LOG_ERR, "innertube browse; post failed; err: %u", ret);
        ldg_curl_headers_destroy(&headers);
        ldg_curl_resp_free(&http_resp);
        return YT_ERR_API_REQ;
    }

    syslog(LOG_DEBUG, "innertube browse; resp size: %zu", http_resp.size);
    if (http_resp.data && http_resp.size > 0) { syslog(LOG_DEBUG, "innertube browse; resp[0..1024]: %.1024s", http_resp.data); }

    ret = yt_json_feed_parse(http_resp.data, http_resp.size, feed);
    syslog(LOG_DEBUG, "innertube browse; parse ret: %u; video_cunt: %u", ret, feed->video_cunt);

    ldg_curl_headers_destroy(&headers);
    ldg_curl_resp_free(&http_resp);
    return ret;
}

uint32_t yt_innertube_search(yt_innertube_ctx_t *ctx, const char *query, yt_feed_t *feed)
{
    if (LDG_UNLIKELY(!ctx)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!query)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!feed)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char body[INNERTUBE_BODY_MAX] = LDG_ARR_ZERO_INIT;
    struct curl_slist *headers = NULL;
    ldg_curl_resp_t http_resp = LDG_STRUCT_ZERO_INIT;

    ret = build_innertube_body(query, 1, body, sizeof(body));
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ret = build_innertube_headers(ctx->access_token, 0, &headers);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ldg_curl_resp_init(&http_resp);

    ret = ldg_curl_easy_post(&ctx->curl, YT_INNERTUBE_BASE_URL "/search", body, headers, &http_resp);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        ldg_curl_headers_destroy(&headers);
        ldg_curl_resp_free(&http_resp);
        return YT_ERR_API_REQ;
    }

    ret = yt_json_search_parse(http_resp.data, http_resp.size, feed);

    ldg_curl_headers_destroy(&headers);
    ldg_curl_resp_free(&http_resp);
    return ret;
}

uint32_t yt_innertube_player(yt_innertube_ctx_t *ctx, const char *video_id, yt_stream_set_t *streams)
{
    if (LDG_UNLIKELY(!ctx)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!video_id)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!streams)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char body[INNERTUBE_BODY_MAX] = LDG_ARR_ZERO_INIT;
    struct curl_slist *headers = NULL;
    ldg_curl_resp_t http_resp = LDG_STRUCT_ZERO_INIT;

    ret = build_player_body(video_id, body, sizeof(body));
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ret = build_innertube_headers(ctx->access_token, 0, &headers);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ldg_curl_resp_init(&http_resp);

    ret = ldg_curl_easy_post(&ctx->curl, YT_INNERTUBE_BASE_URL "/player", body, headers, &http_resp);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        ldg_curl_headers_destroy(&headers);
        ldg_curl_resp_free(&http_resp);
        return YT_ERR_API_REQ;
    }

    ret = yt_json_player_parse(http_resp.data, http_resp.size, streams);

    ldg_curl_headers_destroy(&headers);
    ldg_curl_resp_free(&http_resp);
    return ret;
}
