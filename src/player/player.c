#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <pthread.h>
#include <mpv/client.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <dangling/thread/spsc.h>
#include <yeetee/core/err.h>
#include <yeetee/player/player.h>

static void* event_loop(void *arg)
{
    yt_player_t *player = (yt_player_t *)arg;

    while (player->running)
    {
        mpv_event *ev = mpv_wait_event(player->mpv, 0.1);
        if (LDG_UNLIKELY(!ev)) { continue; }

        if (ev->event_id == MPV_EVENT_NONE) { continue; }

        if (ev->event_id == MPV_EVENT_SHUTDOWN) { break; }

        if (ev->event_id == MPV_EVENT_PROPERTY_CHANGE)
        {
            mpv_event_property *prop = (mpv_event_property *)ev->data;
            if (LDG_UNLIKELY(!prop)) { continue; }

            yt_player_event_t pe = LDG_STRUCT_ZERO_INIT;
            pe.state = YT_PLAYER_PLAYING;

            if (ev->reply_userdata == 1 && prop->format == MPV_FORMAT_DOUBLE) { pe.time_pos = *(double *)prop->data; }
            else if (ev->reply_userdata == 2 && prop->format == MPV_FORMAT_DOUBLE) { pe.duration = *(double *)prop->data; }
            else if (ev->reply_userdata == 3 && prop->format == MPV_FORMAT_FLAG)
            {
                int paused = *(int *)prop->data;
                pe.state = paused ? YT_PLAYER_PAUSED : YT_PLAYER_PLAYING;
            }
            else if (ev->reply_userdata == 4 && prop->format == MPV_FORMAT_FLAG)
            {
                int eof = *(int *)prop->data;
                pe.eof_reached = (uint8_t)eof;
            }

            ldg_spsc_push(&player->event_q, &pe);
        }

        if (ev->event_id == MPV_EVENT_END_FILE)
        {
            mpv_event_end_file *ef = (mpv_event_end_file *)ev->data;
            uint32_t reason = ef ? (uint32_t)ef->reason : 999;
            int32_t err_code = ef ? ef->error : 0;
            syslog(LOG_INFO, "mpv end_file; reason: %u; err: %d", reason, err_code);

            yt_player_event_t pe = LDG_STRUCT_ZERO_INIT;
            pe.state = YT_PLAYER_STOPPED;
            pe.eof_reached = 1;
            ldg_spsc_push(&player->event_q, &pe);
        }

        if (ev->event_id == MPV_EVENT_LOG_MESSAGE)
        {
            mpv_event_log_message *msg = (mpv_event_log_message *)ev->data;
            if (msg) { syslog(LOG_INFO, "mpv [%s] %s: %s", msg->level, msg->prefix, msg->text); }
        }
    }

    return 0x0;
}

uint32_t yt_player_init(yt_player_t *player)
{
    if (LDG_UNLIKELY(!player)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(player, 0, sizeof(*player));

    player->mpv = mpv_create();
    if (LDG_UNLIKELY(!player->mpv)) { return YT_ERR_PLAYER_INIT; }

    mpv_set_option_string(player->mpv, "vo", "libmpv");
    mpv_set_option_string(player->mpv, "ao", "pulse,alsa");
    mpv_set_option_string(player->mpv, "ytdl", "yes");
    mpv_set_option_string(player->mpv, "terminal", "no");
    mpv_set_option_string(player->mpv, "msg-level", "all=no");
    mpv_set_option_string(player->mpv, "save-position-on-quit", "yes");
    mpv_set_option_string(player->mpv, "cache", "yes");
    mpv_set_option_string(player->mpv, "demuxer-max-bytes", "52428800");
    mpv_set_option_string(player->mpv, "demuxer-readahead-secs", "30");

    int ret = mpv_initialize(player->mpv);
    if (LDG_UNLIKELY(ret < 0))
    {
        mpv_destroy(player->mpv);
        player->mpv = 0x0;
        return YT_ERR_PLAYER_INIT;
    }

    mpv_request_log_messages(player->mpv, "warn");
    mpv_observe_property(player->mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(player->mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(player->mpv, 3, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(player->mpv, 4, "eof-reached", MPV_FORMAT_FLAG);

    uint32_t err = ldg_spsc_init(&player->event_q, sizeof(yt_player_event_t), 64);
    if (LDG_UNLIKELY(err != LDG_ERR_AOK))
    {
        mpv_terminate_destroy(player->mpv);
        player->mpv = 0x0;
        return err;
    }

    player->running = 1;

    int pret = pthread_create(&player->event_thread, 0x0, event_loop, player);
    if (LDG_UNLIKELY(pret != 0))
    {
        ldg_spsc_shutdown(&player->event_q);
        mpv_terminate_destroy(player->mpv);
        player->mpv = 0x0;
        player->running = 0;
        return YT_ERR_PLAYER_INIT;
    }

    return LDG_ERR_AOK;
}

void yt_player_shutdown(yt_player_t *player)
{
    if (LDG_UNLIKELY(!player)) { return; }

    player->running = 0;
    int join_ret = pthread_join(player->event_thread, 0x0);
    if (LDG_UNLIKELY(join_ret != 0)) { syslog(LOG_ERR, "player_shutdown; pthread_join failed; ret: %d", join_ret); }

    ldg_spsc_shutdown(&player->event_q);

    if (player->mpv)
    {
        mpv_terminate_destroy(player->mpv);
        player->mpv = 0x0;
    }
}

uint32_t yt_player_load(yt_player_t *player, const char *url)
{
    if (LDG_UNLIKELY(!player)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!url)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!player->mpv)) { return LDG_ERR_NOT_INIT; }

    const char *cmd[] = { "loadfile", url, 0x0 };
    int ret = mpv_command(player->mpv, cmd);
    if (LDG_UNLIKELY(ret < 0)) { return YT_ERR_PLAYER_LOAD; }

    return LDG_ERR_AOK;
}

uint32_t yt_player_pause(yt_player_t *player)
{
    if (LDG_UNLIKELY(!player)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!player->mpv)) { return LDG_ERR_NOT_INIT; }

    const char *cmd[] = { "set", "pause", "yes", 0x0 };
    int ret = mpv_command(player->mpv, cmd);
    if (LDG_UNLIKELY(ret < 0)) { return YT_ERR_PLAYER_LOAD; }

    return LDG_ERR_AOK;
}

uint32_t yt_player_resume(yt_player_t *player)
{
    if (LDG_UNLIKELY(!player)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!player->mpv)) { return LDG_ERR_NOT_INIT; }

    const char *cmd[] = { "set", "pause", "no", 0x0 };
    int ret = mpv_command(player->mpv, cmd);
    if (LDG_UNLIKELY(ret < 0)) { return YT_ERR_PLAYER_LOAD; }

    return LDG_ERR_AOK;
}

uint32_t yt_player_seek(yt_player_t *player, double secs)
{
    if (LDG_UNLIKELY(!player)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!player->mpv)) { return LDG_ERR_NOT_INIT; }

    char secs_str[32] = LDG_ARR_ZERO_INIT;
    snprintf(secs_str, sizeof(secs_str), "%.1f", secs);

    const char *cmd[] = { "seek", secs_str, 0x0 };
    int ret = mpv_command(player->mpv, cmd);
    if (LDG_UNLIKELY(ret < 0)) { return YT_ERR_PLAYER_LOAD; }

    return LDG_ERR_AOK;
}

uint32_t yt_player_stop(yt_player_t *player)
{
    if (LDG_UNLIKELY(!player)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!player->mpv)) { return LDG_ERR_NOT_INIT; }

    const char *cmd[] = { "stop", 0x0 };
    int ret = mpv_command(player->mpv, cmd);
    if (LDG_UNLIKELY(ret < 0)) { return YT_ERR_PLAYER_LOAD; }

    return LDG_ERR_AOK;
}

uint32_t yt_player_volume_set(yt_player_t *player, uint32_t volume)
{
    if (LDG_UNLIKELY(!player)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!player->mpv)) { return LDG_ERR_NOT_INIT; }

    char vol_str[16] = LDG_ARR_ZERO_INIT;
    snprintf(vol_str, sizeof(vol_str), "%u", volume);

    const char *cmd[] = { "set", "volume", vol_str, 0x0 };
    int ret = mpv_command(player->mpv, cmd);
    if (LDG_UNLIKELY(ret < 0)) { return YT_ERR_PLAYER_LOAD; }

    return LDG_ERR_AOK;
}
