#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <app_event_manager.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include "src/lib/common_events.h"

#define MODULE main

LOG_MODULE_REGISTER(MODULE, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

int main(void)
{
    k_event_wait(&app_events, APP_EVENT_GNSS_INITIALIZED, 0, K_FOREVER);

    k_msleep(1000);

    k_event_post(&app_events, APP_EVENT_GNSS_SEARCH);

    while (1) {
        k_msleep(100);
    }

    return 0;
} /* main */
