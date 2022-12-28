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

static bool movement_triggered = false;

/**
 * @brief Send a sms with current position and voltage level of the device
 *
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

    if (0 != sms_init()) {
        return;
    }

    k_event_post(&app_events, APP_EVENT_SMS_INITIALIZED);
    k_event_wait(&app_events, APP_EVENT_APPLICATION_INITIALIZED, 0, K_FOREVER);

    while (1) {
        if (0 < k_event_wait(&app_events, APP_EVENT_MOVEMENT_TRIGGERED, 0, K_NO_WAIT)) {
            if (false == movement_triggered) {
                ret = sms_send_text(CONFIG_SMS_SEND_PHONE_NUMBER, "Movement triggered!");
                if (0 != ret) {
                    LOG_INF("sms_send returned err: %d\n", ret);
                } else {
                    movement_triggered = true;
                }
            }
            k_event_set_masked(&app_events, 0, ~APP_EVENT_MOVEMENT_TRIGGERED);
        }

        if (0 < k_event_wait(&app_events, APP_EVENT_GNSS_POSITION_FIXED, 0, K_NO_WAIT)) {
            ret = sms_app_data_send();
            if (ret) {
                LOG_INF("sms_send returned err: %d\n", ret);
            }
            movement_triggered = false;
            k_event_set_masked(&app_events, 0, ~APP_EVENT_GNSS_POSITION_FIXED);
        }

        k_msleep(CONFIG_SMS_THREAD_SLEEP);
    }
} /* sms_thread */


#define SMS_THREAD_STACK_SIZE    2048
#define SMS_THREAD_PRIORITY      7

K_THREAD_DEFINE(sms_thread_id, SMS_THREAD_STACK_SIZE,
  sms_thread, NULL, NULL, NULL,
  K_PRIO_PREEMPT(SMS_THREAD_PRIORITY), 0, 0);
