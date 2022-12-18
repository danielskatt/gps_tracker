#include <zephyr.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

#include "drivers/sensor/accelerometer.h"
#include "src/lib/common_events.h"

#define MODULE  movement
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_MOVEMENT_LOG_LEVEL);

static void accelerometer_event_handler(const struct accelerometer_sensor_event *const evt)
{
    switch (evt->type) {
        case ACCELEROMETER_EVENT_TRIGGER:
            LOG_INF("New movement trigger! (x: %f ~ y: %f ~ z: %f)", evt->value_array[0], evt->value_array[1],
              evt->value_array[2]);
            k_event_post(&app_events, APP_EVENT_GNSS_SEARCH_REQ);
            break;
        case ACCELEROMETER_EVENT_ERROR:
            LOG_ERR("Accelerometer error!");
            break;
        default:
            break;
    }
}


static int movement_init()
{
    int retval = 0;
    double accelerometer_threshold = 5;

    retval = accelerometer_init(accelerometer_event_handler);
    retval |= accelerometer_movement_thres_set(accelerometer_threshold);
    retval |= accelerometer_trigger_callback_set(1);

    return retval;
}


static void movement_thread(void)
{
    if (0 != movement_init()) {
        LOG_ERR("Failed to init movement!");
        return;
    }

    LOG_INF("Movement initialized!");
    k_event_post(&app_events, APP_EVENT_MOVEMENT_INITIALIZED);

    while (1) {
        k_msleep(CONFIG_MOVEMENT_THREAD_SLEEP);
    }
}


#define MOVEMENT_THREAD_STACK_SIZE    1024
#define MOVEMENT_THREAD_PRIORITY      7

K_THREAD_DEFINE(movement_thread_id, MOVEMENT_THREAD_STACK_SIZE,
  movement_thread, NULL, NULL, NULL,
  K_PRIO_PREEMPT(MOVEMENT_THREAD_PRIORITY), 0, 0);
