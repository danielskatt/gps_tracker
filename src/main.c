#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include "src/lib/common_events.h"

#define MODULE main

LOG_MODULE_REGISTER(MODULE, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

int main(void)
{
    k_event_wait(&app_events, APP_EVENT_GNSS_INITIALIZED, 0, K_FOREVER);

    while (1) {
        k_msleep(100);
    }

    return 0;
} /* main */
