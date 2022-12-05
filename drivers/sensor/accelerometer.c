#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/sensor.h>

#include "accelerometer.h"

struct env_sensor {
    enum sensor_channel channel;
    const struct device *dev;
    struct k_spinlock lock;
};

static struct env_sensor accel_sensor = {
    .channel = SENSOR_CHAN_ACCEL_XYZ,
    .dev = DEVICE_DT_GET(DT_ALIAS(accelerometer)),
};

int accelerometer_init(void)
{
    if (!device_is_ready(accel_sensor.dev)) {
        LOG_ERR("Accelerometer device is not ready");
        evt.type = EXT_SENSOR_EVT_ACCELEROMETER_ERROR;
        evt_handler(&evt);
        return;
    }

    struct sensor_trigger trig = {
        .chan = SENSOR_CHAN_ACCEL_XYZ,
        .type = SENSOR_TRIG_THRESHOLD
    };

    int err = sensor_trigger_set(accel_sensor.dev,
        &trig, accelerometer_trigger_handler);

    if (err) {
        LOG_ERR("Could not set trigger for device %s, error: %d",
          accel_sensor.dev->name, err);
        return err;
    }
}
