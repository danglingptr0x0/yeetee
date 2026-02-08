#include <string.h>
#include <stdio.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <notcurses/notcurses.h>
#include <yeetee/tui/feed.h>

#define YT_FEED_ROWS_PER_ENTRY 5
#define YT_FEED_TITLE_PAD 4

#define YT_FEED_THUMB_COLS 20
#define YT_FEED_THUMB_ROWS 4
#define YT_FEED_TEXT_COL 23

#define YT_FEED_BG_SEL_R 30
#define YT_FEED_BG_SEL_G 30
#define YT_FEED_BG_SEL_B 80

#define YT_FEED_FG_TITLE_R 220
#define YT_FEED_FG_TITLE_G 220
#define YT_FEED_FG_TITLE_B 220

#define YT_FEED_FG_CHAN_R 0
#define YT_FEED_FG_CHAN_G 180
#define YT_FEED_FG_CHAN_B 180

#define YT_FEED_FG_DUR_R 180
#define YT_FEED_FG_DUR_G 180
#define YT_FEED_FG_DUR_B 0

#define YT_FEED_FG_VIEWS_R 140
#define YT_FEED_FG_VIEWS_G 140
#define YT_FEED_FG_VIEWS_B 140

#define YT_FEED_FG_SEP_R 60
#define YT_FEED_FG_SEP_G 60
#define YT_FEED_FG_SEP_B 60

uint32_t yt_feed_ctx_init(yt_feed_ctx_t *ctx, uint32_t thumb_cache_max)
{
    if (LDG_UNLIKELY(!ctx)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(ctx, 0, sizeof(*ctx));
    return yt_thumb_cache_init(&ctx->thumbs, thumb_cache_max);
}

void yt_feed_ctx_shutdown(yt_feed_ctx_t *ctx)
{
    if (LDG_UNLIKELY(!ctx)) { return; }

    yt_feed_thumb_planes_destroy(ctx);
    yt_thumb_cache_shutdown(&ctx->thumbs);
}

void yt_feed_thumb_planes_destroy(yt_feed_ctx_t *ctx)
{
    if (LDG_UNLIKELY(!ctx)) { return; }

    uint32_t i = 0;
    for (; i < ctx->thumb_plane_cunt; i++) { if (ctx->thumb_planes[i])
        {
            ncplane_destroy(ctx->thumb_planes[i]);
            ctx->thumb_planes[i] = 0x0;
        }
    }
    ctx->thumb_plane_cunt = 0;
}

uint32_t yt_feed_render(yt_feed_ctx_t *ctx, struct ncplane *plane)
{
    if (LDG_UNLIKELY(!ctx)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!plane)) { return LDG_ERR_FUNC_ARG_NULL; }

    yt_feed_thumb_planes_destroy(ctx);
    ncplane_erase(plane);

    struct notcurses *nc = ncplane_notcurses(plane);

    unsigned p_rows = 0;
    unsigned p_cols = 0;
    ncplane_dim_yx(plane, &p_rows, &p_cols);
    uint32_t rows = (uint32_t)p_rows;
    uint32_t cols = (uint32_t)p_cols;

    uint32_t visible_cunt = rows / YT_FEED_ROWS_PER_ENTRY;
    uint32_t video_cunt = ctx->feed.video_cunt;

    if (ctx->loading)
    {
        uint32_t center_y = rows / 2;
        uint32_t msg_len = 10;
        uint32_t center_x = (cols > msg_len) ? (cols - msg_len) / 2 : 0;
        ncplane_set_fg_rgb8(plane, YT_FEED_FG_TITLE_R, YT_FEED_FG_TITLE_G, YT_FEED_FG_TITLE_B);
        ncplane_putstr_yx(plane, (int)center_y, (int)center_x, "Loading...");
        return LDG_ERR_AOK;
    }

    uint32_t i = ctx->scroll_offset;
    uint32_t end = ctx->scroll_offset + visible_cunt;
    if (end > video_cunt) { end = video_cunt; }

    for (; i < end; i++)
    {
        const yt_video_t *v = &ctx->feed.videos[i];
        uint32_t row_off = (i - ctx->scroll_offset) * YT_FEED_ROWS_PER_ENTRY;
        uint32_t text_col = 2;

        // thumbnail
        struct ncvisual *vis = yt_thumb_cache_get(&ctx->thumbs, v->id);
        if (vis && ctx->thumb_plane_cunt < YT_FEED_MAX_VISIBLE)
        {
            struct ncplane_options nopts = LDG_STRUCT_ZERO_INIT;
            nopts.y = (int)row_off;
            nopts.x = 2;
            nopts.rows = YT_FEED_THUMB_ROWS;
            nopts.cols = YT_FEED_THUMB_COLS;

            struct ncplane *tp = ncplane_create(plane, &nopts);
            if (tp)
            {
                struct ncvisual_options vopts = LDG_STRUCT_ZERO_INIT;
                vopts.n = tp;
                vopts.scaling = NCSCALE_STRETCH;
                vopts.blitter = NCBLIT_PIXEL;
                ncvisual_blit(nc, vis, &vopts);

                ctx->thumb_planes[ctx->thumb_plane_cunt] = tp;
                ctx->thumb_plane_cunt++;
            }

            text_col = YT_FEED_TEXT_COL;
        }

        if (i == ctx->selected_idx) { ncplane_set_bg_rgb8(plane, YT_FEED_BG_SEL_R, YT_FEED_BG_SEL_G, YT_FEED_BG_SEL_B); }
        else{ ncplane_set_bg_default(plane); }

        // title
        ncplane_set_fg_rgb8(plane, YT_FEED_FG_TITLE_R, YT_FEED_FG_TITLE_G, YT_FEED_FG_TITLE_B);
        uint32_t max_title = (cols > text_col + YT_FEED_TITLE_PAD) ? cols - text_col - YT_FEED_TITLE_PAD : 1;
        ncplane_printf_yx(plane, (int)row_off, (int)text_col, "%.*s", (int)max_title, v->title);

        // channel | duration | views
        ncplane_set_fg_rgb8(plane, YT_FEED_FG_CHAN_R, YT_FEED_FG_CHAN_G, YT_FEED_FG_CHAN_B);
        int col = (int)text_col;
        col += ncplane_putstr_yx(plane, (int)(row_off + 1), col, v->channel);

        ncplane_set_fg_rgb8(plane, YT_FEED_FG_VIEWS_R, YT_FEED_FG_VIEWS_G, YT_FEED_FG_VIEWS_B);
        col += ncplane_putstr_yx(plane, (int)(row_off + 1), col, " | ");

        ncplane_set_fg_rgb8(plane, YT_FEED_FG_DUR_R, YT_FEED_FG_DUR_G, YT_FEED_FG_DUR_B);
        col += ncplane_putstr_yx(plane, (int)(row_off + 1), col, v->duration_str);

        ncplane_set_fg_rgb8(plane, YT_FEED_FG_VIEWS_R, YT_FEED_FG_VIEWS_G, YT_FEED_FG_VIEWS_B);
        col += ncplane_putstr_yx(plane, (int)(row_off + 1), col, " | ");

        ncplane_set_fg_rgb8(plane, YT_FEED_FG_VIEWS_R, YT_FEED_FG_VIEWS_G, YT_FEED_FG_VIEWS_B);
        ncplane_printf_yx(plane, (int)(row_off + 1), col, "%lu views", (unsigned long)v->view_cunt);

        // separator
        ncplane_set_bg_default(plane);
        ncplane_set_fg_rgb8(plane, YT_FEED_FG_SEP_R, YT_FEED_FG_SEP_G, YT_FEED_FG_SEP_B);

        uint32_t c = 0;
        for (; c < cols; c++) { ncplane_putchar_yx(plane, (int)(row_off + YT_FEED_ROWS_PER_ENTRY - 1), (int)c, '-'); }
    }

    return LDG_ERR_AOK;
}

uint32_t yt_feed_nav_up(yt_feed_ctx_t *ctx)
{
    if (LDG_UNLIKELY(!ctx)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (ctx->selected_idx > 0)
    {
        ctx->selected_idx--;
        if (ctx->selected_idx < ctx->scroll_offset) { ctx->scroll_offset = ctx->selected_idx; }
    }

    return LDG_ERR_AOK;
}

uint32_t yt_feed_nav_down(yt_feed_ctx_t *ctx)
{
    if (LDG_UNLIKELY(!ctx)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (ctx->feed.video_cunt > 0 && ctx->selected_idx < ctx->feed.video_cunt - 1)
    {
        ctx->selected_idx++;
        uint32_t visible = 10;
        if (ctx->selected_idx >= ctx->scroll_offset + visible) { ctx->scroll_offset = ctx->selected_idx - visible + 1; }
    }

    return LDG_ERR_AOK;
}

const yt_video_t* yt_feed_selected_get(const yt_feed_ctx_t *ctx)
{
    if (LDG_UNLIKELY(!ctx)) { return 0x0; }

    if (ctx->feed.video_cunt == 0) { return 0x0; }

    if (ctx->selected_idx >= ctx->feed.video_cunt) { return 0x0; }

    return &ctx->feed.videos[ctx->selected_idx];
}
