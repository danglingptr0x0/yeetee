#ifndef YT_TUI_LAYOUT_H
#define YT_TUI_LAYOUT_H

#include <stdint.h>
#include <notcurses/notcurses.h>

#define YT_LAYOUT_HEADER_ROWS 3
#define YT_LAYOUT_STATUS_ROWS 2

typedef struct yt_layout
{
    struct ncplane *header;
    struct ncplane *content;
    struct ncplane *status;
    struct ncplane *overlay;
    uint32_t term_rows;
    uint32_t term_cols;
} yt_layout_t;

uint32_t yt_layout_init(yt_layout_t *layout, struct notcurses *nc);
void yt_layout_shutdown(yt_layout_t *layout);
uint32_t yt_layout_resize(yt_layout_t *layout, struct notcurses *nc);
uint32_t yt_layout_header_render(yt_layout_t *layout, const char *title);
uint32_t yt_layout_status_render(yt_layout_t *layout, const char *msg);

#endif
