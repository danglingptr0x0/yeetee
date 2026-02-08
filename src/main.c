#include <stdint.h>
#include <locale.h>
#include <syslog.h>

#include <dangling/core/err.h>
#include <dangling/core/macros.h>
#include <dangling/mem/alloc.h>
#include <yeetee/core/conf.h>
#include <yeetee/core/err.h>
#include <yeetee/tui/tui.h>

int main(void)
{
    yt_conf_t conf = LDG_STRUCT_ZERO_INIT;
    yt_tui_t tui = LDG_STRUCT_ZERO_INIT;
    uint32_t ret = 0;

    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");

    openlog("yeetee", LOG_PID | LOG_NDELAY, LOG_USER);

    ret = ldg_mem_init();
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return 1; }

    ret = yt_conf_init(&conf);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return 1; }

    ret = yt_conf_load(&conf);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return 1; }

    ret = yt_tui_init(&tui, &conf);
    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return 1; }

    ret = yt_tui_run(&tui);

    yt_tui_shutdown(&tui);

    ldg_mem_shutdown();

    closelog();

    if (LDG_UNLIKELY(ret != LDG_ERR_AOK)) { return 1; }

    return 0;
}
