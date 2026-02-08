#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <notcurses/notcurses.h>
#include <yeetee/tui/queue.h>

#define YT_QUEUE_FG_TITLE_R  220
#define YT_QUEUE_FG_TITLE_G  220
#define YT_QUEUE_FG_TITLE_B  220

#define YT_QUEUE_FG_CHAN_R   0
#define YT_QUEUE_FG_CHAN_G   180
#define YT_QUEUE_FG_CHAN_B   180

#define YT_QUEUE_FG_DUR_R    180
#define YT_QUEUE_FG_DUR_G    180
#define YT_QUEUE_FG_DUR_B    0

#define YT_QUEUE_BG_SEL_R    30
#define YT_QUEUE_BG_SEL_G    30
#define YT_QUEUE_BG_SEL_B    80

#define YT_QUEUE_BG_CUR_R    20
#define YT_QUEUE_BG_CUR_G    60
#define YT_QUEUE_BG_CUR_B    20

uint32_t yt_queue_init(yt_queue_t *q)
{
    if (LDG_UNLIKELY(!q)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(q, 0, sizeof(*q));
    q->current_idx = UINT32_MAX;

    return LDG_ERR_AOK;
}

uint32_t yt_queue_push(yt_queue_t *q, const yt_video_t *video)
{
    if (LDG_UNLIKELY(!q)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!video)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(q->cunt >= YT_QUEUE_MAX)) { return LDG_ERR_FULL; }

    memcpy(&q->items[q->cunt], video, sizeof(yt_video_t));
    q->cunt++;

    if (q->current_idx == UINT32_MAX) { q->current_idx = 0; }

    return LDG_ERR_AOK;
}

uint32_t yt_queue_clear(yt_queue_t *q)
{
    if (LDG_UNLIKELY(!q)) { return LDG_ERR_FUNC_ARG_NULL; }

    q->cunt = 0;
    q->current_idx = UINT32_MAX;
    q->selected_idx = 0;
    q->scroll_offset = 0;

    return LDG_ERR_AOK;
}

const yt_video_t* yt_queue_current_get(const yt_queue_t *q)
{
    if (LDG_UNLIKELY(!q)) { return NULL; }

    if (q->current_idx >= q->cunt) { return NULL; }

    return &q->items[q->current_idx];
}

uint32_t yt_queue_next(yt_queue_t *q)
{
    if (LDG_UNLIKELY(!q)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(q->cunt == 0)) { return LDG_ERR_EMPTY; }

    if (q->current_idx + 1 >= q->cunt) { return LDG_ERR_EOF; }

    q->current_idx++;

    return LDG_ERR_AOK;
}

uint32_t yt_queue_prev(yt_queue_t *q)
{
    if (LDG_UNLIKELY(!q)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(q->cunt == 0)) { return LDG_ERR_EMPTY; }

    q->current_idx = (q->current_idx == 0) ? q->cunt - 1 : q->current_idx - 1;

    return LDG_ERR_AOK;
}

uint32_t yt_queue_shuffle(yt_queue_t *q)
{
    if (LDG_UNLIKELY(!q)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(q->cunt < 2)) { return LDG_ERR_AOK; }

    static uint8_t seeded = 0;
    if (!seeded)
    {
        srand((uint32_t)time(NULL));
        seeded = 1;
    }

    yt_video_t tmp;
    uint32_t i = q->cunt - 1;
    for (; i > 0; i--)
    {
        uint32_t j = (uint32_t)rand() % (i + 1);
        if (i == j) { continue; }

        tmp = q->items[i];
        q->items[i] = q->items[j];
        q->items[j] = tmp;
    }

    return LDG_ERR_AOK;
}

uint32_t yt_queue_render(yt_queue_t *q, struct ncplane *plane)
{
    if (LDG_UNLIKELY(!q)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!plane)) { return LDG_ERR_FUNC_ARG_NULL; }

    ncplane_erase(plane);

    unsigned p_rows = 0;
    unsigned p_cols = 0;
    ncplane_dim_yx(plane, &p_rows, &p_cols);
    uint32_t rows = (uint32_t)p_rows;
    uint32_t cols = (uint32_t)p_cols;
    (void)cols;

    uint32_t visible_cunt = rows;
    uint32_t end = q->scroll_offset + visible_cunt;
    if (end > q->cunt) { end = q->cunt; }

    uint32_t i = q->scroll_offset;
    for (; i < end; i++)
    {
        uint32_t row = i - q->scroll_offset;
        const yt_video_t *v = &q->items[i];

        if (i == q->selected_idx) { ncplane_set_bg_rgb8(plane, YT_QUEUE_BG_SEL_R, YT_QUEUE_BG_SEL_G, YT_QUEUE_BG_SEL_B); }
        else if (i == q->current_idx) { ncplane_set_bg_rgb8(plane, YT_QUEUE_BG_CUR_R, YT_QUEUE_BG_CUR_G, YT_QUEUE_BG_CUR_B); }
        else{ ncplane_set_bg_default(plane); }

        const char *prefix = (i == q->current_idx) ? "> " : "  ";

        ncplane_set_fg_rgb8(plane, YT_QUEUE_FG_TITLE_R, YT_QUEUE_FG_TITLE_G, YT_QUEUE_FG_TITLE_B);
        ncplane_printf_yx(plane, (int)row, 0, "%s[%u] ", prefix, i + 1);

        int col = 2 + 3;
        if (i + 1 >= 10) { col++; }

        if (i + 1 >= 100) { col++; }

        ncplane_set_fg_rgb8(plane, YT_QUEUE_FG_TITLE_R, YT_QUEUE_FG_TITLE_G, YT_QUEUE_FG_TITLE_B);
        col += ncplane_putstr_yx(plane, (int)row, col, v->title);

        ncplane_set_fg_rgb8(plane, YT_QUEUE_FG_CHAN_R, YT_QUEUE_FG_CHAN_G, YT_QUEUE_FG_CHAN_B);
        col += ncplane_putstr_yx(plane, (int)row, col, " - ");
        col += ncplane_putstr_yx(plane, (int)row, col, v->channel);

        ncplane_set_fg_rgb8(plane, YT_QUEUE_FG_DUR_R, YT_QUEUE_FG_DUR_G, YT_QUEUE_FG_DUR_B);
        ncplane_printf_yx(plane, (int)row, col, " (%s)", v->duration_str);

        ncplane_set_bg_default(plane);
    }

    return LDG_ERR_AOK;
}
