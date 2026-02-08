#include <stdint.h>
#include <stddef.h>

#include <dangling/core/err.h>
#include <yeetee/core/err.h>

// err strings
#define YT_ERR_BASE 700
#define YT_ERR_MAX 799
#define YT_ERR_CUNT (YT_ERR_MAX - YT_ERR_BASE + 1)

static const char *yt_err_strs[YT_ERR_CUNT] = {
    [YT_ERR_AUTH_DEVICE_CODE - YT_ERR_BASE] = "auth device code request failed", [YT_ERR_AUTH_POLL - YT_ERR_BASE] = "auth poll failed", [YT_ERR_AUTH_TOKEN_EXPIRED - YT_ERR_BASE] = "auth token expired", [YT_ERR_AUTH_TOKEN_INVALID - YT_ERR_BASE] = "auth token invalid", [YT_ERR_AUTH_REFRESH - YT_ERR_BASE] = "auth token refresh failed", [YT_ERR_AUTH_STORE - YT_ERR_BASE] = "auth token store failed", [YT_ERR_AUTH_LOAD - YT_ERR_BASE] = "auth token load failed", [YT_ERR_API_REQ - YT_ERR_BASE] = "api request failed", [YT_ERR_API_PARSE - YT_ERR_BASE] = "api response parse failed", [YT_ERR_API_RATE_LIMIT - YT_ERR_BASE] = "api rate limited", [YT_ERR_API_FORBIDDEN - YT_ERR_BASE] = "api forbidden", [YT_ERR_API_NOT_FOUND - YT_ERR_BASE] = "api resource not found", [YT_ERR_API_INNERTUBE - YT_ERR_BASE] = "innertube api failed", [YT_ERR_API_YTDLP - YT_ERR_BASE] = "yt-dlp failed", [YT_ERR_API_YTDLP_SPAWN - YT_ERR_BASE] = "yt-dlp spawn failed", [YT_ERR_PLAYER_INIT - YT_ERR_BASE] = "player init failed", [YT_ERR_PLAYER_LOAD - YT_ERR_BASE] = "player load failed", [YT_ERR_PLAYER_RENDER_INIT - YT_ERR_BASE] = "player render init failed", [YT_ERR_PLAYER_RENDER - YT_ERR_BASE] = "player render failed", [YT_ERR_PLAYER_NO_STREAM - YT_ERR_BASE] = "no playable stream found", [YT_ERR_TUI_INIT - YT_ERR_BASE] = "tui init failed", [YT_ERR_TUI_RENDER - YT_ERR_BASE] = "tui render failed", [YT_ERR_TUI_LAYOUT - YT_ERR_BASE] = "tui layout failed", };

const char* yt_err_str_get(uint32_t code)
{
    if (code == LDG_ERR_AOK) { return "ok"; }

    if (code < YT_ERR_BASE || code > YT_ERR_MAX) { return "unknown error"; }

    uint32_t idx = code - YT_ERR_BASE;
    const char *str = yt_err_strs[idx];

    if (!str) { return "unknown error"; }

    return str;
}
