#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dangling/core/err.h>
#include <yeetee/player/player.h>

static uint32_t tests_run = 0;
static uint32_t tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
            tests_run++; \
            if (!(cond)) { tests_failed++; fprintf(stderr, "FAIL: %s: %s\n", __func__, (msg)); } \
} while (0)

#define TEST_RUN(func) do { func(); } while (0)

static void test_player_init_shutdown(void)
{
    yt_player_t player;
    memset(&player, 0, sizeof(player));

    uint32_t ret = yt_player_init(&player);
    TEST_ASSERT(ret == LDG_ERR_AOK, "yt_player_init failed");
    TEST_ASSERT(player.mpv != NULL, "mpv handle should not be NULL");

    yt_player_shutdown(&player);
    TEST_ASSERT(player.mpv == NULL, "mpv handle should be NULL after shutdown");
}

static void test_player_init_null(void)
{
    uint32_t ret = yt_player_init(NULL);
    TEST_ASSERT(ret == LDG_ERR_FUNC_ARG_NULL, "yt_player_init(NULL) should return FUNC_ARG_NULL");
}

int main(void)
{
    TEST_RUN(test_player_init_shutdown);
    TEST_RUN(test_player_init_null);

    fprintf(stderr, "player: %u/%u passed\n", tests_run - tests_failed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
