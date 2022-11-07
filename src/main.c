#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <app_event_manager.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include "common_modules.h"
#include "events/app_module_event.h"

#define MODULE main
#include <caf/events/module_state_event.h>

LOG_MODULE_REGISTER(MODULE, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

/* Application module message queue. */
#define APP_QUEUE_ENTRY_COUNT       10
#define APP_QUEUE_BYTE_ALIGNMENT    4

struct app_msg_data {
    union {
        struct app_module_event app;
    } module;
};

K_MSGQ_DEFINE(msgq_app, sizeof(struct app_msg_data), APP_QUEUE_ENTRY_COUNT,
  APP_QUEUE_BYTE_ALIGNMENT);


static struct module_data self = {
    .name = "app",
    .msg_q = &msgq_app,
    .supports_shutdown = true,
};

static bool app_event_handler(const struct app_event_header *aeh)
{
    // TODO: Get events
    return false;
}


static void data_sample_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    SEND_EVENT(app, APP_EVT_DATA_GET);
}


/* Data sample timer used in active mode. */
K_TIMER_DEFINE(data_sample_timer, data_sample_timer_handler, NULL);

int main(void)
{
    int retval = 0;

    if (0 == app_event_manager_init()) {
        module_set_state(MODULE_STATE_READY);
        SEND_EVENT(app, APP_EVT_START);
    } else {
        LOG_ERR("Application could not start, rebooting...");
        k_sleep(K_SECONDS(3));
        sys_reboot(SYS_REBOOT_COLD);
    }

    /* Get the thread ID for the start of the module */
    self.thread_id = k_current_get();

    retval = module_start(&self);
    if (0 != retval) {
        LOG_ERR("Failed to start the module!");
        SEND_ERROR(app, APP_EVT_ERROR, retval);
        return -1;
    }

    k_msleep(2000);

    SEND_EVENT(app, APP_EVT_DATA_GET);

    while (1) {
        // TODO: Handle event
        k_msleep(100);
    }

    return 0;
} /* main */


/* Handler for events */
APP_EVENT_LISTENER(MODULE, app_event_handler);

/* Module */
APP_EVENT_SUBSCRIBE(MODULE, app_module_event);

/* Other modules */
