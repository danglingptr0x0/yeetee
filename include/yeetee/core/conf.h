#ifndef YT_CORE_CONF_H
#define YT_CORE_CONF_H

#include <stdint.h>
#include <stddef.h>

#define YT_CONF_CLIENT_ID_MAX 256
#define YT_CONF_CLIENT_SECRET_MAX 256
#define YT_CONF_DATA_DIR_MAX 512
#define YT_CONF_CONF_DIR_MAX 512
#define YT_CONF_CACHE_DIR_MAX 512

#define YT_CONF_DEFAULT_THUMB_CACHE_MAX 128
#define YT_CONF_DEFAULT_POOL_WORKERS 4

typedef struct yt_conf
{
    char client_id[YT_CONF_CLIENT_ID_MAX];
    char client_secret[YT_CONF_CLIENT_SECRET_MAX];
    char data_dir[YT_CONF_DATA_DIR_MAX];
    char conf_dir[YT_CONF_CONF_DIR_MAX];
    char cache_dir[YT_CONF_CACHE_DIR_MAX];
    uint32_t thumb_cache_max;
    uint32_t pool_workers;
} yt_conf_t;

uint32_t yt_conf_init(yt_conf_t *conf);
uint32_t yt_conf_load(yt_conf_t *conf);

#endif
