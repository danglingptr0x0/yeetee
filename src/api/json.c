#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <syslog.h>
#include <dangling/core/macros.h>
#include <dangling/core/err.h>

#include <cJSON.h>
#include <yeetee/yeetee.h>
#include <yeetee/core/err.h>
#include <yeetee/api/json.h>

#define JSON_LOG_CHUNK 900

// json

static void json_syslog_full_dump(const char *tag, const char *data, size_t len)
{
    size_t offset = 0;
    uint32_t chunk_idx = 0;

    while (offset < len)
    {
        size_t remaining = len - offset;
        size_t chunk_len = remaining < JSON_LOG_CHUNK ? remaining : JSON_LOG_CHUNK;
        syslog(LOG_DEBUG, "%s; chunk[%u]; offset: %zu; %.900s", tag, chunk_idx, offset, data + offset);
        offset += chunk_len;
        chunk_idx++;
    }
}

static const char* json_cjson_str_get(cJSON *root, const char *key)
{
    cJSON *obj = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!obj || !cJSON_IsString(obj)) { return 0x0; }

    return obj->valuestring;
}

static void json_str_copy(char *dst, size_t dst_len, const char *src)
{
    if (!src)
    {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_len, "%s", src);
}

static uint64_t json_view_cunt_parse(const char *text)
{
    uint64_t views = 0;
    const char *p = text;

    while (*p != '\0')
    {
        if (*p >= '0' && *p <= '9') { views = views * 10 + (uint64_t)(*p - '0'); }

        p++;
    }

    return views;
}

// tv tile renderer
static void json_tile_parse(cJSON *tile, yt_video_t *video)
{
    memset(video, 0, sizeof(*video));

    const char *content_id = json_cjson_str_get(tile, "contentId");
    json_str_copy(video->id, sizeof(video->id), content_id);

    if (video->id[0] == '\0')
    {
        cJSON *cmd = cJSON_GetObjectItemCaseSensitive(tile, "onSelectCommand");
        if (cmd)
        {
            cJSON *ep = cJSON_GetObjectItemCaseSensitive(cmd, "watchEndpoint");
            if (ep) { json_str_copy(video->id, sizeof(video->id), json_cjson_str_get(ep, "videoId")); }
        }
    }

    // metadata.tileMetadataRenderer
    cJSON *meta = cJSON_GetObjectItemCaseSensitive(tile, "metadata");
    if (meta)
    {
        cJSON *tmr = cJSON_GetObjectItemCaseSensitive(meta, "tileMetadataRenderer");
        if (tmr)
        {
            cJSON *title_obj = cJSON_GetObjectItemCaseSensitive(tmr, "title");
            if (title_obj) { json_str_copy(video->title, sizeof(video->title), json_cjson_str_get(title_obj, "simpleText")); }

            if (video->title[0] == '\0' && title_obj)
            {
                cJSON *runs = cJSON_GetObjectItemCaseSensitive(title_obj, "runs");
                if (runs && cJSON_IsArray(runs) && cJSON_GetArraySize(runs) > 0)
                {
                    cJSON *run0 = cJSON_GetArrayItem(runs, 0);
                    if (run0) { json_str_copy(video->title, sizeof(video->title), json_cjson_str_get(run0, "text")); }
                }
            }

            cJSON *lines = cJSON_GetObjectItemCaseSensitive(tmr, "lines");
            if (lines && cJSON_IsArray(lines))
            {
                uint32_t line_cunt = (uint32_t)cJSON_GetArraySize(lines);
                uint32_t li = 0;

                for (; li < line_cunt; li++)
                {
                    cJSON *line = cJSON_GetArrayItem(lines, (int)li);
                    cJSON *lr = cJSON_GetObjectItemCaseSensitive(line, "lineRenderer");
                    if (!lr) { continue; }

                    cJSON *items = cJSON_GetObjectItemCaseSensitive(lr, "items");
                    if (!items || !cJSON_IsArray(items)) { continue; }

                    cJSON *item0 = cJSON_GetArrayItem(items, 0);
                    if (!item0) { continue; }

                    cJSON *lir = cJSON_GetObjectItemCaseSensitive(item0, "lineItemRenderer");
                    if (!lir) { continue; }

                    cJSON *text_obj = cJSON_GetObjectItemCaseSensitive(lir, "text");
                    if (!text_obj) { continue; }

                    const char *text = json_cjson_str_get(text_obj, "simpleText");
                    if (!text)
                    {
                        cJSON *runs = cJSON_GetObjectItemCaseSensitive(text_obj, "runs");
                        if (runs && cJSON_IsArray(runs) && cJSON_GetArraySize(runs) > 0)
                        {
                            cJSON *r0 = cJSON_GetArrayItem(runs, 0);
                            if (r0) { text = json_cjson_str_get(r0, "text"); }
                        }
                    }

                    if (!text) { continue; }

                    if (li == 0 && video->channel[0] == '\0') { json_str_copy(video->channel, sizeof(video->channel), text); }
                    else if (strstr(text, "view")) { video->view_cunt = json_view_cunt_parse(text); }
                }
            }
        }
    }

    // header.tileHeaderRenderer.thumbnail
    cJSON *hdr = cJSON_GetObjectItemCaseSensitive(tile, "header");
    if (hdr)
    {
        cJSON *thr = cJSON_GetObjectItemCaseSensitive(hdr, "tileHeaderRenderer");
        if (thr)
        {
            cJSON *thumb = cJSON_GetObjectItemCaseSensitive(thr, "thumbnail");
            if (thumb)
            {
                cJSON *thumbs = cJSON_GetObjectItemCaseSensitive(thumb, "thumbnails");
                if (thumbs && cJSON_IsArray(thumbs))
                {
                    uint32_t tc = (uint32_t)cJSON_GetArraySize(thumbs);
                    if (tc > 0)
                    {
                        cJSON *last = cJSON_GetArrayItem(thumbs, (int)(tc - 1));
                        if (last) { json_str_copy(video->thumb_url, sizeof(video->thumb_url), json_cjson_str_get(last, "url")); }
                    }
                }
            }

            cJSON *overlays = cJSON_GetObjectItemCaseSensitive(thr, "thumbnailOverlays");
            if (overlays && cJSON_IsArray(overlays))
            {
                uint32_t oi = 0;
                uint32_t oc = (uint32_t)cJSON_GetArraySize(overlays);

                for (; oi < oc; oi++)
                {
                    cJSON *ov = cJSON_GetArrayItem(overlays, (int)oi);
                    cJSON *tor = cJSON_GetObjectItemCaseSensitive(ov, "thumbnailOverlayTimeStatusRenderer");
                    if (!tor) { continue; }

                    cJSON *txt = cJSON_GetObjectItemCaseSensitive(tor, "text");
                    if (txt) { json_str_copy(video->duration_str, sizeof(video->duration_str), json_cjson_str_get(txt, "simpleText")); }
                }
            }
        }
    }
}

// web video renderer
static void json_video_parse(cJSON *renderer, yt_video_t *video)
{
    memset(video, 0, sizeof(*video));

    json_str_copy(video->id, sizeof(video->id), json_cjson_str_get(renderer, "videoId"));

    cJSON *title = cJSON_GetObjectItemCaseSensitive(renderer, "title");
    if (title)
    {
        cJSON *runs = cJSON_GetObjectItemCaseSensitive(title, "runs");
        if (runs && cJSON_IsArray(runs) && cJSON_GetArraySize(runs) > 0)
        {
            cJSON *r0 = cJSON_GetArrayItem(runs, 0);
            if (r0) { json_str_copy(video->title, sizeof(video->title), json_cjson_str_get(r0, "text")); }
        }
    }

    const char *chan = 0x0;
    cJSON *owner = cJSON_GetObjectItemCaseSensitive(renderer, "ownerText");
    if (owner)
    {
        cJSON *runs = cJSON_GetObjectItemCaseSensitive(owner, "runs");
        if (runs && cJSON_IsArray(runs) && cJSON_GetArraySize(runs) > 0)
        {
            cJSON *r0 = cJSON_GetArrayItem(runs, 0);
            if (r0) { chan = json_cjson_str_get(r0, "text"); }
        }
    }

    if (!chan)
    {
        cJSON *byline = cJSON_GetObjectItemCaseSensitive(renderer, "longBylineText");
        if (!byline) { byline = cJSON_GetObjectItemCaseSensitive(renderer, "shortBylineText"); }

        if (byline)
        {
            cJSON *runs = cJSON_GetObjectItemCaseSensitive(byline, "runs");
            if (runs && cJSON_IsArray(runs) && cJSON_GetArraySize(runs) > 0)
            {
                cJSON *r0 = cJSON_GetArrayItem(runs, 0);
                if (r0) { chan = json_cjson_str_get(r0, "text"); }
            }
        }
    }

    json_str_copy(video->channel, sizeof(video->channel), chan);

    cJSON *thumb = cJSON_GetObjectItemCaseSensitive(renderer, "thumbnail");
    if (thumb)
    {
        cJSON *thumbs = cJSON_GetObjectItemCaseSensitive(thumb, "thumbnails");
        if (thumbs && cJSON_IsArray(thumbs))
        {
            uint32_t tc = (uint32_t)cJSON_GetArraySize(thumbs);
            if (tc > 0)
            {
                cJSON *last = cJSON_GetArrayItem(thumbs, (int)(tc - 1));
                if (last) { json_str_copy(video->thumb_url, sizeof(video->thumb_url), json_cjson_str_get(last, "url")); }
            }
        }
    }

    cJSON *len_text = cJSON_GetObjectItemCaseSensitive(renderer, "lengthText");
    if (len_text) { json_str_copy(video->duration_str, sizeof(video->duration_str), json_cjson_str_get(len_text, "simpleText")); }

    cJSON *view_text = cJSON_GetObjectItemCaseSensitive(renderer, "viewCountText");
    if (view_text)
    {
        const char *vt = json_cjson_str_get(view_text, "simpleText");
        if (vt) { video->view_cunt = json_view_cunt_parse(vt); }
    }
}

// feed
uint32_t yt_json_feed_parse(const char *data, size_t len, yt_feed_t *feed)
{
    if (LDG_UNLIKELY(!data)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!feed)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(feed, 0, sizeof(*feed));

    json_syslog_full_dump("feed_parse_raw", data, len);

    cJSON *root = cJSON_ParseWithLength(data, len);
    if (LDG_UNLIKELY(!root))
    {
        syslog(LOG_ERR, "feed_parse; cjson parse failed; len: %zu; err: %s", len, cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown");
        return YT_ERR_API_PARSE;
    }

    cJSON *err_obj = cJSON_GetObjectItemCaseSensitive(root, "error");
    if (err_obj)
    {
        cJSON *msg = cJSON_GetObjectItemCaseSensitive(err_obj, "message");
        cJSON *code = cJSON_GetObjectItemCaseSensitive(err_obj, "code");
        syslog(LOG_ERR, "feed_parse; api error; code: %d; msg: %s", code ? code->valueint : 0, (msg && cJSON_IsString(msg)) ? msg->valuestring : "unknown");
        cJSON_Delete(root);
        return YT_ERR_API_REQ;
    }

    cJSON *contents = cJSON_GetObjectItemCaseSensitive(root, "contents");
    if (!contents)
    {
        syslog(LOG_ERR, "%s", "feed_parse; no contents key in root");
        cJSON_Delete(root);
        return YT_ERR_API_PARSE;
    }

    // tv path
    cJSON *tv_browse = cJSON_GetObjectItemCaseSensitive(contents, "tvBrowseRenderer");
    if (tv_browse)
    {
        syslog(LOG_DEBUG, "%s", "feed_parse; using TV path");

        cJSON *tv_content = cJSON_GetObjectItemCaseSensitive(tv_browse, "content");
        cJSON *surface = tv_content ? cJSON_GetObjectItemCaseSensitive(tv_content, "tvSurfaceContentRenderer") : 0x0;
        cJSON *surface_content = surface ? cJSON_GetObjectItemCaseSensitive(surface, "content") : 0x0;
        cJSON *section_list = surface_content ? cJSON_GetObjectItemCaseSensitive(surface_content, "sectionListRenderer") : 0x0;
        cJSON *sections = section_list ? cJSON_GetObjectItemCaseSensitive(section_list, "contents") : 0x0;

        if (!sections || !cJSON_IsArray(sections))
        {
            syslog(LOG_ERR, "%s", "feed_parse; TV sectionListRenderer.contents not found");
            cJSON_Delete(root);
            return YT_ERR_API_PARSE;
        }

        uint32_t sec_cunt = (uint32_t)cJSON_GetArraySize(sections);
        syslog(LOG_DEBUG, "feed_parse; TV sections: %u", sec_cunt);
        uint32_t si = 0;

        for (; si < sec_cunt && feed->video_cunt < YT_FEED_MAX_VIDEOS; si++)
        {
            cJSON *section = cJSON_GetArrayItem(sections, (int)si);
            cJSON *shelf = cJSON_GetObjectItemCaseSensitive(section, "shelfRenderer");
            if (!shelf) { continue; }

            cJSON *shelf_content = cJSON_GetObjectItemCaseSensitive(shelf, "content");
            if (!shelf_content) { continue; }

            cJSON *hlist = cJSON_GetObjectItemCaseSensitive(shelf_content, "horizontalListRenderer");
            if (!hlist) { continue; }

            cJSON *items = cJSON_GetObjectItemCaseSensitive(hlist, "items");
            if (!items || !cJSON_IsArray(items)) { continue; }

            uint32_t item_cunt = (uint32_t)cJSON_GetArraySize(items);
            uint32_t ii = 0;

            for (; ii < item_cunt && feed->video_cunt < YT_FEED_MAX_VIDEOS; ii++)
            {
                cJSON *item = cJSON_GetArrayItem(items, (int)ii);
                cJSON *tile = cJSON_GetObjectItemCaseSensitive(item, "tileRenderer");
                if (!tile) { continue; }

                json_tile_parse(tile, &feed->videos[feed->video_cunt]);

                if (feed->videos[feed->video_cunt].id[0] != '\0') { feed->video_cunt++; }
            }
        }

        syslog(LOG_DEBUG, "feed_parse; TV parsed %u videos", feed->video_cunt);
        cJSON_Delete(root);
        return LDG_ERR_AOK;
    }

    // web path
    cJSON *two_col = cJSON_GetObjectItemCaseSensitive(contents, "twoColumnBrowseResultsRenderer");
    if (two_col)
    {
        syslog(LOG_DEBUG, "%s", "feed_parse; using WEB path");

        cJSON *tabs = cJSON_GetObjectItemCaseSensitive(two_col, "tabs");
        cJSON *tab0 = (tabs && cJSON_IsArray(tabs) && cJSON_GetArraySize(tabs) > 0) ? cJSON_GetArrayItem(tabs, 0) : 0x0;
        cJSON *tab_renderer = tab0 ? cJSON_GetObjectItemCaseSensitive(tab0, "tabRenderer") : 0x0;
        cJSON *tab_content = tab_renderer ? cJSON_GetObjectItemCaseSensitive(tab_renderer, "content") : 0x0;
        cJSON *rich_grid = tab_content ? cJSON_GetObjectItemCaseSensitive(tab_content, "richGridRenderer") : 0x0;
        cJSON *grid_contents = rich_grid ? cJSON_GetObjectItemCaseSensitive(rich_grid, "contents") : 0x0;

        if (!grid_contents || !cJSON_IsArray(grid_contents))
        {
            syslog(LOG_ERR, "%s", "feed_parse; WEB richGridRenderer.contents not found");
            cJSON_Delete(root);
            return YT_ERR_API_PARSE;
        }

        uint32_t arr_len = (uint32_t)cJSON_GetArraySize(grid_contents);
        uint32_t idx = 0;

        for (; idx < arr_len && feed->video_cunt < YT_FEED_MAX_VIDEOS; idx++)
        {
            cJSON *item = cJSON_GetArrayItem(grid_contents, (int)idx);
            cJSON *rich = cJSON_GetObjectItemCaseSensitive(item, "richItemRenderer");
            cJSON *content = rich ? cJSON_GetObjectItemCaseSensitive(rich, "content") : 0x0;
            cJSON *renderer = content ? cJSON_GetObjectItemCaseSensitive(content, "videoRenderer") : 0x0;
            if (!renderer) { continue; }

            json_video_parse(renderer, &feed->videos[feed->video_cunt]);

            if (feed->videos[feed->video_cunt].id[0] != '\0') { feed->video_cunt++; }
        }

        syslog(LOG_DEBUG, "feed_parse; WEB parsed %u videos", feed->video_cunt);
        cJSON_Delete(root);
        return LDG_ERR_AOK;
    }

    syslog(LOG_ERR, "%s", "feed_parse; unknown response format");
    cJSON_Delete(root);
    return YT_ERR_API_PARSE;
}

// search
uint32_t yt_json_search_parse(const char *data, size_t len, yt_feed_t *feed)
{
    if (LDG_UNLIKELY(!data)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!feed)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(feed, 0, sizeof(*feed));

    cJSON *root = cJSON_ParseWithLength(data, len);
    if (LDG_UNLIKELY(!root)) { return YT_ERR_API_PARSE; }

    cJSON *contents = cJSON_GetObjectItemCaseSensitive(root, "contents");
    if (!contents)
    {
        cJSON_Delete(root);
        return YT_ERR_API_PARSE;
    }

    // web search
    cJSON *two_col = cJSON_GetObjectItemCaseSensitive(contents, "twoColumnSearchResultsRenderer");
    if (two_col)
    {
        cJSON *primary = cJSON_GetObjectItemCaseSensitive(two_col, "primaryContents");
        cJSON *section_list = primary ? cJSON_GetObjectItemCaseSensitive(primary, "sectionListRenderer") : 0x0;
        cJSON *sections = section_list ? cJSON_GetObjectItemCaseSensitive(section_list, "contents") : 0x0;

        if (sections && cJSON_IsArray(sections) && cJSON_GetArraySize(sections) > 0)
        {
            cJSON *sec0 = cJSON_GetArrayItem(sections, 0);
            cJSON *item_section = sec0 ? cJSON_GetObjectItemCaseSensitive(sec0, "itemSectionRenderer") : 0x0;
            cJSON *items = item_section ? cJSON_GetObjectItemCaseSensitive(item_section, "contents") : 0x0;

            if (items && cJSON_IsArray(items))
            {
                uint32_t arr_len = (uint32_t)cJSON_GetArraySize(items);
                uint32_t idx = 0;

                for (; idx < arr_len && feed->video_cunt < YT_FEED_MAX_VIDEOS; idx++)
                {
                    cJSON *item = cJSON_GetArrayItem(items, (int)idx);
                    cJSON *renderer = cJSON_GetObjectItemCaseSensitive(item, "videoRenderer");
                    if (!renderer) { continue; }

                    json_video_parse(renderer, &feed->videos[feed->video_cunt]);

                    if (feed->videos[feed->video_cunt].id[0] != '\0') { feed->video_cunt++; }
                }
            }
        }

        cJSON_Delete(root);
        return LDG_ERR_AOK;
    }

    // tv search
    cJSON *section_list = cJSON_GetObjectItemCaseSensitive(contents, "sectionListRenderer");
    if (section_list)
    {
        cJSON *sections = cJSON_GetObjectItemCaseSensitive(section_list, "contents");
        if (sections && cJSON_IsArray(sections))
        {
            uint32_t sec_cunt = (uint32_t)cJSON_GetArraySize(sections);
            uint32_t si = 0;

            for (; si < sec_cunt && feed->video_cunt < YT_FEED_MAX_VIDEOS; si++)
            {
                cJSON *section = cJSON_GetArrayItem(sections, (int)si);

                cJSON *shelf = cJSON_GetObjectItemCaseSensitive(section, "shelfRenderer");
                if (shelf)
                {
                    cJSON *shelf_content = cJSON_GetObjectItemCaseSensitive(shelf, "content");
                    cJSON *hlist = shelf_content ? cJSON_GetObjectItemCaseSensitive(shelf_content, "horizontalListRenderer") : 0x0;
                    cJSON *items = hlist ? cJSON_GetObjectItemCaseSensitive(hlist, "items") : 0x0;

                    if (items && cJSON_IsArray(items))
                    {
                        uint32_t ii = 0;
                        uint32_t ic = (uint32_t)cJSON_GetArraySize(items);

                        for (; ii < ic && feed->video_cunt < YT_FEED_MAX_VIDEOS; ii++)
                        {
                            cJSON *item = cJSON_GetArrayItem(items, (int)ii);
                            cJSON *tile = cJSON_GetObjectItemCaseSensitive(item, "tileRenderer");
                            if (!tile) { continue; }

                            json_tile_parse(tile, &feed->videos[feed->video_cunt]);

                            if (feed->videos[feed->video_cunt].id[0] != '\0') { feed->video_cunt++; }
                        }
                    }

                    continue;
                }

                cJSON *item_sec = cJSON_GetObjectItemCaseSensitive(section, "itemSectionRenderer");
                if (item_sec)
                {
                    cJSON *items = cJSON_GetObjectItemCaseSensitive(item_sec, "contents");
                    if (items && cJSON_IsArray(items))
                    {
                        uint32_t ii = 0;
                        uint32_t ic = (uint32_t)cJSON_GetArraySize(items);

                        for (; ii < ic && feed->video_cunt < YT_FEED_MAX_VIDEOS; ii++)
                        {
                            cJSON *item = cJSON_GetArrayItem(items, (int)ii);
                            cJSON *renderer = cJSON_GetObjectItemCaseSensitive(item, "videoRenderer");
                            if (!renderer)
                            {
                                cJSON *tile = cJSON_GetObjectItemCaseSensitive(item, "tileRenderer");
                                if (!tile) { continue; }

                                json_tile_parse(tile, &feed->videos[feed->video_cunt]);

                                if (feed->videos[feed->video_cunt].id[0] != '\0') { feed->video_cunt++; }

                                continue;
                            }

                            json_video_parse(renderer, &feed->videos[feed->video_cunt]);

                            if (feed->videos[feed->video_cunt].id[0] != '\0') { feed->video_cunt++; }
                        }
                    }
                }
            }
        }

        cJSON_Delete(root);
        return LDG_ERR_AOK;
    }

    cJSON_Delete(root);
    return YT_ERR_API_PARSE;
}

// player
static void json_stream_parse(cJSON *fmt, yt_stream_t *stream)
{
    memset(stream, 0, sizeof(*stream));

    json_str_copy(stream->url, sizeof(stream->url), json_cjson_str_get(fmt, "url"));
    json_str_copy(stream->mime, sizeof(stream->mime), json_cjson_str_get(fmt, "mimeType"));

    if (stream->mime[0] != '\0' && strncmp(stream->mime, "audio/", 6) == 0) { stream->is_audio = 1; }

    cJSON *tmp = 0x0;

    tmp = cJSON_GetObjectItemCaseSensitive(fmt, "itag");
    if (tmp && cJSON_IsNumber(tmp)) { stream->itag = (uint32_t)tmp->valueint; }

    tmp = cJSON_GetObjectItemCaseSensitive(fmt, "width");
    if (tmp && cJSON_IsNumber(tmp)) { stream->width = (uint32_t)tmp->valueint; }

    tmp = cJSON_GetObjectItemCaseSensitive(fmt, "height");
    if (tmp && cJSON_IsNumber(tmp)) { stream->height = (uint32_t)tmp->valueint; }

    tmp = cJSON_GetObjectItemCaseSensitive(fmt, "bitrate");
    if (tmp && cJSON_IsNumber(tmp)) { stream->bitrate = (uint32_t)tmp->valueint; }
}

uint32_t yt_json_player_parse(const char *data, size_t len, yt_stream_set_t *streams)
{
    if (LDG_UNLIKELY(!data)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!streams)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(streams, 0, sizeof(*streams));

    cJSON *root = cJSON_ParseWithLength(data, len);
    if (LDG_UNLIKELY(!root)) { return YT_ERR_API_PARSE; }

    cJSON *streaming = cJSON_GetObjectItemCaseSensitive(root, "streamingData");
    if (!streaming)
    {
        cJSON_Delete(root);
        return YT_ERR_API_PARSE;
    }

    cJSON *formats = cJSON_GetObjectItemCaseSensitive(streaming, "formats");
    if (formats && cJSON_IsArray(formats))
    {
        uint32_t fmt_cunt = (uint32_t)cJSON_GetArraySize(formats);
        uint32_t idx = 0;

        for (; idx < fmt_cunt && streams->stream_cunt < YT_STREAM_MAX; idx++)
        {
            cJSON *fmt = cJSON_GetArrayItem(formats, (int)idx);
            json_stream_parse(fmt, &streams->streams[streams->stream_cunt]);
            if (streams->streams[streams->stream_cunt].url[0] != '\0') { streams->stream_cunt++; }
        }
    }

    cJSON *adaptive = cJSON_GetObjectItemCaseSensitive(streaming, "adaptiveFormats");
    if (adaptive && cJSON_IsArray(adaptive))
    {
        uint32_t adp_cunt = (uint32_t)cJSON_GetArraySize(adaptive);
        uint32_t idx = 0;

        for (; idx < adp_cunt && streams->stream_cunt < YT_STREAM_MAX; idx++)
        {
            cJSON *fmt = cJSON_GetArrayItem(adaptive, (int)idx);
            json_stream_parse(fmt, &streams->streams[streams->stream_cunt]);
            if (streams->streams[streams->stream_cunt].url[0] != '\0') { streams->stream_cunt++; }
        }
    }

    cJSON_Delete(root);

    if (streams->stream_cunt == 0) { return YT_ERR_API_PARSE; }

    return LDG_ERR_AOK;
}

// token
uint32_t yt_json_token_parse(const char *data, size_t len, yt_token_t *token)
{
    if (LDG_UNLIKELY(!data)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!token)) { return LDG_ERR_FUNC_ARG_NULL; }

    cJSON *root = cJSON_ParseWithLength(data, len);
    if (LDG_UNLIKELY(!root)) { return YT_ERR_API_PARSE; }

    const char *at = json_cjson_str_get(root, "access_token");
    if (!at)
    {
        cJSON_Delete(root);
        return YT_ERR_AUTH_TOKEN_INVALID;
    }

    json_str_copy(token->access, sizeof(token->access), at);

    const char *rt = json_cjson_str_get(root, "refresh_token");
    if (rt) { json_str_copy(token->refresh, sizeof(token->refresh), rt); }

    cJSON *exp = cJSON_GetObjectItemCaseSensitive(root, "expires_in");
    if (exp && cJSON_IsNumber(exp)) { token->expiry_epoch = (uint64_t)time(0x0) + (uint64_t)exp->valueint; }

    cJSON_Delete(root);
    return LDG_ERR_AOK;
}

// device code
uint32_t yt_json_device_code_parse(const char *data, size_t len, char *device_code, size_t dc_len, char *user_code, size_t uc_len, char *verify_url, size_t vu_len, uint32_t *interval, uint32_t *expires_in)
{
    if (LDG_UNLIKELY(!data)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!device_code)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!user_code)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!verify_url)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!interval)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!expires_in)) { return LDG_ERR_FUNC_ARG_NULL; }

    cJSON *root = cJSON_ParseWithLength(data, len);
    if (LDG_UNLIKELY(!root)) { return YT_ERR_API_PARSE; }

    const char *dc = json_cjson_str_get(root, "device_code");
    if (!dc)
    {
        cJSON_Delete(root);
        return YT_ERR_AUTH_DEVICE_CODE;
    }

    json_str_copy(device_code, dc_len, dc);

    const char *uc = json_cjson_str_get(root, "user_code");
    if (uc) { json_str_copy(user_code, uc_len, uc); }

    const char *vu = json_cjson_str_get(root, "verification_url");
    if (vu) { json_str_copy(verify_url, vu_len, vu); }

    cJSON *iv = cJSON_GetObjectItemCaseSensitive(root, "interval");
    if (iv && cJSON_IsNumber(iv)) { *interval = (uint32_t)iv->valueint; }
    else { *interval = 5; }

    cJSON *ei = cJSON_GetObjectItemCaseSensitive(root, "expires_in");
    if (ei && cJSON_IsNumber(ei)) { *expires_in = (uint32_t)ei->valueint; }
    else { *expires_in = 300; }

    cJSON_Delete(root);
    return LDG_ERR_AOK;
}
