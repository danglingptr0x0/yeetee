#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include <cJSON.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>

#include <yeetee/yeetee.h>
#include <yeetee/core/err.h>
#include <yeetee/api/ytdlp.h>
#include <yeetee/api/json.h>

#define YTDLP_BUFF_MAX     (256 * 1024)
#define YTDLP_SEARCH_MAX   32
#define YTDLP_LINE_MAX     (128 * 1024)
#define YTDLP_READ_CHUNK   4096

// ytdlp

static uint32_t exec_ytdlp_read(char *const argv[], char *buff, size_t buff_len, size_t *out_len)
{
    int32_t pipefd[2] = LDG_ARR_ZERO_INIT;
    pid_t pid = 0;
    ssize_t rd = 0;
    size_t total = 0;
    int32_t status = 0;

    if (pipe(pipefd) != 0) { return YT_ERR_API_YTDLP_SPAWN; }

    pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return YT_ERR_API_YTDLP_SPAWN;
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipefd[1]);

    while (total < buff_len - 1)
    {
        size_t remaining = buff_len - 1 - total;
        size_t chunk = remaining < YTDLP_READ_CHUNK ? remaining : YTDLP_READ_CHUNK;
        rd = read(pipefd[0], buff + total, chunk);
        if (rd <= 0) { break; }

        total += (size_t)rd;
    }
    buff[total] = '\0';

    close(pipefd[0]);

    waitpid(pid, &status, 0);

    if (out_len) { *out_len = total; }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { return YT_ERR_API_YTDLP; }

    return LDG_ERR_AOK;
}

static void parse_ytdlp_formats(cJSON *root, yt_stream_set_t *streams)
{
    cJSON *formats = cJSON_GetObjectItemCaseSensitive(root, "formats");
    if (!formats || !cJSON_IsArray(formats)) { return; }

    uint32_t fmt_cunt = (uint32_t)cJSON_GetArraySize(formats);
    uint32_t idx = 0;

    for (idx = 0; idx < fmt_cunt && streams->stream_cunt < YT_STREAM_MAX; idx++)
    {
        cJSON *fmt = cJSON_GetArrayItem(formats, (int)idx);
        yt_stream_t *stream = &streams->streams[streams->stream_cunt];
        cJSON *tmp = NULL;

        memset(stream, 0, sizeof(*stream));

        tmp = cJSON_GetObjectItemCaseSensitive(fmt, "url");
        if (!tmp || !cJSON_IsString(tmp)) { continue; }

        snprintf(stream->url, sizeof(stream->url), "%s", tmp->valuestring);

        tmp = cJSON_GetObjectItemCaseSensitive(fmt, "format_id");
        if (tmp && cJSON_IsString(tmp))
        {
            uint32_t fid = 0;
            const char *p = tmp->valuestring;
            while (*p >= '0' && *p <= '9') { fid = fid * 10 + (uint32_t)(*p++ - '0'); }
            stream->itag = fid;
        }

        tmp = cJSON_GetObjectItemCaseSensitive(fmt, "width");
        if (tmp && cJSON_IsNumber(tmp)) { stream->width = (uint32_t)tmp->valueint; }

        tmp = cJSON_GetObjectItemCaseSensitive(fmt, "height");
        if (tmp && cJSON_IsNumber(tmp)) { stream->height = (uint32_t)tmp->valueint; }

        tmp = cJSON_GetObjectItemCaseSensitive(fmt, "tbr");
        if (tmp && cJSON_IsNumber(tmp)) { stream->bitrate = (uint32_t)(tmp->valuedouble * 1000.0); }

        cJSON *acodec = cJSON_GetObjectItemCaseSensitive(fmt, "acodec");
        if (acodec && cJSON_IsString(acodec) && strcmp(acodec->valuestring, "none") != 0)
        {
            cJSON *vcodec = cJSON_GetObjectItemCaseSensitive(fmt, "vcodec");
            if (vcodec && cJSON_IsString(vcodec) && strcmp(vcodec->valuestring, "none") == 0)
            {
                stream->is_audio = 1;
                snprintf(stream->mime, sizeof(stream->mime), "audio/unknown");
            }
        }

        if (stream->mime[0] == '\0') { snprintf(stream->mime, sizeof(stream->mime), "video/unknown"); }

        streams->stream_cunt++;
    }
}

uint32_t yt_ytdlp_stream_url_get(const char *video_id, yt_stream_set_t *streams)
{
    if (LDG_UNLIKELY(!video_id)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!streams)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char *buff = NULL;
    size_t out_len = 0;

    memset(streams, 0, sizeof(*streams));

    char vid_arg[YT_VIDEO_ID_MAX];
    snprintf(vid_arg, sizeof(vid_arg), "%s", video_id);
    char *const argv[] = { (char *)"yt-dlp", (char *)"-j", (char *)"--no-download", (char *)"--", vid_arg, NULL };

    buff = (char *)malloc(YTDLP_BUFF_MAX);
    if (LDG_UNLIKELY(!buff)) { return LDG_ERR_ALLOC_NULL; }

    ret = exec_ytdlp_read(argv, buff, YTDLP_BUFF_MAX, &out_len);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        free(buff);
        return ret;
    }

    cJSON *root = cJSON_ParseWithLength(buff, out_len);
    if (LDG_UNLIKELY(!root))
    {
        free(buff);
        return YT_ERR_API_PARSE;
    }

    parse_ytdlp_formats(root, streams);

    cJSON_Delete(root);
    free(buff);

    if (streams->stream_cunt == 0) { return YT_ERR_API_YTDLP; }

    return LDG_ERR_AOK;
}

static void parse_ytdlp_video_line(const char *line, yt_video_t *video)
{
    memset(video, 0, sizeof(*video));

    cJSON *root = cJSON_Parse(line);
    if (!root) { return; }

    cJSON *tmp = NULL;

    tmp = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (tmp && cJSON_IsString(tmp)) { snprintf(video->id, sizeof(video->id), "%s", tmp->valuestring); }

    tmp = cJSON_GetObjectItemCaseSensitive(root, "title");
    if (tmp && cJSON_IsString(tmp)) { snprintf(video->title, sizeof(video->title), "%s", tmp->valuestring); }

    tmp = cJSON_GetObjectItemCaseSensitive(root, "channel");
    if (tmp && cJSON_IsString(tmp)) { snprintf(video->channel, sizeof(video->channel), "%s", tmp->valuestring); }

    tmp = cJSON_GetObjectItemCaseSensitive(root, "thumbnail");
    if (tmp && cJSON_IsString(tmp)) { snprintf(video->thumb_url, sizeof(video->thumb_url), "%s", tmp->valuestring); }

    tmp = cJSON_GetObjectItemCaseSensitive(root, "duration");
    if (tmp && cJSON_IsNumber(tmp))
    {
        video->duration_secs = (uint64_t)tmp->valuedouble;
        uint32_t total = (uint32_t)(video->duration_secs % 360000);
        uint32_t hrs = total / 3600;
        uint32_t mins = (total % 3600) / 60;
        uint32_t secs = total % 60;
        if (hrs > 0) { snprintf(video->duration_str, sizeof(video->duration_str), "%02u:%02u:%02u", (unsigned)hrs, (unsigned)mins, (unsigned)secs); }
        else{ snprintf(video->duration_str, sizeof(video->duration_str), "%02u:%02u", (unsigned)mins, (unsigned)secs); }
    }

    tmp = cJSON_GetObjectItemCaseSensitive(root, "view_count");
    if (tmp && cJSON_IsNumber(tmp)) { video->view_cunt = (uint64_t)tmp->valuedouble; }

    cJSON_Delete(root);
}

uint32_t yt_ytdlp_search(const char *query, uint32_t max_results, yt_feed_t *feed)
{
    if (LDG_UNLIKELY(!query)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!feed)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char *buff = NULL;
    size_t out_len = 0;
    char search_str[YTDLP_SEARCH_MAX + YT_VIDEO_TITLE_MAX] = LDG_ARR_ZERO_INIT;

    memset(feed, 0, sizeof(*feed));

    if (max_results == 0 || max_results > YT_FEED_MAX_VIDEOS) { max_results = YT_FEED_MAX_VIDEOS; }

    snprintf(search_str, sizeof(search_str), "ytsearch%u:%s", max_results, query);

    char *const argv[] = { (char *)"yt-dlp", (char *)"--dump-json", (char *)"--flat-playlist", (char *)"--", search_str, NULL };

    buff = (char *)malloc(YTDLP_BUFF_MAX);
    if (LDG_UNLIKELY(!buff)) { return LDG_ERR_ALLOC_NULL; }

    ret = exec_ytdlp_read(argv, buff, YTDLP_BUFF_MAX, &out_len);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        free(buff);
        return ret;
    }

    char *line = buff;
    char *nl = NULL;

    while ((nl = strchr(line, '\n')) != NULL && feed->video_cunt < YT_FEED_MAX_VIDEOS)
    {
        *nl = '\0';

        if (*line != '\0')
        {
            parse_ytdlp_video_line(line, &feed->videos[feed->video_cunt]);
            if (feed->videos[feed->video_cunt].id[0] != '\0') { feed->video_cunt++; }
        }

        line = nl + 1;
    }

    if (*line != '\0' && feed->video_cunt < YT_FEED_MAX_VIDEOS)
    {
        parse_ytdlp_video_line(line, &feed->videos[feed->video_cunt]);
        if (feed->videos[feed->video_cunt].id[0] != '\0') { feed->video_cunt++; }
    }

    free(buff);

    if (feed->video_cunt == 0) { return YT_ERR_API_YTDLP; }

    return LDG_ERR_AOK;
}
