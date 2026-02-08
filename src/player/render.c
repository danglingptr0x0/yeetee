#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <syslog.h>
#include <pthread.h>
#include <mpv/client.h>
#include <mpv/render.h>
#include <notcurses/notcurses.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <yeetee/core/err.h>
#include <yeetee/player/render.h>

#define YT_RENDER_BPP        4
#define YT_RENDER_MAX_W      1920
#define YT_RENDER_MAX_H      1080
#define YT_RENDER_ALPHA_MASK 0xFF000000
#define YT_RENDER_SHM_PATH_0 "/dev/shm/yeetee_frame_0"
#define YT_RENDER_SHM_PATH_1 "/dev/shm/yeetee_frame_1"
#define YT_RENDER_SHM_B64_0  "L2Rldi9zaG0veWVldGVlX2ZyYW1lXzA="
#define YT_RENDER_SHM_B64_1  "L2Rldi9zaG0veWVldGVlX2ZyYW1lXzE="

static void render_update_cb(void *arg)
{
    yt_render_t *render = (yt_render_t *)arg;
    render->frame_ready = 1;
}

static void render_frame(yt_render_t *render)
{
    if (!render->frame_ready) { return; }

    render->frame_ready = 0;

    uint8_t idx = render->back;
    int skip_target = 0;
    int sw_size[2] = { (int)render->pixel_w, (int)render->pixel_h };
    size_t stride = render->stride;

    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_SW_SIZE, &sw_size },
        { MPV_RENDER_PARAM_SW_FORMAT, (void *)"rgb0" },
        { MPV_RENDER_PARAM_SW_STRIDE, &stride },
        { MPV_RENDER_PARAM_SW_POINTER, render->shm_map[idx] },
        { MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &skip_target },
        { MPV_RENDER_PARAM_INVALID, NULL }
    };

    int ret = mpv_render_context_render(render->ctx, params);
    if (LDG_UNLIKELY(ret < 0)) { return; }

    uint32_t *pixels = (uint32_t *)render->shm_map[idx];
    size_t pixel_cunt = render->shm_size / YT_RENDER_BPP;
    size_t i = 0;
    for (; i < pixel_cunt; i++) { pixels[i] |= YT_RENDER_ALPHA_MASK; }

    pthread_mutex_lock(&render->stdout_mut);
    ssize_t wr = write(STDOUT_FILENO, render->esc_cmd[idx], render->esc_len[idx]);
    pthread_mutex_unlock(&render->stdout_mut);
    if (LDG_UNLIKELY(wr < 0)) { return; }

    render->back = 1 - idx;

    render->frame_cunt++;
    struct timespec ts;
    memset(&ts, 0, sizeof(ts));
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_ns = (uint64_t)ts.tv_sec * LDG_NS_PER_SEC + (uint64_t)ts.tv_nsec;

    if (now_ns - render->fps_epoch_ns >= LDG_NS_PER_SEC)
    {
        render->fps = render->frame_cunt;
        render->frame_cunt = 0;
        render->fps_epoch_ns = now_ns;
    }
}

static void* render_loop(void *arg)
{
    yt_render_t *render = (yt_render_t *)arg;

    while (render->running)
    {
        render_frame(render);

        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000;
        nanosleep(&ts, 0x0);
    }

    return 0x0;
}

uint32_t yt_render_init(yt_render_t *render, mpv_handle *mpv, struct ncplane *parent, uint32_t cell_rows, uint32_t cell_cols)
{
    if (LDG_UNLIKELY(!render)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!mpv)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!parent)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(render, 0, sizeof(*render));
    render->shm_fd[0] = -1;
    render->shm_fd[1] = -1;

    unsigned pix_y = 0;
    unsigned pix_x = 0;
    unsigned cell_px_y = 0;
    unsigned cell_px_x = 0;
    unsigned max_bmap_y = 0;
    unsigned max_bmap_x = 0;
    ncplane_pixel_geom(parent, &pix_y, &pix_x, &cell_px_y, &cell_px_x, &max_bmap_y, &max_bmap_x);

    if (LDG_UNLIKELY(cell_px_x == 0 || cell_px_y == 0))
    {
        syslog(LOG_ERR, "render_init; cell pixel geom zero; cell_px_x: %u; cell_px_y: %u", cell_px_x, cell_px_y);
        return YT_ERR_PLAYER_RENDER_INIT;
    }

    uint32_t native_w = cell_cols * cell_px_x;
    uint32_t native_h = cell_rows * cell_px_y;
    render->pixel_w = native_w;
    render->pixel_h = native_h;

    if (render->pixel_w > YT_RENDER_MAX_W || render->pixel_h > YT_RENDER_MAX_H)
    {
        if (render->pixel_w * YT_RENDER_MAX_H > render->pixel_h * YT_RENDER_MAX_W)
        {
            render->pixel_h = (render->pixel_h * YT_RENDER_MAX_W) / render->pixel_w;
            render->pixel_w = YT_RENDER_MAX_W;
        }
        else
        {
            render->pixel_w = (render->pixel_w * YT_RENDER_MAX_H) / render->pixel_h;
            render->pixel_h = YT_RENDER_MAX_H;
        }
    }

    render->stride = (size_t)render->pixel_w * YT_RENDER_BPP;
    render->shm_size = render->stride * render->pixel_h;

    syslog(LOG_INFO, "render_init; native: %ux%u; scaled: %ux%u; stride: %zu; cell_px: %ux%u", native_w, native_h, render->pixel_w, render->pixel_h, render->stride, cell_px_x, cell_px_y);

    // shm
    const char *shm_paths[2] = { YT_RENDER_SHM_PATH_0, YT_RENDER_SHM_PATH_1 };

    uint32_t b = 0;
    for (; b < 2; b++)
    {
        render->shm_fd[b] = open(shm_paths[b], O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (LDG_UNLIKELY(render->shm_fd[b] < 0))
        {
            syslog(LOG_ERR, "render_init; open shm failed; idx: %u", b);
            if (b == 1) { munmap(render->shm_map[0], render->shm_size); render->shm_map[0] = 0x0; close(render->shm_fd[0]); render->shm_fd[0] = -1; }

            return YT_ERR_PLAYER_RENDER_INIT;
        }

        int ft = ftruncate(render->shm_fd[b], (off_t)render->shm_size);
        if (LDG_UNLIKELY(ft < 0))
        {
            syslog(LOG_ERR, "render_init; ftruncate shm failed; idx: %u", b);
            close(render->shm_fd[b]);
            render->shm_fd[b] = -1;
            if (b == 1) { munmap(render->shm_map[0], render->shm_size); render->shm_map[0] = 0x0; close(render->shm_fd[0]); render->shm_fd[0] = -1; }

            return YT_ERR_PLAYER_RENDER_INIT;
        }

        render->shm_map[b] = (uint8_t *)mmap(0x0, render->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, render->shm_fd[b], 0);
        if (LDG_UNLIKELY(render->shm_map[b] == MAP_FAILED))
        {
            syslog(LOG_ERR, "render_init; mmap shm failed; idx: %u", b);
            render->shm_map[b] = 0x0;
            close(render->shm_fd[b]);
            render->shm_fd[b] = -1;
            if (b == 1) { munmap(render->shm_map[0], render->shm_size); render->shm_map[0] = 0x0; close(render->shm_fd[0]); render->shm_fd[0] = -1; }

            return YT_ERR_PLAYER_RENDER_INIT;
        }
    }

    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE, (void *)MPV_RENDER_API_TYPE_SW },
        { MPV_RENDER_PARAM_INVALID, NULL }
    };

    int ret = mpv_render_context_create(&render->ctx, mpv, params);
    if (LDG_UNLIKELY(ret < 0))
    {
        syslog(LOG_ERR, "render_init; mpv_render_context_create failed; ret: %d", ret);
        for (b = 0; b < 2; b++) { munmap(render->shm_map[b], render->shm_size); render->shm_map[b] = 0x0; close(render->shm_fd[b]); render->shm_fd[b] = -1; }
        return YT_ERR_PLAYER_RENDER_INIT;
    }

    mpv_render_context_set_update_callback(render->ctx, render_update_cb, render);

    render->video_cell_cols = cell_cols;
    render->video_cell_rows = cell_rows;

    unsigned parent_rows = 0;
    unsigned parent_cols = 0;
    ncplane_dim_yx(parent, &parent_rows, &parent_cols);
    int offset_x = (parent_cols > cell_cols) ? (int)(parent_cols - cell_cols) / 2 : 0;

    ncplane_options nopts;
    memset(&nopts, 0, sizeof(nopts));
    nopts.y = 0;
    nopts.x = offset_x;
    nopts.rows = cell_rows;
    nopts.cols = cell_cols;

    render->video_plane = ncplane_create(parent, &nopts);
    if (LDG_UNLIKELY(!render->video_plane))
    {
        syslog(LOG_ERR, "render_init; ncplane_create failed");
        mpv_render_context_free(render->ctx);
        render->ctx = 0x0;
        for (b = 0; b < 2; b++) { munmap(render->shm_map[b], render->shm_size); render->shm_map[b] = 0x0; close(render->shm_fd[b]); render->shm_fd[b] = -1; }
        return YT_ERR_PLAYER_RENDER_INIT;
    }

    int abs_y = 0;
    int abs_x = 0;
    ncplane_abs_yx(render->video_plane, &abs_y, &abs_x);
    render->video_abs_y = (uint32_t)abs_y;
    render->video_abs_x = (uint32_t)abs_x;

    // esc
    const char *shm_b64[2] = { YT_RENDER_SHM_B64_0, YT_RENDER_SHM_B64_1 };
    uint32_t kitty_id[2] = { 1, 2 };

    for (b = 0; b < 2; b++)
    {
        uint32_t other = 1 - b;
        memset(render->esc_cmd[b], 0, sizeof(render->esc_cmd[b]));
        int esc_ret = snprintf(render->esc_cmd[b], sizeof(render->esc_cmd[b]), "\x1b[?2026h"
            "\x1b[%u;%uH"
            "\x1b_Ga=T,q=2,f=32,s=%u,v=%u,i=%u,t=f,c=%u,r=%u;%s\x1b\\"
            "\x1b_Ga=d,d=I,i=%u,q=2;\x1b\\"
            "\x1b[?2026l", render->video_abs_y + 1, render->video_abs_x + 1, render->pixel_w, render->pixel_h, kitty_id[b], render->video_cell_cols, render->video_cell_rows, shm_b64[b], kitty_id[other]);
        if (LDG_UNLIKELY(esc_ret < 0 || esc_ret >= (int)sizeof(render->esc_cmd[b])))
        {
            syslog(LOG_ERR, "render_init; esc_cmd overflow; idx: %u", b);
            mpv_render_context_free(render->ctx);
            render->ctx = 0x0;
            ncplane_destroy(render->video_plane);
            render->video_plane = 0x0;
            for (uint32_t c = 0; c < 2; c++) { munmap(render->shm_map[c], render->shm_size); render->shm_map[c] = 0x0; close(render->shm_fd[c]); render->shm_fd[c] = -1; }
            return YT_ERR_PLAYER_RENDER_INIT;
        }

        render->esc_len[b] = (uint32_t)esc_ret;
    }

    struct timespec init_ts;
    memset(&init_ts, 0, sizeof(init_ts));
    clock_gettime(CLOCK_MONOTONIC, &init_ts);
    render->fps_epoch_ns = (uint64_t)init_ts.tv_sec * LDG_NS_PER_SEC + (uint64_t)init_ts.tv_nsec;

    syslog(LOG_INFO, "render_init; video abs: %u,%u; cells: %ux%u; shm_size: %zu; esc_len: %u/%u", render->video_abs_y, render->video_abs_x, render->video_cell_cols, render->video_cell_rows, render->shm_size, render->esc_len[0], render->esc_len[1]);

    pthread_mutex_init(&render->stdout_mut, 0x0);
    render->running = 1;
    int pret = pthread_create(&render->render_thread, 0x0, render_loop, render);
    if (LDG_UNLIKELY(pret != 0))
    {
        syslog(LOG_ERR, "render_init; pthread_create failed; ret: %d", pret);
        render->running = 0;
        pthread_mutex_destroy(&render->stdout_mut);
        mpv_render_context_free(render->ctx);
        render->ctx = 0x0;
        ncplane_destroy(render->video_plane);
        render->video_plane = 0x0;
        for (b = 0; b < 2; b++) { munmap(render->shm_map[b], render->shm_size); render->shm_map[b] = 0x0; close(render->shm_fd[b]); render->shm_fd[b] = -1; }
        return YT_ERR_PLAYER_RENDER_INIT;
    }

    return LDG_ERR_AOK;
}

void yt_render_shutdown(yt_render_t *render)
{
    if (LDG_UNLIKELY(!render)) { return; }

    if (render->running)
    {
        render->running = 0;
        pthread_join(render->render_thread, 0x0);
    }

    pthread_mutex_destroy(&render->stdout_mut);

    char del_esc[] = "\x1b_Ga=d,d=I,i=1,q=2;\x1b\\\x1b_Ga=d,d=I,i=2,q=2;\x1b\\";
    ssize_t wr = write(STDOUT_FILENO, del_esc, sizeof(del_esc) - 1);
    if (wr < 0) { syslog(LOG_ERR, "render_shutdown; kitty delete write failed"); }

    if (render->ctx)
    {
        mpv_render_context_free(render->ctx);
        render->ctx = 0x0;
    }

    if (render->video_plane)
    {
        ncplane_destroy(render->video_plane);
        render->video_plane = 0x0;
    }

    uint32_t b = 0;
    for (; b < 2; b++)
    {
        if (render->shm_map[b])
        {
            munmap(render->shm_map[b], render->shm_size);
            render->shm_map[b] = 0x0;
        }

        if (render->shm_fd[b] >= 0)
        {
            close(render->shm_fd[b]);
            render->shm_fd[b] = -1;
        }
    }

    int ul = unlink(YT_RENDER_SHM_PATH_0);
    if (ul < 0) { syslog(LOG_ERR, "render_shutdown; shm unlink failed; idx: 0"); }

    ul = unlink(YT_RENDER_SHM_PATH_1);
    if (ul < 0) { syslog(LOG_ERR, "render_shutdown; shm unlink failed; idx: 1"); }
}

void yt_render_stdout_lock(yt_render_t *render)
{
    pthread_mutex_lock(&render->stdout_mut);
}

void yt_render_stdout_unlock(yt_render_t *render)
{
    pthread_mutex_unlock(&render->stdout_mut);
}
