#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dangling/core/err.h>
#include <notcurses/notcurses.h>
#include <yeetee/yeetee.h>
#include <yeetee/tui/input.h>
#include <yeetee/tui/queue.h>

static uint32_t tests_run = 0;
static uint32_t tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
            tests_run++; \
            if (!(cond)) { tests_failed++; fprintf(stderr, "FAIL: %s: %s\n", __func__, (msg)); } \
} while (0)

#define TEST_RUN(func) do { func(); } while (0)

static void test_input_quit(void)
{
    struct ncinput ni;
    memset(&ni, 0, sizeof(ni));
    ni.id = 'q';

    yt_action_t action = yt_input_dispatch(&ni);
    TEST_ASSERT(action == YT_ACTION_QUIT, "q should map to QUIT");
}

static void test_input_nav(void)
{
    struct ncinput ni;
    memset(&ni, 0, sizeof(ni));

    ni.id = 'j';
    TEST_ASSERT(yt_input_dispatch(&ni) == YT_ACTION_DOWN, "j should map to DOWN");

    ni.id = 'k';
    TEST_ASSERT(yt_input_dispatch(&ni) == YT_ACTION_UP, "k should map to UP");

    ni.id = 'h';
    TEST_ASSERT(yt_input_dispatch(&ni) == YT_ACTION_LEFT, "h should map to LEFT");

    ni.id = 'l';
    TEST_ASSERT(yt_input_dispatch(&ni) == YT_ACTION_RIGHT, "l should map to RIGHT");
}

static void test_input_select(void)
{
    struct ncinput ni;
    memset(&ni, 0, sizeof(ni));
    ni.id = NCKEY_ENTER;

    yt_action_t action = yt_input_dispatch(&ni);
    TEST_ASSERT(action == YT_ACTION_SELECT, "Enter should map to SELECT");
}

static void test_input_unknown(void)
{
    struct ncinput ni;
    memset(&ni, 0, sizeof(ni));
    ni.id = 'z';

    yt_action_t action = yt_input_dispatch(&ni);
    TEST_ASSERT(action == YT_ACTION_NONE, "unknown key should map to NONE");
}

static void test_queue_push_pop(void)
{
    yt_queue_t q;
    yt_queue_init(&q);

    yt_video_t v1;
    memset(&v1, 0, sizeof(v1));
    strncpy(v1.id, "vid1", YT_VIDEO_ID_MAX - 1);
    strncpy(v1.title, "Video One", YT_VIDEO_TITLE_MAX - 1);

    yt_video_t v2;
    memset(&v2, 0, sizeof(v2));
    strncpy(v2.id, "vid2", YT_VIDEO_ID_MAX - 1);
    strncpy(v2.title, "Video Two", YT_VIDEO_TITLE_MAX - 1);

    yt_video_t v3;
    memset(&v3, 0, sizeof(v3));
    strncpy(v3.id, "vid3", YT_VIDEO_ID_MAX - 1);
    strncpy(v3.title, "Video Three", YT_VIDEO_TITLE_MAX - 1);

    TEST_ASSERT(yt_queue_push(&q, &v1) == LDG_ERR_AOK, "push v1 failed");
    TEST_ASSERT(yt_queue_push(&q, &v2) == LDG_ERR_AOK, "push v2 failed");
    TEST_ASSERT(yt_queue_push(&q, &v3) == LDG_ERR_AOK, "push v3 failed");
    TEST_ASSERT(q.cunt == 3, "cunt should be 3");

    const yt_video_t *cur = yt_queue_current_get(&q);
    TEST_ASSERT(cur != NULL, "current should not be NULL");
    TEST_ASSERT(strcmp(cur->id, "vid1") == 0, "current should be vid1");

    TEST_ASSERT(yt_queue_next(&q) == LDG_ERR_AOK, "next failed");
    cur = yt_queue_current_get(&q);
    TEST_ASSERT(strcmp(cur->id, "vid2") == 0, "current should be vid2 after next");

    TEST_ASSERT(yt_queue_next(&q) == LDG_ERR_AOK, "next failed");
    cur = yt_queue_current_get(&q);
    TEST_ASSERT(strcmp(cur->id, "vid3") == 0, "current should be vid3 after next");

    TEST_ASSERT(yt_queue_next(&q) == LDG_ERR_AOK, "next wrap failed");
    cur = yt_queue_current_get(&q);
    TEST_ASSERT(strcmp(cur->id, "vid1") == 0, "current should wrap to vid1");

    TEST_ASSERT(yt_queue_prev(&q) == LDG_ERR_AOK, "prev failed");
    cur = yt_queue_current_get(&q);
    TEST_ASSERT(strcmp(cur->id, "vid3") == 0, "prev should wrap to vid3");
}

static void test_queue_clear(void)
{
    yt_queue_t q;
    yt_queue_init(&q);

    yt_video_t v;
    memset(&v, 0, sizeof(v));
    strncpy(v.id, "vid1", YT_VIDEO_ID_MAX - 1);

    yt_queue_push(&q, &v);
    yt_queue_push(&q, &v);
    TEST_ASSERT(q.cunt == 2, "cunt should be 2");

    yt_queue_clear(&q);
    TEST_ASSERT(q.cunt == 0, "cunt should be 0 after clear");
    TEST_ASSERT(q.current_idx == UINT32_MAX, "current_idx should be UINT32_MAX after clear");

    const yt_video_t *cur = yt_queue_current_get(&q);
    TEST_ASSERT(cur == NULL, "current should be NULL after clear");
}

static void test_queue_full(void)
{
    yt_queue_t q;
    yt_queue_init(&q);

    yt_video_t v;
    memset(&v, 0, sizeof(v));
    strncpy(v.id, "vid", YT_VIDEO_ID_MAX - 1);

    uint32_t i = 0;
    for (; i < YT_QUEUE_MAX; i++)
    {
        uint32_t ret = yt_queue_push(&q, &v);
        TEST_ASSERT(ret == LDG_ERR_AOK, "push should succeed within capacity");
    }

    TEST_ASSERT(q.cunt == YT_QUEUE_MAX, "cunt should be YT_QUEUE_MAX");

    uint32_t ret = yt_queue_push(&q, &v);
    TEST_ASSERT(ret == LDG_ERR_FULL, "push beyond capacity should return FULL");
}

int main(void)
{
    TEST_RUN(test_input_quit);
    TEST_RUN(test_input_nav);
    TEST_RUN(test_input_select);
    TEST_RUN(test_input_unknown);
    TEST_RUN(test_queue_push_pop);
    TEST_RUN(test_queue_clear);
    TEST_RUN(test_queue_full);

    fprintf(stderr, "tui: %u/%u passed\n", tests_run - tests_failed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}
