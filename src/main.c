#include <zephyr.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include "src/movement/movement.h"

#include "src/lib/common_events.h"

int init()
{
    int retval = 0;

    if (0 != movement_init()) {
        printk("Failed to init movement!");
        return -1;
    }

    return retval;
}


void main(void)
{
    if (0 != init()) {
        return;
    }

    k_event_wait_all(&app_events, APP_EVENT_GNSS_INITIALIZED | APP_EVENT_SMS_INITIALIZED, 0, K_FOREVER);
    k_event_post(&app_events, APP_EVENT_APPLICATION_INITIALIZED);
} /* main */
