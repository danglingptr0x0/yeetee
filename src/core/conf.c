#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <dangling/core/err.h>
#include <dangling/core/macros.h>
#include <yeetee/core/conf.h>

// conf
#define YT_CONF_LINE_MAX 1024
#define YT_CONF_READ_BUFF_SIZE 4096
#define YT_CONF_FILENAME "config"
#define YT_CONF_SUBDIR "/yeetee"

#define YT_CONF_DEFAULT_CLIENT_ID "861556708454-d6dlm3lh05idd8npek18k6be8ba3oc68.apps.googleusercontent.com"
#define YT_CONF_DEFAULT_CLIENT_SECRET "SboVhoG9s0rNafixCSGGKXAT"

static uint32_t conf_dir_build(char *dst, size_t dst_size, const char *env_name, const char *fallback_suffix)
{
    const char *env_val = getenv(env_name);
    const char *home = getenv("HOME");

    if (LDG_UNLIKELY(!home)) { return LDG_ERR_NOT_FOUND; }

    if (env_val)
    {
        size_t len = strlen(env_val) + strlen(YT_CONF_SUBDIR);
        if (LDG_UNLIKELY(len >= dst_size)) { return LDG_ERR_STR_TRUNC; }

        memcpy(dst, env_val, strlen(env_val));
        memcpy(dst + strlen(env_val), YT_CONF_SUBDIR, strlen(YT_CONF_SUBDIR) + LDG_STR_TERM_SIZE);
    }
    else
    {
        size_t home_len = strlen(home);
        size_t suffix_len = strlen(fallback_suffix);
        size_t subdir_len = strlen(YT_CONF_SUBDIR);
        size_t total = home_len + suffix_len + subdir_len;
        if (LDG_UNLIKELY(total >= dst_size)) { return LDG_ERR_STR_TRUNC; }

        memcpy(dst, home, home_len);
        memcpy(dst + home_len, fallback_suffix, suffix_len);
        memcpy(dst + home_len + suffix_len, YT_CONF_SUBDIR, subdir_len + LDG_STR_TERM_SIZE);
    }

    return LDG_ERR_AOK;
}

static uint32_t conf_line_parse(yt_conf_t *conf, const char *key, size_t key_len, const char *val, size_t val_len)
{
    if (key_len == 9 && memcmp(key, "client_id", 9) == 0)
    {
        if (LDG_UNLIKELY(val_len >= YT_CONF_CLIENT_ID_MAX)) { return LDG_ERR_STR_TRUNC; }

        memcpy(conf->client_id, val, val_len);
        conf->client_id[val_len] = LDG_STR_TERM;
    }
    else if (key_len == 13 && memcmp(key, "client_secret", 13) == 0)
    {
        if (LDG_UNLIKELY(val_len >= YT_CONF_CLIENT_SECRET_MAX)) { return LDG_ERR_STR_TRUNC; }

        memcpy(conf->client_secret, val, val_len);
        conf->client_secret[val_len] = LDG_STR_TERM;
    }
    else if (key_len == 15 && memcmp(key, "thumb_cache_max", 15) == 0)
    {
        uint32_t num = 0;
        for (size_t i = 0; i < val_len; i++)
        {
            if (LDG_UNLIKELY(val[i] < '0' || val[i] > '9')) { return LDG_ERR_FUNC_ARG_INVALID; }

            num = num * LDG_BASE_DECIMAL + (uint32_t)(val[i] - '0');
        }
        conf->thumb_cache_max = num;
    }
    else if (key_len == 12 && memcmp(key, "pool_workers", 12) == 0)
    {
        uint32_t num = 0;
        for (size_t i = 0; i < val_len; i++)
        {
            if (LDG_UNLIKELY(val[i] < '0' || val[i] > '9')) { return LDG_ERR_FUNC_ARG_INVALID; }

            num = num * LDG_BASE_DECIMAL + (uint32_t)(val[i] - '0');
        }
        conf->pool_workers = num;
    }

    return LDG_ERR_AOK;
}

static uint32_t conf_file_parse(yt_conf_t *conf, int fd)
{
    char buff[YT_CONF_READ_BUFF_SIZE] = LDG_ARR_ZERO_INIT;
    char line[YT_CONF_LINE_MAX] = LDG_ARR_ZERO_INIT;
    size_t line_len = 0;
    ssize_t bytes_read = 0;

    for (;;)
    {
        bytes_read = read(fd, buff, YT_CONF_READ_BUFF_SIZE);
        if (bytes_read <= 0) { break; }

        for (ssize_t i = 0; i < bytes_read; i++)
        {
            if (buff[i] == '\n' || buff[i] == '\r')
            {
                if (line_len == 0) { continue; }

                line[line_len] = LDG_STR_TERM;

                const char *eq = NULL;
                for (size_t j = 0; j < line_len; j++) { if (line[j] == '=')
                    {
                        eq = &line[j];
                        break;
                    }
                }

                if (eq)
                {
                    size_t key_len = (size_t)(eq - line);
                    const char *val = eq + 1;
                    size_t val_len = line_len - key_len - 1;
                    uint32_t ret = conf_line_parse(conf, line, key_len, val, val_len);
                    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }
                }

                line_len = 0;
            }
            else
            {
                if (LDG_UNLIKELY(line_len >= YT_CONF_LINE_MAX - 1)) { return LDG_ERR_STR_TRUNC; }

                line[line_len] = buff[i];
                line_len++;
            }
        }
    }

    if (line_len > 0)
    {
        line[line_len] = LDG_STR_TERM;
        const char *eq = NULL;
        for (size_t j = 0; j < line_len; j++) { if (line[j] == '=')
            {
                eq = &line[j];
                break;
            }
        }

        if (eq)
        {
            size_t key_len = (size_t)(eq - line);
            const char *val = eq + 1;
            size_t val_len = line_len - key_len - 1;
            uint32_t ret = conf_line_parse(conf, line, key_len, val, val_len);
            if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }
        }
    }

    return LDG_ERR_AOK;
}

uint32_t yt_conf_init(yt_conf_t *conf)
{
    if (LDG_UNLIKELY(!conf)) { return LDG_ERR_FUNC_ARG_NULL; }

    memset(conf, 0, sizeof(yt_conf_t));

    uint32_t ret = conf_dir_build(conf->conf_dir, YT_CONF_CONF_DIR_MAX, "XDG_CONFIG_HOME", "/.config");
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ret = conf_dir_build(conf->data_dir, YT_CONF_DATA_DIR_MAX, "XDG_DATA_HOME", "/.local/share");
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ret = conf_dir_build(conf->cache_dir, YT_CONF_CACHE_DIR_MAX, "XDG_CACHE_HOME", "/.cache");
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    memcpy(conf->client_id, YT_CONF_DEFAULT_CLIENT_ID, sizeof(YT_CONF_DEFAULT_CLIENT_ID));
    memcpy(conf->client_secret, YT_CONF_DEFAULT_CLIENT_SECRET, sizeof(YT_CONF_DEFAULT_CLIENT_SECRET));
    conf->thumb_cache_max = YT_CONF_DEFAULT_THUMB_CACHE_MAX;
    conf->pool_workers = YT_CONF_DEFAULT_POOL_WORKERS;

    return LDG_ERR_AOK;
}

uint32_t yt_conf_load(yt_conf_t *conf)
{
    if (LDG_UNLIKELY(!conf)) { return LDG_ERR_FUNC_ARG_NULL; }

    mkdir(conf->conf_dir, 0700);
    mkdir(conf->data_dir, 0700);
    mkdir(conf->cache_dir, 0700);

    char path[YT_CONF_CONF_DIR_MAX + sizeof("/" YT_CONF_FILENAME)] = LDG_ARR_ZERO_INIT;
    size_t dir_len = strlen(conf->conf_dir);
    memcpy(path, conf->conf_dir, dir_len);
    path[dir_len] = '/';
    memcpy(path + dir_len + 1, YT_CONF_FILENAME, sizeof(YT_CONF_FILENAME));

    int fd = open(path, O_RDONLY);
    if (fd < 0) { return LDG_ERR_AOK; }

    uint32_t ret = conf_file_parse(conf, fd);
    close(fd);

    return ret;
}
