#ifndef YT_TUI_TUI_H
#define YT_TUI_TUI_H

#include <stdint.h>
#include <notcurses/notcurses.h>
#include <dangling/thread/pool.h>
#include <dangling/thread/spsc.h>
#include <yeetee/yeetee.h>
#include <yeetee/core/conf.h>
#include <yeetee/auth/oauth.h>
#include <yeetee/auth/token.h>
#include <yeetee/api/innertube.h>
#include <yeetee/player/player.h>
#include <yeetee/player/render.h>
#include <yeetee/tui/layout.h>
#include <yeetee/tui/feed.h>
#include <yeetee/tui/queue.h>

#define YT_TUI_SEARCH_MAX 256

typedef enum yt_tui_view
{
    YT_TUI_VIEW_AUTH = 0,
    YT_TUI_VIEW_FEED,
    YT_TUI_VIEW_PLAYER,
    YT_TUI_VIEW_SEARCH,
    YT_TUI_VIEW_QUEUE
} yt_tui_view_t;

typedef struct yt_tui
{
    struct notcurses *nc;
    yt_layout_t layout;
    yt_conf_t *conf;
    yt_token_t token;
    yt_oauth_device_resp_t device_resp;
    yt_innertube_ctx_t api;
    yt_player_t player;
    yt_render_t render;
    yt_feed_ctx_t feed;
    yt_queue_t queue;
    ldg_thread_pool_t pool;
    ldg_spsc_queue_t api_result_q;
    ldg_spsc_queue_t auth_result_q;
    ldg_spsc_queue_t stream_result_q;
    ldg_spsc_queue_t thumb_result_q;
    char search_buff[YT_TUI_SEARCH_MAX];
    uint32_t search_len;
    yt_tui_view_t current_view;
    uint32_t auth_err;
    uint32_t thumb_req_cunt;
    volatile uint8_t running;
    uint8_t auth_started;
    uint8_t feed_req_pending;
    uint8_t player_ready;
    uint8_t stream_req_pending;
    uint8_t render_active;
    uint8_t pudding[2];
} yt_tui_t;

uint32_t yt_tui_init(yt_tui_t *tui, yt_conf_t *conf);
void yt_tui_shutdown(yt_tui_t *tui);
uint32_t yt_tui_run(yt_tui_t *tui);

#endif
