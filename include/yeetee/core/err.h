#ifndef YT_CORE_ERR_H
#define YT_CORE_ERR_H

#include <dangling/core/err.h>

// auth (700-719)
#define YT_ERR_AUTH_DEVICE_CODE   700
#define YT_ERR_AUTH_POLL          701
#define YT_ERR_AUTH_TOKEN_EXPIRED 702
#define YT_ERR_AUTH_TOKEN_INVALID 703
#define YT_ERR_AUTH_REFRESH       704
#define YT_ERR_AUTH_STORE         705
#define YT_ERR_AUTH_LOAD          706

// api (720-759)
#define YT_ERR_API_REQ            720
#define YT_ERR_API_PARSE          721
#define YT_ERR_API_RATE_LIMIT     722
#define YT_ERR_API_FORBIDDEN      723
#define YT_ERR_API_NOT_FOUND      724
#define YT_ERR_API_INNERTUBE      725
#define YT_ERR_API_YTDLP          726
#define YT_ERR_API_YTDLP_SPAWN   727

// player (760-779)
#define YT_ERR_PLAYER_INIT        760
#define YT_ERR_PLAYER_LOAD        761
#define YT_ERR_PLAYER_RENDER_INIT 762
#define YT_ERR_PLAYER_RENDER      763
#define YT_ERR_PLAYER_NO_STREAM   764

// tui (780-799)
#define YT_ERR_TUI_INIT           780
#define YT_ERR_TUI_RENDER         781
#define YT_ERR_TUI_LAYOUT         782

const char* yt_err_str_get(uint32_t code);

#endif
