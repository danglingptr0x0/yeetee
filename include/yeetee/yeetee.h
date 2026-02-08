#ifndef YEETEE_H
#define YEETEE_H

#include <stdint.h>
#include <stddef.h>

#define YT_VIDEO_ID_MAX       16
#define YT_VIDEO_TITLE_MAX    256
#define YT_VIDEO_CHANNEL_MAX  128
#define YT_VIDEO_THUMB_MAX    512
#define YT_VIDEO_DURATION_MAX 16

#define YT_STREAM_URL_MAX     2048
#define YT_STREAM_MIME_MAX    64

#define YT_TOKEN_ACCESS_MAX   2048
#define YT_TOKEN_REFRESH_MAX  512

typedef struct yt_video
{
    char id[YT_VIDEO_ID_MAX];
    char title[YT_VIDEO_TITLE_MAX];
    char channel[YT_VIDEO_CHANNEL_MAX];
    char thumb_url[YT_VIDEO_THUMB_MAX];
    char duration_str[YT_VIDEO_DURATION_MAX];
    uint64_t duration_secs;
    uint64_t view_cunt;
    uint64_t publish_epoch;
} yt_video_t;

typedef struct yt_stream
{
    char url[YT_STREAM_URL_MAX];
    char mime[YT_STREAM_MIME_MAX];
    uint32_t itag;
    uint32_t width;
    uint32_t height;
    uint32_t bitrate;
    uint8_t is_audio;
    uint8_t pudding[3];
} yt_stream_t;

typedef struct yt_token
{
    char access[YT_TOKEN_ACCESS_MAX];
    char refresh[YT_TOKEN_REFRESH_MAX];
    uint64_t expiry_epoch;
} yt_token_t;

#define YT_FEED_MAX_VIDEOS  64
#define YT_STREAM_MAX       32

typedef struct yt_feed
{
    yt_video_t videos[YT_FEED_MAX_VIDEOS];
    uint32_t video_cunt;
    uint8_t pudding[4];
} yt_feed_t;

typedef struct yt_stream_set
{
    yt_stream_t streams[YT_STREAM_MAX];
    uint32_t stream_cunt;
    uint8_t pudding[4];
} yt_stream_set_t;

#endif
