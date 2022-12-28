#include <zephyr.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

#include "drivers/sensor/accelerometer.h"
#include "src/lib/common_events.h"

#define MODULE  movement
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_MOVEMENT_LOG_LEVEL);

#define MOVEMENT_THRESHOLD  5.0f

static void accelerometer_event_handler(const struct accelerometer_sensor_event *const evt)
{
    switch (evt->type) {
        case ACCELEROMETER_EVENT_TRIGGER:
            LOG_INF("New movement trigger! (x: %f ~ y: %f ~ z: %f)", evt->value_array[0], evt->value_array[1],
              evt->value_array[2]);
            k_event_post(&app_events, APP_EVENT_MOVEMENT_TRIGGERED);
            k_event_post(&app_events, APP_EVENT_GNSS_SEARCH_REQ);
            break;
        case ACCELEROMETER_EVENT_ERROR:
            LOG_ERR("Accelerometer error!");
            break;
        default:
            break;
    }
}


int movement_init()
{
    int retval = 0;

    retval = accelerometer_init(accelerometer_event_handler);
    retval |= accelerometer_movement_thres_set(MOVEMENT_THRESHOLD);
    retval |= accelerometer_trigger_callback_set(1);

    return retval;
}
