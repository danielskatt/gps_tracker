#include <stdio.h>
#include <zephyr.h>
#include <modem/lte_lc.h>
#include <zephyr/kernel.h>
#include <string.h>
#include <modem/sms.h>
#include "src/positioning/positioning.h"

#include "src/lib/common_events.h"

#define MODULE  sms_module

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_SMS_LOG_LEVEL);

/**
 * @brief Send a sms with current position and voltage level of the device
 *
 * @return int 0 on success, negative on fail
 */
static int sms_app_data_send(void)
{
    char str[150];
    float latitude, longitude, altitude, accuracy;
    int voltage_level;

    if (0 != positioning_data_get(&latitude, &longitude, &altitude, &accuracy, &voltage_level)) {
        return -1;
    }

    sprintf(str, "Latitude: %f\nLongitude: %f\nAltitude: %f\nAccuracy: %f\nVoltage level: %d", latitude, longitude,
      altitude, accuracy, voltage_level);

    return sms_send_text(CONFIG_SMS_SEND_PHONE_NUMBER, str);
}


/**
 * @brief Send if the device is currently searching for position or idle
 *
 * @return int 0 on success, negative on fail
 */
static int sms_app_log_send(void)
{
    char str[150] = { 0 };
    uint32_t events = k_event_wait(&app_events, -1, 0, K_NO_WAIT);

    if (events & APP_EVENT_APPLICATION_INITIALIZED) {
        sprintf(str, "Tracker idle");
    } else {
        sprintf(str, "Device not initialized!");
    }

    return sms_send_text(CONFIG_SMS_SEND_PHONE_NUMBER, str);
}


/**
 * @brief
 *
 * @param data
 * @param context
 */
static void sms_callback(struct sms_data *const data, void *context)
{
    if (data == NULL) {
        LOG_INF("%s with NULL data\n", __func__);
        return;
    }

    if (data->type == SMS_TYPE_DELIVER) {
        /* When SMS message is received, print information */
        struct sms_deliver_header *header = &data->header.deliver;

        if (header->concatenated.ref_number) {
            ;
        }

        if (0 == strcmp("Status", data->payload)) {
            k_event_post(&app_events, APP_EVENT_GNSS_SEARCH_REQ);
        }

        if (0 == strcmp("Log", data->payload)) {
            k_event_post(&app_events, APP_EVENT_SMS_LOG_SEND);
        }

        LOG_INF("\nSMS received:\n");
        LOG_INF("\tTime:   %02d-%02d-%02d %02d:%02d:%02d\n",
          header->time.year,
          header->time.month,
          header->time.day,
          header->time.hour,
          header->time.minute,
          header->time.second);

        LOG_INF("\tText:   '%s'\n", data->payload);
        LOG_INF("\tLength: %d\n", data->payload_len);

        if (header->app_port.present) {
            LOG_INF("\tApplication port addressing scheme: dest_port=%d, src_port=%d\n",
              header->app_port.dest_port,
              header->app_port.src_port);
        }
        if (header->concatenated.present) {
            LOG_INF("\tConcatenated short message: ref_number=%d, msg %d/%d\n",
              header->concatenated.ref_number,
              header->concatenated.seq_number,
              header->concatenated.total_msgs);
        }
    } else if (data->type == SMS_TYPE_STATUS_REPORT) {
        LOG_INF("SMS status report received\n");
    } else {
        LOG_INF("SMS protocol message with unknown type received\n");
    }
} /* sms_callback */


/**
 * @brief
 *
 */
static int sms_init()
{
    int handle = 0;

    handle = sms_register_listener(sms_callback, NULL);
    if (0 != handle) {
        LOG_INF("sms_register_listener returned err: %d\n", handle);
        return -1;
    }

    return 0;
}


static void sms_thread(void)
{
    int ret = 0;
    uint32_t events = 0;
    bool movement_triggered_send = true;

    if (0 != sms_init()) {
        return;
    }

    k_event_post(&app_events, APP_EVENT_SMS_INITIALIZED);
    k_event_wait(&app_events, APP_EVENT_APPLICATION_INITIALIZED, 0, K_FOREVER);

    while (1) {
        events = k_event_wait(&app_events, -1, 0, K_FOREVER);

        if (APP_EVENT_MOVEMENT_TRIGGERED & events) {
            if (movement_triggered_send) {
                ret = sms_send_text(CONFIG_SMS_SEND_PHONE_NUMBER, "Movement triggered!");
                if (0 == ret) {
                    movement_triggered_send = false;
                } else {
                    LOG_INF("sms_send returned err: %d\n", ret);
                }
            }
            k_event_set_masked(&app_events, 0, ~APP_EVENT_MOVEMENT_TRIGGERED);
        }

        if (APP_EVENT_GNSS_POSITION_FIXED & events) {
            ret = sms_app_data_send();
            if (ret) {
                LOG_INF("%d: sms_send returned err: %d\n", __LINE__, ret);
            }
            movement_triggered_send = true;
            k_event_set_masked(&app_events, APP_EVENT_GNSS_STOP, ~APP_EVENT_GNSS_POSITION_FIXED);
        }

        if (APP_EVENT_SMS_LOG_SEND & events) {
            ret = sms_app_log_send();
            if (ret) {
                LOG_INF("%d: sms_send returned err: %d\n", __LINE__, ret);
            }
            k_event_set_masked(&app_events, 0, ~APP_EVENT_SMS_LOG_SEND);
        }

        k_msleep(CONFIG_SMS_THREAD_SLEEP);
    }
} /* sms_thread */


#define SMS_THREAD_STACK_SIZE    2048
#define SMS_THREAD_PRIORITY      7

K_THREAD_DEFINE(sms_thread_id, SMS_THREAD_STACK_SIZE,
  sms_thread, NULL, NULL, NULL,
  K_PRIO_PREEMPT(SMS_THREAD_PRIORITY), 0, 0);
