#include <stdint.h>
#include <string.h>
#include <notcurses/notcurses.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <yeetee/core/err.h>
#include <yeetee/tui/layout.h>

uint32_t yt_layout_init(yt_layout_t *layout, struct notcurses *nc)
{
    if (LDG_UNLIKELY(!layout)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!nc)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(layout, 0, sizeof(*layout));

    struct ncplane *stdplane = notcurses_stdplane(nc);
    unsigned rows = 0;
    unsigned cols = 0;
    ncplane_dim_yx(stdplane, &rows, &cols);

    layout->term_rows = rows;
    layout->term_cols = cols;

    uint32_t content_rows = rows - YT_LAYOUT_HEADER_ROWS - YT_LAYOUT_STATUS_ROWS;

    ncplane_options hdr_opts;
    memset(&hdr_opts, 0, sizeof(hdr_opts));
    hdr_opts.y = 0;
    hdr_opts.x = 0;
    hdr_opts.rows = YT_LAYOUT_HEADER_ROWS;
    hdr_opts.cols = cols;

    layout->header = ncplane_create(stdplane, &hdr_opts);
    if (LDG_UNLIKELY(!layout->header)) { return YT_ERR_TUI_LAYOUT; }

    ncplane_options content_opts;
    memset(&content_opts, 0, sizeof(content_opts));
    content_opts.y = YT_LAYOUT_HEADER_ROWS;
    content_opts.x = 0;
    content_opts.rows = content_rows;
    content_opts.cols = cols;

    layout->content = ncplane_create(stdplane, &content_opts);
    if (LDG_UNLIKELY(!layout->content))
    {
        ncplane_destroy(layout->header);
        layout->header = 0x0;
        return YT_ERR_TUI_LAYOUT;
    }

    ncplane_options status_opts;
    memset(&status_opts, 0, sizeof(status_opts));
    status_opts.y = (int)(rows - YT_LAYOUT_STATUS_ROWS);
    status_opts.x = 0;
    status_opts.rows = YT_LAYOUT_STATUS_ROWS;
    status_opts.cols = cols;

    layout->status = ncplane_create(stdplane, &status_opts);
    if (LDG_UNLIKELY(!layout->status))
    {
        ncplane_destroy(layout->content);
        layout->content = 0x0;
        ncplane_destroy(layout->header);
        layout->header = 0x0;
        return YT_ERR_TUI_LAYOUT;
    }

    layout->overlay = 0x0;

    return LDG_ERR_AOK;
}

void yt_layout_shutdown(yt_layout_t *layout)
{
    if (LDG_UNLIKELY(!layout)) { return; }

    if (layout->overlay)
    {
        ncplane_destroy(layout->overlay);
        layout->overlay = 0x0;
    }

    if (layout->status)
    {
        ncplane_destroy(layout->status);
        layout->status = 0x0;
    }

    if (layout->content)
    {
        ncplane_destroy(layout->content);
        layout->content = 0x0;
    }

    if (layout->header)
    {
        ncplane_destroy(layout->header);
        layout->header = 0x0;
    }
}

uint32_t yt_layout_resize(yt_layout_t *layout, struct notcurses *nc)
{
    if (LDG_UNLIKELY(!layout)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!nc)) { return LDG_ERR_FUNC_ARG_NULL; }

    yt_layout_shutdown(layout);
    return yt_layout_init(layout, nc);
}

uint32_t yt_layout_header_render(yt_layout_t *layout, const char *title)
{
    if (LDG_UNLIKELY(!layout)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!layout->header)) { return LDG_ERR_NOT_INIT; }

    ncplane_erase(layout->header);

    // logo
    ncplane_set_bg_rgb8(layout->header, 204, 0, 0);
    ncplane_set_fg_rgb8(layout->header, 255, 255, 255);
    ncplane_putstr_yx(layout->header, 0, 1, "           ");
    ncplane_putstr_yx(layout->header, 1, 1, "     >     ");
    ncplane_putstr_yx(layout->header, 2, 1, "           ");
    ncplane_set_bg_default(layout->header);

    ncplane_putstr_yx(layout->header, 1, 13, "Premium");
    ncplane_set_fg_rgb8(layout->header, 200, 200, 200);

    if (title) { ncplane_putstr_yx(layout->header, 1, 22, title); }

    return LDG_ERR_AOK;
}

uint32_t yt_layout_status_render(yt_layout_t *layout, const char *msg)
{
    if (LDG_UNLIKELY(!layout)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!layout->status)) { return LDG_ERR_NOT_INIT; }

    ncplane_erase(layout->status);
    ncplane_set_fg_rgb8(layout->status, 150, 150, 150);

    if (msg) { ncplane_putstr_yx(layout->status, 0, 2, msg); }

    return LDG_ERR_AOK;
}
