#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <notcurses/notcurses.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <dangling/net/curl.h>
#include <dangling/thread/pool.h>
#include <dangling/thread/spsc.h>
#include <yeetee/core/err.h>
#include <yeetee/core/conf.h>
#include <yeetee/auth/oauth.h>
#include <yeetee/auth/token.h>
#include <yeetee/api/innertube.h>
#include <yeetee/api/ytdlp.h>
#include <yeetee/player/player.h>
#include <yeetee/player/render.h>
#include <yeetee/tui/layout.h>
#include <yeetee/tui/input.h>
#include <yeetee/tui/feed.h>
#include <yeetee/tui/queue.h>
#include <yeetee/tui/tui.h>

#define YT_TUI_POLL_NS        50000000
#define YT_TUI_SEEK_SECS 10.0
#define YT_TUI_VOL_STEP 5
#define YT_TUI_VIDEO_INFO_ROWS 3

// term rst
static void tui_term_reset(void)
{
    static const char esc[] =
        "\x1b_Ga=d,d=A,q=2;\x1b\\"
        "\x1b[?2026l"
        "\x1b[?1003l\x1b[?1006l\x1b[?1002l\x1b[?1000l"
        "\x1b[?25h"
        "\x1b[0m";

    ssize_t wr = write(STDOUT_FILENO, esc, sizeof(esc) - 1);
    if (wr < 0) { syslog(LOG_ERR, "tui_term_reset; write failed"); }
}

// signal cleanup cb
static void tui_signal_cleanup(int sig)
{
    static const char esc[] =
        "\x1b_Ga=d,d=A,q=2;\x1b\\"
        "\x1b[?25h"
        "\x1b[0m";

    write(STDOUT_FILENO, esc, sizeof(esc) - 1);
    signal(sig, SIG_DFL);
    raise(sig);
}

// auth task context
typedef struct tui_auth_task_ctx
{
    char client_id[YT_CONF_CLIENT_ID_MAX];
    char client_secret[YT_CONF_CLIENT_SECRET_MAX];
    char data_dir[YT_CONF_DATA_DIR_MAX];
    yt_oauth_device_resp_t device_resp;
    ldg_spsc_queue_t *result_q;
} tui_auth_task_ctx_t;

// feed task context
typedef struct tui_feed_task_ctx
{
    char access_token[YT_TOKEN_ACCESS_MAX];
    char browse_id[64];
    ldg_spsc_queue_t *result_q;
} tui_feed_task_ctx_t;

// search task context
typedef struct tui_search_task_ctx
{
    char access_token[YT_TOKEN_ACCESS_MAX];
    char query[YT_TUI_SEARCH_MAX];
    ldg_spsc_queue_t *result_q;
} tui_search_task_ctx_t;

// thumb task context
typedef struct tui_thumb_task_ctx
{
    char video_id[YT_VIDEO_ID_MAX];
    char thumb_url[YT_VIDEO_THUMB_MAX];
    char cache_dir[YT_CONF_CACHE_DIR_MAX];
    ldg_spsc_queue_t *result_q;
} tui_thumb_task_ctx_t;

// thumb result
typedef struct tui_thumb_result
{
    char video_id[YT_VIDEO_ID_MAX];
    struct ncvisual *vis;
} tui_thumb_result_t;

// auth worker
static void auth_poll_task(void *arg)
{
    tui_auth_task_ctx_t *ctx = (tui_auth_task_ctx_t *)arg;
    yt_token_t token;
    ldg_curl_easy_ctx_t curl;
    uint32_t ret = 0;

    memset(&token, 0, sizeof(token));

    ret = ldg_curl_easy_ctx_create(&curl);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        free(ctx);
        return;
    }

    ret = yt_oauth_poll(&curl, ctx->client_id, ctx->client_secret, ctx->device_resp.device_code, ctx->device_resp.interval, &token);
    if (ret == LDG_ERR_AOK)
    {
        yt_token_store(&token, ctx->data_dir);
        ldg_spsc_push(ctx->result_q, &token);
    }

    ldg_curl_easy_ctx_destroy(&curl);
    free(ctx);
}

// feed worker
static void feed_browse_task(void *arg)
{
    tui_feed_task_ctx_t *ctx = (tui_feed_task_ctx_t *)arg;
    yt_innertube_ctx_t api;
    yt_feed_t feed;
    uint32_t ret = 0;

    memset(&feed, 0, sizeof(feed));

    ret = yt_innertube_ctx_init(&api, ctx->access_token);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        ldg_spsc_push(ctx->result_q, &feed);
        free(ctx);
        return;
    }

    yt_innertube_browse(&api, ctx->browse_id, &feed);
    ldg_spsc_push(ctx->result_q, &feed);

    yt_innertube_ctx_shutdown(&api);
    free(ctx);
}

// search worker
static void search_task(void *arg)
{
    tui_search_task_ctx_t *ctx = (tui_search_task_ctx_t *)arg;
    yt_innertube_ctx_t api;
    yt_feed_t feed;
    uint32_t ret = 0;

    memset(&feed, 0, sizeof(feed));

    ret = yt_innertube_ctx_init(&api, ctx->access_token);
    if (ret == LDG_ERR_AOK)
    {
        ret = yt_innertube_search(&api, ctx->query, &feed);
        yt_innertube_ctx_shutdown(&api);
    }

    if (ret != LDG_ERR_AOK || feed.video_cunt == 0) { yt_ytdlp_search(ctx->query, 10, &feed); }

    ldg_spsc_push(ctx->result_q, &feed);

    free(ctx);
}

// thumb worker
static void thumb_fetch_task(void *arg)
{
    tui_thumb_task_ctx_t *ctx = (tui_thumb_task_ctx_t *)arg;
    struct ncvisual *vis = NULL;
    uint32_t ret = 0;

    ret = yt_thumb_fetch(ctx->thumb_url, ctx->cache_dir, ctx->video_id, &vis);
    if (ret == LDG_ERR_AOK && vis)
    {
        tui_thumb_result_t result;
        memset(&result, 0, sizeof(result));
        snprintf(result.video_id, sizeof(result.video_id), "%s", ctx->video_id);
        result.vis = vis;
        ldg_spsc_push(ctx->result_q, &result);
    }

    free(ctx);
}

// thumb submit
static void tui_thumb_request_all(yt_tui_t *tui)
{
    uint32_t i = 0;
    tui->thumb_req_cunt = 0;

    for (; i < tui->feed.feed.video_cunt; i++)
    {
        const yt_video_t *v = &tui->feed.feed.videos[i];
        if (v->thumb_url[0] == '\0') { continue; }

        if (yt_thumb_cache_get(&tui->feed.thumbs, v->id)) { continue; }

        tui_thumb_task_ctx_t *ctx = (tui_thumb_task_ctx_t *)malloc(sizeof(tui_thumb_task_ctx_t));
        if (LDG_UNLIKELY(!ctx)) { continue; }

        memset(ctx, 0, sizeof(*ctx));
        snprintf(ctx->video_id, sizeof(ctx->video_id), "%s", v->id);
        snprintf(ctx->thumb_url, sizeof(ctx->thumb_url), "%s", v->thumb_url);
        snprintf(ctx->cache_dir, sizeof(ctx->cache_dir), "%s", tui->conf->cache_dir);
        ctx->result_q = &tui->thumb_result_q;

        uint32_t ret = ldg_thread_pool_submit(&tui->pool, thumb_fetch_task, ctx);
        if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { free(ctx); continue; }

        tui->thumb_req_cunt++;
    }
}

// submit feed load
static void tui_feed_request(yt_tui_t *tui)
{
    if (tui->feed_req_pending) { return; }

    tui_feed_task_ctx_t *ctx = (tui_feed_task_ctx_t *)malloc(sizeof(tui_feed_task_ctx_t));
    if (LDG_UNLIKELY(!ctx)) { return; }

    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->access_token, sizeof(ctx->access_token), "%s", tui->token.access);
    snprintf(ctx->browse_id, sizeof(ctx->browse_id), "%s", YT_INNERTUBE_BROWSE_HOME);
    ctx->result_q = &tui->api_result_q;

    uint32_t ret = ldg_thread_pool_submit(&tui->pool, feed_browse_task, ctx);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        free(ctx);
        return;
    }

    tui->feed_req_pending = 1;
}

// submit auth flow
static void tui_auth_start(yt_tui_t *tui)
{
    if (tui->auth_started) { return; }

    tui->auth_started = 1;
    tui->auth_err = LDG_ERR_AOK;

    if (tui->conf->client_id[0] == '\0')
    {
        tui->auth_err = YT_ERR_AUTH_DEVICE_CODE;
        return;
    }

    ldg_curl_easy_ctx_t curl;
    uint32_t ret = ldg_curl_easy_ctx_create(&curl);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        tui->auth_err = ret;
        return;
    }

    memset(&tui->device_resp, 0, sizeof(tui->device_resp));
    ret = yt_oauth_device_code_req(&curl, tui->conf->client_id, &tui->device_resp);
    ldg_curl_easy_ctx_destroy(&curl);

    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        tui->auth_err = ret;
        return;
    }

    tui_auth_task_ctx_t *ctx = (tui_auth_task_ctx_t *)malloc(sizeof(tui_auth_task_ctx_t));
    if (LDG_UNLIKELY(!ctx))
    {
        tui->auth_err = LDG_ERR_ALLOC_NULL;
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->client_id, sizeof(ctx->client_id), "%s", tui->conf->client_id);
    snprintf(ctx->client_secret, sizeof(ctx->client_secret), "%s", tui->conf->client_secret);
    snprintf(ctx->data_dir, sizeof(ctx->data_dir), "%s", tui->conf->data_dir);
    memcpy(&ctx->device_resp, &tui->device_resp, sizeof(tui->device_resp));
    ctx->result_q = &tui->auth_result_q;

    ret = ldg_thread_pool_submit(&tui->pool, auth_poll_task, ctx);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        free(ctx);
        tui->auth_err = ret;
        return;
    }
}

// player
static uint32_t tui_player_ensure(yt_tui_t *tui)
{
    if (!tui->player_ready)
    {
        uint32_t ret = yt_player_init(&tui->player);
        if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

        tui->player_ready = 1;
    }

    if (!tui->render_active)
    {
        yt_feed_thumb_planes_destroy(&tui->feed);
        ncplane_erase(tui->layout.content);

        unsigned content_rows = 0;
        unsigned content_cols = 0;
        ncplane_dim_yx(tui->layout.content, &content_rows, &content_cols);

        uint32_t video_rows = (content_rows > YT_TUI_VIDEO_INFO_ROWS) ? content_rows - YT_TUI_VIDEO_INFO_ROWS : content_rows;
        uint32_t video_cols = content_cols * 95 / 100;

        uint32_t ret = yt_render_init(&tui->render, tui->player.mpv, tui->layout.content, video_rows, video_cols);
        if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
        {
            syslog(LOG_ERR, "tui_player_ensure; render init failed; ret: %u", ret);
            return ret;
        }

        tui->render_active = 1;
    }

    return LDG_ERR_AOK;
}

// load video by id
static void tui_player_load_video(yt_tui_t *tui, const char *video_id)
{
    uint32_t ret = tui_player_ensure(tui);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return; }

    char url[128];
    memset(url, 0, sizeof(url));
    snprintf(url, sizeof(url), "https://www.youtube.com/watch?v=%s", video_id);
    syslog(LOG_INFO, "player_load; url: %s", url);
    yt_player_load(&tui->player, url);
}

// input handlers
static void tui_handle_feed(yt_tui_t *tui, yt_action_t action)
{
    switch (action)
    {
        case YT_ACTION_NONE:
        case YT_ACTION_QUIT:
        case YT_ACTION_LEFT:
        case YT_ACTION_BACK:
        case YT_ACTION_PAUSE:
        case YT_ACTION_SEEK_FWD:
        case YT_ACTION_SEEK_BACK:
        case YT_ACTION_VOL_UP:
        case YT_ACTION_VOL_DOWN:
        case YT_ACTION_NEXT:
        case YT_ACTION_PREV:
        case YT_ACTION_SHUFFLE:
            break;

        case YT_ACTION_RIGHT:
            tui->current_view = YT_TUI_VIEW_QUEUE;
            break;

        case YT_ACTION_UP:
            yt_feed_nav_up(&tui->feed);
            break;

        case YT_ACTION_DOWN:
            yt_feed_nav_down(&tui->feed);
            break;

        case YT_ACTION_SELECT:
        {
            const yt_video_t *vid = yt_feed_selected_get(&tui->feed);
            if (vid)
            {
                yt_queue_push(&tui->queue, vid);
                tui->current_view = YT_TUI_VIEW_PLAYER;
                tui_player_load_video(tui, vid->id);
            }

            break;
        }

        case YT_ACTION_QUEUE_ADD:
        {
            const yt_video_t *vid = yt_feed_selected_get(&tui->feed);
            if (vid) { yt_queue_push(&tui->queue, vid); }

            break;
        }

        case YT_ACTION_REFRESH:
            tui->feed.loading = 1;
            tui->feed_req_pending = 0;
            tui_feed_request(tui);
            break;

        case YT_ACTION_SEARCH:
            memset(tui->search_buff, 0, sizeof(tui->search_buff));
            tui->search_len = 0;
            tui->current_view = YT_TUI_VIEW_SEARCH;
            break;

        default:
            break;
    }
}

static void tui_handle_player(yt_tui_t *tui, yt_action_t action)
{
    switch (action)
    {
        case YT_ACTION_NONE:
        case YT_ACTION_QUIT:
        case YT_ACTION_UP:
        case YT_ACTION_DOWN:
        case YT_ACTION_LEFT:
        case YT_ACTION_RIGHT:
        case YT_ACTION_SELECT:
        case YT_ACTION_SEARCH:
        case YT_ACTION_QUEUE_ADD:
        case YT_ACTION_REFRESH:
            break;

        case YT_ACTION_PAUSE:
        {
            if (!tui->player_ready) { break; }

            int paused = 0;
            mpv_get_property(tui->player.mpv, "pause", MPV_FORMAT_FLAG, &paused);
            if (paused) { yt_player_resume(&tui->player); }
            else { yt_player_pause(&tui->player); }

            break;
        }

        case YT_ACTION_SEEK_FWD:
            if (tui->player_ready) { yt_player_seek(&tui->player, YT_TUI_SEEK_SECS); }

            break;

        case YT_ACTION_SEEK_BACK:
            if (tui->player_ready) { yt_player_seek(&tui->player, -YT_TUI_SEEK_SECS); }

            break;

        case YT_ACTION_VOL_UP:
        {
            if (!tui->player_ready) { break; }

            double vol = 0.0;
            mpv_get_property(tui->player.mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
            vol += YT_TUI_VOL_STEP;
            if (vol > 150.0) { vol = 150.0; }

            yt_player_volume_set(&tui->player, (uint32_t)vol);
            break;
        }

        case YT_ACTION_VOL_DOWN:
        {
            if (!tui->player_ready) { break; }

            double vol = 0.0;
            mpv_get_property(tui->player.mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
            vol -= YT_TUI_VOL_STEP;
            if (vol < 0.0) { vol = 0.0; }

            yt_player_volume_set(&tui->player, (uint32_t)vol);
            break;
        }

        case YT_ACTION_NEXT:
        {
            uint32_t ret = yt_queue_next(&tui->queue);
            if (ret == LDG_ERR_AOK)
            {
                const yt_video_t *vid = yt_queue_current_get(&tui->queue);
                if (vid) { tui_player_load_video(tui, vid->id); }
            }

            break;
        }

        case YT_ACTION_PREV:
        {
            uint32_t ret = yt_queue_prev(&tui->queue);
            if (ret == LDG_ERR_AOK)
            {
                const yt_video_t *vid = yt_queue_current_get(&tui->queue);
                if (vid) { tui_player_load_video(tui, vid->id); }
            }

            break;
        }

        case YT_ACTION_SHUFFLE:
            yt_queue_shuffle(&tui->queue);
            break;

        case YT_ACTION_BACK:
            if (tui->player_ready) { yt_player_stop(&tui->player); }

            if (tui->render_active)
            {
                yt_render_shutdown(&tui->render);
                tui->render_active = 0;
            }

            tui->current_view = YT_TUI_VIEW_FEED;
            break;

        default:
            break;
    }
}

static void tui_handle_queue(yt_tui_t *tui, yt_action_t action)
{
    switch (action)
    {
        case YT_ACTION_NONE:
        case YT_ACTION_QUIT:
        case YT_ACTION_LEFT:
        case YT_ACTION_RIGHT:
        case YT_ACTION_PAUSE:
        case YT_ACTION_SEEK_FWD:
        case YT_ACTION_SEEK_BACK:
        case YT_ACTION_VOL_UP:
        case YT_ACTION_VOL_DOWN:
        case YT_ACTION_QUEUE_ADD:
        case YT_ACTION_REFRESH:
        case YT_ACTION_SEARCH:
            break;

        case YT_ACTION_UP:
            if (tui->queue.selected_idx > 0) { tui->queue.selected_idx--; }

            break;

        case YT_ACTION_DOWN:
            if (tui->queue.cunt > 0 && tui->queue.selected_idx < tui->queue.cunt - 1) { tui->queue.selected_idx++; }

            break;

        case YT_ACTION_SELECT:
        {
            if (tui->queue.selected_idx < tui->queue.cunt)
            {
                tui->queue.current_idx = tui->queue.selected_idx;
                tui->current_view = YT_TUI_VIEW_PLAYER;
                const yt_video_t *vid = yt_queue_current_get(&tui->queue);
                if (vid) { tui_player_load_video(tui, vid->id); }
            }

            break;
        }

        case YT_ACTION_NEXT:
        {
            uint32_t ret = yt_queue_next(&tui->queue);
            if (ret == LDG_ERR_AOK)
            {
                tui->current_view = YT_TUI_VIEW_PLAYER;
                const yt_video_t *vid = yt_queue_current_get(&tui->queue);
                if (vid) { tui_player_load_video(tui, vid->id); }
            }

            break;
        }

        case YT_ACTION_PREV:
        {
            uint32_t ret = yt_queue_prev(&tui->queue);
            if (ret == LDG_ERR_AOK)
            {
                tui->current_view = YT_TUI_VIEW_PLAYER;
                const yt_video_t *vid = yt_queue_current_get(&tui->queue);
                if (vid) { tui_player_load_video(tui, vid->id); }
            }

            break;
        }

        case YT_ACTION_SHUFFLE:
            yt_queue_shuffle(&tui->queue);
            break;

        case YT_ACTION_BACK:
            tui->current_view = YT_TUI_VIEW_FEED;
            break;

        default:
            break;
    }
}

static void tui_handle_search(yt_tui_t *tui, const struct ncinput *ni)
{
    if (ni->id == NCKEY_ESC)
    {
        tui->current_view = YT_TUI_VIEW_FEED;
        return;
    }

    if (ni->id == NCKEY_ENTER || ni->id == '\n')
    {
        if (tui->search_len > 0)
        {
            tui->search_buff[tui->search_len] = '\0';

            tui_search_task_ctx_t *ctx = (tui_search_task_ctx_t *)malloc(sizeof(tui_search_task_ctx_t));
            if (ctx)
            {
                memset(ctx, 0, sizeof(*ctx));
                snprintf(ctx->access_token, sizeof(ctx->access_token), "%s", tui->token.access);
                snprintf(ctx->query, sizeof(ctx->query), "%s", tui->search_buff);
                ctx->result_q = &tui->api_result_q;

                uint32_t ret = ldg_thread_pool_submit(&tui->pool, search_task, ctx);
                if (ret == LDG_ERR_AOK)
                {
                    tui->feed.loading = 1;
                    tui->feed_req_pending = 1;
                }
                else{ free(ctx); }
            }
        }

        tui->current_view = YT_TUI_VIEW_FEED;
        return;
    }

    if (ni->id == NCKEY_BACKSPACE || ni->id == 127)
    {
        if (tui->search_len > 0) { tui->search_len--; }

        return;
    }

    if (ni->id >= 32 && ni->id < 127 && tui->search_len < YT_TUI_SEARCH_MAX - 1)
    {
        tui->search_buff[tui->search_len] = (char)ni->id;
        tui->search_len++;
    }
}

// renderers
static void tui_render_auth(yt_tui_t *tui)
{
    yt_layout_header_render(&tui->layout, "auth");
    ncplane_erase(tui->layout.content);
    ncplane_set_fg_rgb8(tui->layout.content, 200, 200, 200);

    if (tui->auth_err != LDG_ERR_AOK)
    {
        ncplane_set_fg_rgb8(tui->layout.content, 255, 80, 80);
        ncplane_putstr_yx(tui->layout.content, 2, 2, "auth failed:");

        if (tui->conf->client_id[0] == '\0')
        {
            ncplane_putstr_yx(tui->layout.content, 2, 16, "no client_id configured");
            ncplane_set_fg_rgb8(tui->layout.content, 150, 150, 150);
            ncplane_putstr_yx(tui->layout.content, 4, 2, "add client_id=<id> to:");
            ncplane_set_fg_rgb8(tui->layout.content, 100, 200, 255);
            ncplane_printf_yx(tui->layout.content, 5, 4, "%s/config", tui->conf->conf_dir);
        }
        else
        {
            const char *err_str = yt_err_str_get(tui->auth_err);
            ncplane_putstr_yx(tui->layout.content, 2, 16, err_str);
        }

        ncplane_set_fg_rgb8(tui->layout.content, 150, 150, 150);
        ncplane_putstr_yx(tui->layout.content, 7, 2, "press r to retry");
    }
    else if (tui->auth_started && tui->device_resp.user_code[0] != '\0')
    {
        ncplane_putstr_yx(tui->layout.content, 2, 2, "go to:");
        ncplane_set_fg_rgb8(tui->layout.content, 100, 200, 255);
        ncplane_putstr_yx(tui->layout.content, 2, 10, tui->device_resp.verify_url);
        ncplane_set_fg_rgb8(tui->layout.content, 200, 200, 200);
        ncplane_putstr_yx(tui->layout.content, 4, 2, "enter code:");
        ncplane_set_fg_rgb8(tui->layout.content, 255, 255, 100);
        ncplane_putstr_yx(tui->layout.content, 4, 15, tui->device_resp.user_code);
    }
    else{ ncplane_putstr_yx(tui->layout.content, 2, 2, "requesting device code..."); }

    yt_layout_status_render(&tui->layout, "waiting for authorization | r:retry q:quit");
}

static void tui_render_feed(yt_tui_t *tui)
{
    yt_layout_header_render(&tui->layout, "feed");
    yt_feed_render(&tui->feed, tui->layout.content);

    char status_msg[128];
    memset(status_msg, 0, sizeof(status_msg));
    snprintf(status_msg, sizeof(status_msg), "videos: %u | q:quit j/k:nav enter:play a:queue l:queue r:refresh /:search", tui->feed.feed.video_cunt);
    yt_layout_status_render(&tui->layout, status_msg);
}

static void tui_render_player(yt_tui_t *tui)
{
    yt_layout_header_render(&tui->layout, "player");

    unsigned content_rows = 0;
    unsigned content_cols = 0;
    ncplane_dim_yx(tui->layout.content, &content_rows, &content_cols);

    uint32_t video_rows = (content_rows > YT_TUI_VIDEO_INFO_ROWS) ? content_rows - YT_TUI_VIDEO_INFO_ROWS : content_rows;
    uint32_t info_y = video_rows;

    uint32_t r = info_y;
    for (; r < content_rows; r++) { ncplane_putstr_yx(tui->layout.content, (int)r, 0, ""); }

    const yt_video_t *current = yt_queue_current_get(&tui->queue);
    if (current)
    {
        ncplane_set_fg_rgb8(tui->layout.content, 220, 220, 220);
        ncplane_putstr_yx(tui->layout.content, (int)info_y, 2, current->title);

        ncplane_set_fg_rgb8(tui->layout.content, 0, 180, 180);
        ncplane_printf_yx(tui->layout.content, (int)(info_y + 1), 2, "%s", current->channel);

        if (current->view_cunt > 0)
        {
            ncplane_set_fg_rgb8(tui->layout.content, 150, 150, 150);
            ncplane_printf_yx(tui->layout.content, (int)(info_y + 1), (int)(2 + strlen(current->channel) + 1), "| %lu views", (unsigned long)current->view_cunt);
        }

        if (tui->player_ready)
        {
            double pos = 0.0;
            double dur = 0.0;
            double vol = 0.0;
            int paused = 0;
            mpv_get_property(tui->player.mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos);
            mpv_get_property(tui->player.mpv, "duration", MPV_FORMAT_DOUBLE, &dur);
            mpv_get_property(tui->player.mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
            mpv_get_property(tui->player.mpv, "pause", MPV_FORMAT_FLAG, &paused);

            // progress bar
            uint32_t bar_w = (content_cols > 40) ? content_cols - 40 : 10;
            uint32_t filled = 0;
            if (dur > 0.0) { filled = (uint32_t)((pos / dur) * (double)bar_w); }

            if (filled > bar_w) { filled = bar_w; }

            char bar[256];
            memset(bar, 0, sizeof(bar));
            uint32_t bi = 0;
            for (; bi < bar_w && bi < sizeof(bar) - 1; bi++) { bar[bi] = (bi < filled) ? '=' : '-'; }
            bar[bi] = '\0';

            ncplane_set_fg_rgb8(tui->layout.content, 180, 180, 0);
            ncplane_printf_yx(tui->layout.content, (int)(info_y + 2), 2, "%s  %02u:%02u / %02u:%02u  [%s]  vol: %u%%", paused ? "paused " : "playing", (uint32_t)pos / 60, (uint32_t)pos % 60, (uint32_t)dur / 60, (uint32_t)dur % 60, bar, (uint32_t)vol);
        }
        else
        {
            ncplane_set_fg_rgb8(tui->layout.content, 150, 150, 150);
            ncplane_putstr_yx(tui->layout.content, (int)(info_y + 2), 2, "loading stream...");
        }
    }
    else
    {
        ncplane_set_fg_rgb8(tui->layout.content, 150, 150, 150);
        ncplane_putstr_yx(tui->layout.content, (int)info_y, 2, "no video selected");
    }

    yt_layout_status_render(&tui->layout, "space:pause </>:seek +/-:vol n/p:next/prev esc:back");

    if (tui->render_active)
    {
        unsigned status_rows = 0;
        unsigned status_cols = 0;
        ncplane_dim_yx(tui->layout.status, &status_rows, &status_cols);
        ncplane_set_fg_rgb8(tui->layout.status, 100, 100, 100);
        ncplane_printf_yx(tui->layout.status, 0, (int)(status_cols - 22), "%ux%u %u fps", tui->render.pixel_w, tui->render.pixel_h, tui->render.fps);
    }
}

static void tui_render_search(yt_tui_t *tui)
{
    yt_layout_header_render(&tui->layout, "search");
    ncplane_erase(tui->layout.content);
    ncplane_set_fg_rgb8(tui->layout.content, 200, 200, 200);
    ncplane_putstr_yx(tui->layout.content, 2, 2, "search: ");

    tui->search_buff[tui->search_len] = '\0';
    ncplane_set_fg_rgb8(tui->layout.content, 255, 255, 255);
    ncplane_putstr_yx(tui->layout.content, 2, 10, tui->search_buff);

    ncplane_set_fg_rgb8(tui->layout.content, 100, 100, 100);
    ncplane_putchar_yx(tui->layout.content, 2, (int)(10 + tui->search_len), '_');

    yt_layout_status_render(&tui->layout, "type query, enter:search esc:cancel");
}

static void tui_render_queue(yt_tui_t *tui)
{
    yt_layout_header_render(&tui->layout, "queue");
    yt_queue_render(&tui->queue, tui->layout.content);

    char status_msg[128];
    memset(status_msg, 0, sizeof(status_msg));
    snprintf(status_msg, sizeof(status_msg), "tracks: %u | j/k:nav enter:play s:shuffle esc:back", tui->queue.cunt);
    yt_layout_status_render(&tui->layout, status_msg);
}

// init
uint32_t yt_tui_init(yt_tui_t *tui, yt_conf_t *conf)
{
    if (LDG_UNLIKELY(!tui)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!conf)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(tui, 0, sizeof(*tui));
    tui->conf = conf;

    tui_term_reset();
    signal(SIGINT, tui_signal_cleanup);
    signal(SIGTERM, tui_signal_cleanup);

    notcurses_options nc_opts;
    memset(&nc_opts, 0, sizeof(nc_opts));
    nc_opts.flags = NCOPTION_SUPPRESS_BANNERS;

    tui->nc = notcurses_init(&nc_opts, NULL);
    if (LDG_UNLIKELY(!tui->nc)) { return YT_ERR_TUI_INIT; }

    uint32_t err = yt_layout_init(&tui->layout, tui->nc);
    if (LDG_UNLIKELY(err != LDG_ERR_AOK))
    {
        notcurses_stop(tui->nc);
        tui->nc = NULL;
        return err;
    }

    err = ldg_thread_pool_init(&tui->pool, conf->pool_workers);
    if (LDG_UNLIKELY(err != LDG_ERR_AOK))
    {
        yt_layout_shutdown(&tui->layout);
        notcurses_stop(tui->nc);
        tui->nc = NULL;
        return err;
    }

    err = ldg_spsc_init(&tui->api_result_q, sizeof(yt_feed_t), 4);
    if (LDG_UNLIKELY(err != LDG_ERR_AOK))
    {
        ldg_thread_pool_shutdown(&tui->pool);
        yt_layout_shutdown(&tui->layout);
        notcurses_stop(tui->nc);
        tui->nc = NULL;
        return err;
    }

    err = ldg_spsc_init(&tui->auth_result_q, sizeof(yt_token_t), 2);
    if (LDG_UNLIKELY(err != LDG_ERR_AOK))
    {
        ldg_spsc_shutdown(&tui->api_result_q);
        ldg_thread_pool_shutdown(&tui->pool);
        yt_layout_shutdown(&tui->layout);
        notcurses_stop(tui->nc);
        tui->nc = NULL;
        return err;
    }

    err = ldg_spsc_init(&tui->stream_result_q, sizeof(yt_stream_set_t), 4);
    if (LDG_UNLIKELY(err != LDG_ERR_AOK))
    {
        ldg_spsc_shutdown(&tui->auth_result_q);
        ldg_spsc_shutdown(&tui->api_result_q);
        ldg_thread_pool_shutdown(&tui->pool);
        yt_layout_shutdown(&tui->layout);
        notcurses_stop(tui->nc);
        tui->nc = NULL;
        return err;
    }

    err = ldg_spsc_init(&tui->thumb_result_q, sizeof(tui_thumb_result_t), 32);
    if (LDG_UNLIKELY(err != LDG_ERR_AOK))
    {
        ldg_spsc_shutdown(&tui->stream_result_q);
        ldg_spsc_shutdown(&tui->auth_result_q);
        ldg_spsc_shutdown(&tui->api_result_q);
        ldg_thread_pool_shutdown(&tui->pool);
        yt_layout_shutdown(&tui->layout);
        notcurses_stop(tui->nc);
        tui->nc = NULL;
        return err;
    }

    err = yt_feed_ctx_init(&tui->feed, conf->thumb_cache_max);
    if (LDG_UNLIKELY(err != LDG_ERR_AOK))
    {
        ldg_spsc_shutdown(&tui->thumb_result_q);
        ldg_spsc_shutdown(&tui->stream_result_q);
        ldg_spsc_shutdown(&tui->auth_result_q);
        ldg_spsc_shutdown(&tui->api_result_q);
        ldg_thread_pool_shutdown(&tui->pool);
        yt_layout_shutdown(&tui->layout);
        notcurses_stop(tui->nc);
        tui->nc = NULL;
        return err;
    }

    yt_queue_init(&tui->queue);

    err = yt_token_load(&tui->token, conf->data_dir);
    if (err != LDG_ERR_AOK)
    {
        tui->current_view = YT_TUI_VIEW_AUTH;
        return LDG_ERR_AOK;
    }

    if (yt_token_expired_is(&tui->token))
    {
        ldg_curl_easy_ctx_t curl;
        err = ldg_curl_easy_ctx_create(&curl);
        if (err == LDG_ERR_AOK)
        {
            err = yt_token_refresh(&curl, conf->client_id, conf->client_secret, &tui->token);
            ldg_curl_easy_ctx_destroy(&curl);

            if (err == LDG_ERR_AOK) { yt_token_store(&tui->token, conf->data_dir); }
            else
            {
                tui->current_view = YT_TUI_VIEW_AUTH;
                return LDG_ERR_AOK;
            }
        }
        else
        {
            tui->current_view = YT_TUI_VIEW_AUTH;
            return LDG_ERR_AOK;
        }
    }

    err = yt_innertube_ctx_init(&tui->api, tui->token.access);
    if (LDG_UNLIKELY(err != LDG_ERR_AOK))
    {
        tui->current_view = YT_TUI_VIEW_AUTH;
        return LDG_ERR_AOK;
    }

    tui->current_view = YT_TUI_VIEW_FEED;
    tui->feed.loading = 1;

    return LDG_ERR_AOK;
}

// main loop
uint32_t yt_tui_run(yt_tui_t *tui)
{
    if (LDG_UNLIKELY(!tui)) { return LDG_ERR_FUNC_ARG_NULL; }

    tui->running = 1;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = YT_TUI_POLL_NS;
    uint64_t player_render_ns = 0;

    while (tui->running)
    {
        // auth
        if (tui->current_view == YT_TUI_VIEW_AUTH && !tui->auth_started) { tui_auth_start(tui); }

        // feed
        if (tui->current_view == YT_TUI_VIEW_FEED && tui->feed.loading && !tui->feed_req_pending) { tui_feed_request(tui); }

        // input
        struct ncinput ni;
        memset(&ni, 0, sizeof(ni));
        uint32_t got = notcurses_get(tui->nc, &ts, &ni);

        if (got != 0 && ni.id == NCKEY_RESIZE)
        {
            if (tui->render_active)
            {
                yt_render_shutdown(&tui->render);
                tui->render_active = 0;
            }

            yt_feed_thumb_planes_destroy(&tui->feed);
            yt_layout_resize(&tui->layout, tui->nc);

            if (tui->current_view == YT_TUI_VIEW_PLAYER && tui->player_ready)
            {
                unsigned content_rows = 0;
                unsigned content_cols = 0;
                ncplane_dim_yx(tui->layout.content, &content_rows, &content_cols);

                uint32_t video_rows = (content_rows > YT_TUI_VIDEO_INFO_ROWS) ? content_rows - YT_TUI_VIDEO_INFO_ROWS : content_rows;
                uint32_t video_cols = content_cols * 95 / 100;

                yt_render_init(&tui->render, tui->player.mpv, tui->layout.content, video_rows, video_cols);
                tui->render_active = 1;
            }

            notcurses_render(tui->nc);
            continue;
        }

        if (got != 0)
        {
            if (tui->current_view == YT_TUI_VIEW_SEARCH)
            {
                if (ni.id == 'q' && tui->search_len == 0)
                {
                    tui->running = 0;
                    break;
                }

                tui_handle_search(tui, &ni);
            }
            else
            {
                yt_action_t action = yt_input_dispatch(&ni);

                if (action == YT_ACTION_QUIT)
                {
                    tui->running = 0;
                    break;
                }

                switch (tui->current_view)
                {
                    case YT_TUI_VIEW_AUTH:
                        if (action == YT_ACTION_REFRESH)
                        {
                            tui->auth_started = 0;
                            tui->auth_err = LDG_ERR_AOK;
                        }

                        break;

                    case YT_TUI_VIEW_FEED:
                        tui_handle_feed(tui, action);
                        break;

                    case YT_TUI_VIEW_PLAYER:
                        tui_handle_player(tui, action);
                        break;

                    case YT_TUI_VIEW_QUEUE:
                        tui_handle_queue(tui, action);
                        break;

                    case YT_TUI_VIEW_SEARCH:
                        break;

                    default:
                        break;
                }
            }
        }

        // poll auth result
        {
            yt_token_t auth_token;
            memset(&auth_token, 0, sizeof(auth_token));
            if (ldg_spsc_pop(&tui->auth_result_q, &auth_token) == LDG_ERR_AOK)
            {
                memcpy(&tui->token, &auth_token, sizeof(auth_token));
                yt_innertube_ctx_shutdown(&tui->api);
                yt_innertube_ctx_init(&tui->api, tui->token.access);
                tui->current_view = YT_TUI_VIEW_FEED;
                tui->feed.loading = 1;
                tui->feed_req_pending = 0;
                tui->auth_started = 0;
            }
        }

        // poll feed result
        {
            yt_feed_t feed_result;
            memset(&feed_result, 0, sizeof(feed_result));
            if (ldg_spsc_pop(&tui->api_result_q, &feed_result) == LDG_ERR_AOK)
            {
                memcpy(&tui->feed.feed, &feed_result, sizeof(feed_result));
                tui->feed.loading = 0;
                tui->feed.selected_idx = 0;
                tui->feed.scroll_offset = 0;
                tui->feed_req_pending = 0;
                tui_thumb_request_all(tui);
            }
        }

        // poll thumb results
        {
            tui_thumb_result_t thumb_result;
            memset(&thumb_result, 0, sizeof(thumb_result));
            while (ldg_spsc_pop(&tui->thumb_result_q, &thumb_result) == LDG_ERR_AOK) { yt_thumb_cache_put(&tui->feed.thumbs, thumb_result.video_id, thumb_result.vis); }
        }

        // poll player events
        if (tui->player_ready)
        {
            yt_player_event_t pe;
            memset(&pe, 0, sizeof(pe));
            while (ldg_spsc_pop(&tui->player.event_q, &pe) == LDG_ERR_AOK)
            {
                if (pe.eof_reached)
                {
                    uint32_t next_ret = yt_queue_next(&tui->queue);
                    if (next_ret == LDG_ERR_AOK)
                    {
                        const yt_video_t *vid = yt_queue_current_get(&tui->queue);
                        if (vid) { tui_player_load_video(tui, vid->id); }
                    }
                    else
                    {
                        if (tui->render_active)
                        {
                            yt_render_shutdown(&tui->render);
                            tui->render_active = 0;
                        }

                        tui->current_view = YT_TUI_VIEW_FEED;
                    }
                }
            }
        }

        // player refresh
        if (tui->current_view == YT_TUI_VIEW_PLAYER)
        {
            struct timespec nc_ts;
            memset(&nc_ts, 0, sizeof(nc_ts));
            clock_gettime(CLOCK_MONOTONIC, &nc_ts);
            uint64_t nc_now = (uint64_t)nc_ts.tv_sec * LDG_NS_PER_SEC + (uint64_t)nc_ts.tv_nsec;

            if (got != 0 || nc_now - player_render_ns >= LDG_NS_PER_SEC / 2)
            {
                tui_render_player(tui);

                if (tui->render_active) { yt_render_stdout_lock(&tui->render); }

                notcurses_render(tui->nc);
                if (tui->render_active) { yt_render_stdout_unlock(&tui->render); }

                player_render_ns = nc_now;
            }
        }
        else if (tui->current_view != YT_TUI_VIEW_PLAYER)
        {
            switch (tui->current_view)
            {
                case YT_TUI_VIEW_AUTH:
                    tui_render_auth(tui);
                    break;

                case YT_TUI_VIEW_FEED:
                    tui_render_feed(tui);
                    break;

                case YT_TUI_VIEW_PLAYER:
                    break;

                case YT_TUI_VIEW_SEARCH:
                    tui_render_search(tui);
                    break;

                case YT_TUI_VIEW_QUEUE:
                    tui_render_queue(tui);
                    break;

                default:
                    break;
            }

            notcurses_render(tui->nc);
        }
    }

    return LDG_ERR_AOK;
}

// shutdown
void yt_tui_shutdown(yt_tui_t *tui)
{
    if (LDG_UNLIKELY(!tui)) { return; }

    if (tui->render_active)
    {
        yt_render_shutdown(&tui->render);
        tui->render_active = 0;
    }

    if (tui->player_ready) { yt_player_shutdown(&tui->player); }

    yt_feed_ctx_shutdown(&tui->feed);
    ldg_thread_pool_shutdown(&tui->pool);
    ldg_spsc_shutdown(&tui->thumb_result_q);
    ldg_spsc_shutdown(&tui->stream_result_q);
    ldg_spsc_shutdown(&tui->auth_result_q);
    ldg_spsc_shutdown(&tui->api_result_q);
    yt_innertube_ctx_shutdown(&tui->api);
    yt_layout_shutdown(&tui->layout);

    if (tui->nc)
    {
        notcurses_stop(tui->nc);
        tui->nc = NULL;
    }

    tui_term_reset();
}
