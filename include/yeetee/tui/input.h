#ifndef YT_TUI_INPUT_H
#define YT_TUI_INPUT_H

#include <stdint.h>
#include <notcurses/notcurses.h>

typedef enum yt_action
{
    YT_ACTION_NONE = 0,
    YT_ACTION_QUIT,
    YT_ACTION_UP,
    YT_ACTION_DOWN,
    YT_ACTION_LEFT,
    YT_ACTION_RIGHT,
    YT_ACTION_SELECT,
    YT_ACTION_BACK,
    YT_ACTION_SEARCH,
    YT_ACTION_PAUSE,
    YT_ACTION_SEEK_FWD,
    YT_ACTION_SEEK_BACK,
    YT_ACTION_VOL_UP,
    YT_ACTION_VOL_DOWN,
    YT_ACTION_NEXT,
    YT_ACTION_PREV,
    YT_ACTION_QUEUE_ADD,
    YT_ACTION_SHUFFLE,
    YT_ACTION_REFRESH
} yt_action_t;

yt_action_t yt_input_dispatch(const struct ncinput *ni);

#endif
