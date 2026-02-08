#include <stdint.h>
#include <notcurses/notcurses.h>
#include <notcurses/nckeys.h>
#include <yeetee/tui/input.h>

yt_action_t yt_input_dispatch(const struct ncinput *ni)
{
    if (!ni || ni->id == 0) { return YT_ACTION_NONE; }

    switch (ni->id)
    {
        case 'q':
            return YT_ACTION_QUIT;

        case 'k':
        case NCKEY_UP:
            return YT_ACTION_UP;

        case 'j':
        case NCKEY_DOWN:
            return YT_ACTION_DOWN;

        case 'h':
        case NCKEY_LEFT:
            return YT_ACTION_LEFT;

        case 'l':
        case NCKEY_RIGHT:
            return YT_ACTION_RIGHT;

        case NCKEY_ENTER:
        case '\n':
            return YT_ACTION_SELECT;

        case NCKEY_ESC:
            return YT_ACTION_BACK;

        case '/':
            return YT_ACTION_SEARCH;

        case ' ':
            return YT_ACTION_PAUSE;

        case '>':
            return YT_ACTION_SEEK_FWD;

        case '<':
            return YT_ACTION_SEEK_BACK;

        case '+':
        case '=':
            return YT_ACTION_VOL_UP;

        case '-':
            return YT_ACTION_VOL_DOWN;

        case 'n':
            return YT_ACTION_NEXT;

        case 'p':
            return YT_ACTION_PREV;

        case 'a':
            return YT_ACTION_QUEUE_ADD;

        case 's':
            return YT_ACTION_SHUFFLE;

        case 'r':
            return YT_ACTION_REFRESH;

        default:
            return YT_ACTION_NONE;
    }
}
