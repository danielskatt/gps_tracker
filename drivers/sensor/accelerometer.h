#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

/** Number of accelerometer channels. */
#define ACCELEROMETER_CHANNELS 3

/** @brief Enum containing callback events from library. */
enum accelerometer_event_type {
    ACCELEROMETER_EVENT_TRIGGER,
    /** Events propagated when an error associated with a sensor device occurs. */
    ACCELEROMETER_EVENT_ERROR,
};

/** @brief Structure containing external sensor data. */
struct accelerometer_sensor_event {
    /** Sensor type. */
    enum accelerometer_event_type type;
    /** Event data. */
    union {
        /** Array of external sensor values. */
        double value_array[ACCELEROMETER_CHANNELS];
        /** Single external sensor value. */
        double value;
    };
};

/** @brief External sensors library asynchronous event handler.
 *
 *  @param[in] evt The event and any associated parameters.
 */
typedef void (*accelerometer_handler_t)(const struct accelerometer_sensor_event *const evt);

/**
 * @brief Initializes the library, sets callback handler.
 *
 * @param[in] handler Pointer to callback handler.
 *
 * @return 0 on success or negative error value on failure.
 */
int accelerometer_init(accelerometer_handler_t handler);

/**
 * @brief Set the threshold that triggeres callback on accelerometer data.
 *
 * @param[in] threshold_new Variable that sets the accelerometer threshold value in m/s2.
 *
 * @return 0 on success or negative error value on failure.
 */
int accelerometer_movement_thres_set(double threshold_new);

/**
 * @brief Enable or disable accelerometer trigger handler.
 *
 * @param[in] enable Flag that enables or disables callback triggers from the accelerometer.
 *
 * @return 0 on success or negative error value on failure.
 */
int accelerometer_trigger_callback_set(bool enable);


#endif /* ACCELEROMETER_H */
