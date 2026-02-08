#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dangling/core/err.h>
#include <yeetee/yeetee.h>
#include <yeetee/auth/token.h>

static uint32_t tests_run = 0;
static uint32_t tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
            tests_run++; \
            if (!(cond)) { tests_failed++; fprintf(stderr, "FAIL: %s: %s\n", __func__, (msg)); } \
} while (0)

#define TEST_RUN(func) do { func(); } while (0)

static void test_token_store_load(void)
{
    char tmpdir[] = "/tmp/yt_test_auth_XXXXXX";
    char *result = mkdtemp(tmpdir);
    TEST_ASSERT(result != NULL, "mkdtemp failed");
    if (!result) { return; }

    yt_token_t tok_out;
    memset(&tok_out, 0, sizeof(tok_out));
    strncpy(tok_out.access, "test_access_token_12345", YT_TOKEN_ACCESS_MAX - 1);
    strncpy(tok_out.refresh, "test_refresh_token_67890", YT_TOKEN_REFRESH_MAX - 1);
    tok_out.expiry_epoch = 1700000000;

    uint32_t ret = yt_token_store(&tok_out, tmpdir);
    TEST_ASSERT(ret == LDG_ERR_AOK, "yt_token_store failed");

    yt_token_t tok_in;
    memset(&tok_in, 0, sizeof(tok_in));
    ret = yt_token_load(&tok_in, tmpdir);
    TEST_ASSERT(ret == LDG_ERR_AOK, "yt_token_load failed");

    TEST_ASSERT(strcmp(tok_in.access, tok_out.access) == 0, "access token mismatch");
    TEST_ASSERT(strcmp(tok_in.refresh, tok_out.refresh) == 0, "refresh token mismatch");
    TEST_ASSERT(tok_in.expiry_epoch == tok_out.expiry_epoch, "expiry_epoch mismatch");

    char path[512];
    snprintf(path, sizeof(path), "%s/token.json", tmpdir);
    unlink(path);
    rmdir(tmpdir);
}

static void test_token_expired(void)
{
    yt_token_t tok;
    memset(&tok, 0, sizeof(tok));

    tok.expiry_epoch = (uint64_t)time(NULL) - 100;
    uint32_t expired = yt_token_expired_is(&tok);
    TEST_ASSERT(expired == 1, "token should be expired");

    tok.expiry_epoch = (uint64_t)time(NULL) + 100;
    expired = yt_token_expired_is(&tok);
    TEST_ASSERT(expired == 0, "token should not be expired");
}

static void test_token_store_null(void)
{
    uint32_t ret = yt_token_store(NULL, "/tmp");
    TEST_ASSERT(ret != LDG_ERR_AOK, "yt_token_store(NULL) should return error");
}

int main(void)
{
    TEST_RUN(test_token_store_load);
    TEST_RUN(test_token_expired);
    TEST_RUN(test_token_store_null);

    fprintf(stderr, "auth: %u/%u passed\n", tests_run - tests_failed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
