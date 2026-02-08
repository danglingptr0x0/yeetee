#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

#include <dangling/core/macros.h>
#include <dangling/core/err.h>
#include <dangling/net/curl.h>

#include <yeetee/yeetee.h>
#include <yeetee/core/err.h>
#include <yeetee/auth/token.h>
#include <yeetee/api/json.h>

#define TOKEN_FILENAME "token"
#define TOKEN_PATH_MAX 1024
#define TOKEN_BUFF_MAX 4096
#define TOKEN_REFRESH_URL "https://oauth2.googleapis.com/token"
#define TOKEN_BODY_MAX 2048

// token

static uint32_t token_path_build(char *buff, size_t buff_len, const char *data_dir)
{
    int written = 0;

    written = snprintf(buff, buff_len, "%s/%s", data_dir, TOKEN_FILENAME);
    if (LDG_UNLIKELY(written < 0 || (size_t)written >= buff_len)) { return LDG_ERR_OVERFLOW; }

    return LDG_ERR_AOK;
}

uint32_t yt_token_store(const yt_token_t *token, const char *data_dir)
{
    if (LDG_UNLIKELY(!token)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!data_dir)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char path[TOKEN_PATH_MAX] = LDG_ARR_ZERO_INIT;
    char buff[TOKEN_BUFF_MAX] = LDG_ARR_ZERO_INIT;
    int32_t fd = 0;
    int32_t written = 0;
    ssize_t wr_ret = 0;

    mkdir(data_dir, 0700);

    ret = token_path_build(path, sizeof(path), data_dir);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (LDG_UNLIKELY(fd < 0)) { return YT_ERR_AUTH_STORE; }

    written = snprintf(buff, sizeof(buff), "access=%s\nrefresh=%s\nexpiry=%lu\n", token->access, token->refresh, (unsigned long)token->expiry_epoch);
    if (LDG_UNLIKELY(written < 0 || (uint32_t)written >= sizeof(buff)))
    {
        close(fd);
        return LDG_ERR_OVERFLOW;
    }

    wr_ret = write(fd, buff, (size_t)written);
    close(fd);

    if (LDG_UNLIKELY(wr_ret != written)) { return LDG_ERR_IO_WRITE; }

    return LDG_ERR_AOK;
}

static uint32_t token_line_parse(const char *line, yt_token_t *token)
{
    if (strncmp(line, "access=", 7) == 0)
    {
        snprintf(token->access, sizeof(token->access), "%s", line + 7);
        return LDG_ERR_AOK;
    }

    if (strncmp(line, "refresh=", 8) == 0)
    {
        snprintf(token->refresh, sizeof(token->refresh), "%s", line + 8);
        return LDG_ERR_AOK;
    }

    if (strncmp(line, "expiry=", 7) == 0)
    {
        token->expiry_epoch = 0;
        const char *val = line + 7;
        while (*val >= '0' && *val <= '9')
        {
            token->expiry_epoch = token->expiry_epoch * 10 + (uint64_t)(*val - '0');
            val++;
        }
        return LDG_ERR_AOK;
    }

    return LDG_ERR_AOK;
}

uint32_t yt_token_load(yt_token_t *token, const char *data_dir)
{
    if (LDG_UNLIKELY(!token)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!data_dir)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char path[TOKEN_PATH_MAX] = LDG_ARR_ZERO_INIT;
    char buff[TOKEN_BUFF_MAX] = LDG_ARR_ZERO_INIT;
    int32_t fd = 0;
    ssize_t rd_ret = 0;

    ret = token_path_build(path, sizeof(path), data_dir);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    fd = open(path, O_RDONLY);
    if (LDG_UNLIKELY(fd < 0)) { return YT_ERR_AUTH_LOAD; }

    rd_ret = read(fd, buff, sizeof(buff) - 1);
    close(fd);

    if (LDG_UNLIKELY(rd_ret <= 0)) { return LDG_ERR_IO_READ; }

    buff[rd_ret] = '\0';

    memset(token, 0, sizeof(*token));

    char *line = buff;
    char *nl = 0x0;
    while ((nl = strchr(line, '\n')) != 0x0)
    {
        *nl = '\0';
        token_line_parse(line, token);
        line = nl + 1;
    }
    if (*line != '\0') { token_line_parse(line, token); }

    return LDG_ERR_AOK;
}

uint32_t yt_token_refresh(ldg_curl_easy_ctx_t *curl, const char *client_id, const char *client_secret, yt_token_t *token)
{
    if (LDG_UNLIKELY(!curl)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!client_id)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!client_secret)) { return LDG_ERR_FUNC_ARG_NULL; }

    if (LDG_UNLIKELY(!token)) { return LDG_ERR_FUNC_ARG_NULL; }

    uint32_t ret = LDG_ERR_AOK;
    char body[TOKEN_BODY_MAX] = LDG_ARR_ZERO_INIT;
    struct curl_slist *headers = 0x0;
    ldg_curl_resp_t http_resp = LDG_STRUCT_ZERO_INIT;
    yt_token_t refreshed = LDG_STRUCT_ZERO_INIT;

    ldg_curl_resp_init(&http_resp);

    snprintf(body, sizeof(body), "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token", client_id, client_secret, token->refresh);

    ret = ldg_curl_headers_append(&headers, "Content-Type: application/x-www-form-urlencoded");
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return ret; }

    ret = ldg_curl_easy_post(curl, TOKEN_REFRESH_URL, body, headers, &http_resp);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        ldg_curl_headers_destroy(&headers);
        ldg_curl_resp_free(&http_resp);
        return YT_ERR_AUTH_REFRESH;
    }

    ret = yt_json_token_parse(http_resp.data, http_resp.size, &refreshed);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK))
    {
        ldg_curl_headers_destroy(&headers);
        ldg_curl_resp_free(&http_resp);
        return YT_ERR_AUTH_REFRESH;
    }

    snprintf(token->access, sizeof(token->access), "%s", refreshed.access);
    token->expiry_epoch = refreshed.expiry_epoch;

    if (refreshed.refresh[0] != '\0') { snprintf(token->refresh, sizeof(token->refresh), "%s", refreshed.refresh); }

    ldg_curl_headers_destroy(&headers);
    ldg_curl_resp_free(&http_resp);
    return LDG_ERR_AOK;
}

uint32_t yt_token_expired_is(const yt_token_t *token)
{
    if (LDG_UNLIKELY(!token)) { return LDG_ERR_FUNC_ARG_NULL; }

    time_t now = time(0x0);
    return (uint64_t)now >= token->expiry_epoch;
}
