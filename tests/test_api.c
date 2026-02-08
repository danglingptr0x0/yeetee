#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dangling/core/err.h>
#include <cJSON.h>
#include <yeetee/yeetee.h>
#include <yeetee/api/json.h>

static uint32_t tests_run = 0;
static uint32_t tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
            tests_run++; \
            if (!(cond)) { tests_failed++; fprintf(stderr, "FAIL: %s: %s\n", __func__, (msg)); } \
} while (0)

#define TEST_RUN(func) do { func(); } while (0)

static void test_cjson_basic(void)
{
    cJSON *root = cJSON_Parse("{\"outer\":{\"key\":\"found\"}}");
    TEST_ASSERT(root != NULL, "cjson parse failed");

    cJSON *outer = cJSON_GetObjectItemCaseSensitive(root, "outer");
    TEST_ASSERT(outer != NULL, "outer not found");

    cJSON *key = cJSON_GetObjectItemCaseSensitive(outer, "key");
    TEST_ASSERT(key != NULL, "key not found");
    TEST_ASSERT(cJSON_IsString(key), "key not string");
    TEST_ASSERT(strcmp(key->valuestring, "found") == 0, "value mismatch");

    cJSON *missing = cJSON_GetObjectItemCaseSensitive(outer, "missing");
    TEST_ASSERT(missing == NULL, "missing should be NULL");

    cJSON_Delete(root);
}

static void test_json_token_parse(void)
{
    const char *json_str = "{\"access_token\":\"abc123\",\"refresh_token\":\"ref456\",\"expires_in\":3600}";
    size_t len = strlen(json_str);

    yt_token_t tok;
    memset(&tok, 0, sizeof(tok));

    uint32_t ret = yt_json_token_parse(json_str, len, &tok);
    TEST_ASSERT(ret == LDG_ERR_AOK, "token parse failed");
    TEST_ASSERT(strcmp(tok.access, "abc123") == 0, "access_token mismatch");
    TEST_ASSERT(strcmp(tok.refresh, "ref456") == 0, "refresh_token mismatch");
    TEST_ASSERT(tok.expiry_epoch > 0, "expiry_epoch should be set");
}

static void test_json_device_code_parse(void)
{
    const char *json_str = "{\"device_code\":\"dc1\",\"user_code\":\"ABCD-EFGH\",\"verification_url\":\"https://google.com/device\",\"interval\":5,\"expires_in\":1800}";
    size_t len = strlen(json_str);

    char device_code[512];
    char user_code[32];
    char verify_url[256];
    uint32_t interval = 0;
    uint32_t expires_in = 0;

    memset(device_code, 0, sizeof(device_code));
    memset(user_code, 0, sizeof(user_code));
    memset(verify_url, 0, sizeof(verify_url));

    uint32_t ret = yt_json_device_code_parse(json_str, len, device_code, sizeof(device_code), user_code, sizeof(user_code), verify_url, sizeof(verify_url), &interval, &expires_in);
    TEST_ASSERT(ret == LDG_ERR_AOK, "device_code parse failed");
    TEST_ASSERT(strcmp(device_code, "dc1") == 0, "device_code mismatch");
    TEST_ASSERT(strcmp(user_code, "ABCD-EFGH") == 0, "user_code mismatch");
    TEST_ASSERT(strcmp(verify_url, "https://google.com/device") == 0, "verify_url mismatch");
    TEST_ASSERT(interval == 5, "interval mismatch");
    TEST_ASSERT(expires_in == 1800, "expires_in mismatch");
}

static void test_json_feed_parse_empty(void)
{
    const char *json_str = "{}";
    size_t len = strlen(json_str);

    yt_feed_t feed;
    memset(&feed, 0, sizeof(feed));

    yt_json_feed_parse(json_str, len, &feed);
    TEST_ASSERT(feed.video_cunt == 0, "empty feed should have 0 videos");
}

int main(void)
{
    TEST_RUN(test_cjson_basic);
    TEST_RUN(test_json_token_parse);
    TEST_RUN(test_json_device_code_parse);
    TEST_RUN(test_json_feed_parse_empty);

    fprintf(stderr, "api: %u/%u passed\n", tests_run - tests_failed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
