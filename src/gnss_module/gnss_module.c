#include <zephyr.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <date_time.h>
#include <nrf_modem_at.h>
#include <nrf_modem_gnss.h>
#include "drivers/gnss/gnss.h"
#include "src/events/app_module_event.h"
#include "src/events/gnss_module_event.h"
#include "src/common_modules/common_modules.h"
#include <zephyr/logging/log.h>

#define MODULE  gnss_module

LOG_MODULE_REGISTER(MODULE, CONFIG_GNSS_MODULE_LOG_LEVEL);

struct gnss_msg_data {
    union {
        struct app_module_event app;
        struct gnss_module_event gnss;
    } module;
};

/* GNSS module state */
static enum gnss_state {
    STATE_INIT,
    STATE_RUNNING,
    STATE_SHUTDOWN,
} state;

/* GNSS module sub-state */
static enum gnss_sub_state {
    SUB_STATE_SEARCH,
    SUB_STATE_IDLE
} sub_state;

static void message_handler(struct gnss_msg_data *msg);

static struct nrf_modem_gnss_nmea_data_frame nmea_data;
static struct nrf_modem_gnss_pvt_data_frame pvt_data;

K_MSGQ_DEFINE(event_msgq, sizeof(int), 10, 4);

static struct module_data self = {
    .name = "gnss",
    .msg_q = NULL,
    .supports_shutdown = true,
};

/**
 * @brief
 *
 * @param new_state
 */
static void state_set(enum gnss_state new_state)
{
    state = new_state;
}


/**
 * @brief
 *
 * @param new_sub_state
 */
static void sub_state_set(enum gnss_state new_sub_state)
{
    sub_state = new_sub_state;
}


/**
 * @brief
 *
 * @param app_ev_header
 * @return true
 * @return false
 */
static bool app_event_handler(const struct app_event_header *app_ev_header)
{
    /* Handle app module events */
    if (is_app_module_event(app_ev_header)) {
        struct app_module_event *event = cast_app_module_event(app_ev_header);
        struct gnss_msg_data msg = {
            .module.app = *event
        };

        printk("Received event from app\n");
        message_handler(&msg);
    }

    /* Handle GNSS module events */
    if (is_gnss_module_event(app_ev_header)) {
        struct gnss_module_event *event = cast_gnss_module_event(app_ev_header);
        struct gnss_msg_data msg = {
            .module.gnss = *event
        };

        printk("Received event from gnss\n");
        message_handler(&msg);
    }

    /* Don't reset the event */
    return false;
}


/**
 * @brief
 *
 * @param event
 */
static void gnss_event_handler(int event)
{
    int retval = 0;

    retval = k_msgq_put(&event_msgq, &event, K_NO_WAIT);
    if (retval) {
        SEND_ERROR(gnss, GNSS_EVT_ERROR_CODE, retval);
    }
}


/**
 * @brief
 *
 * @return int
 */
static int gnss_module_init(void)
{
    int retval = 0;

    retval = nrf_modem_gnss_event_handler_set(gnss_event_handler);

    if (0 != retval) {
        printk("Failed to set GNSS event handler, retval: %d", retval);
    }

    retval = gnss_init();
    if (retval) {
        LOG_WRN("GNSS driver failed to init");
        printk("Failed to init gnss!\n");
    }

    return retval;
}


/**
 * @brief
 *
 * @param msg
 */
static void state_init(struct gnss_msg_data *msg)
{
    int retval = 0;

    if (IS_EVENT(msg, app, APP_EVT_START)) {
        retval = module_start(&self);
        retval = gnss_module_init();
        if (retval) {
            printk("GNSS module failed to init");
            return;
        }
    }
    state_set(STATE_RUNNING);
}


/**
 * @brief
 *
 * @param msg
 */
static void state_search(struct gnss_msg_data *msg)
{
    if (IS_EVENT(msg, gnss, GNSS_EVT_ACTIVE)) {
        sub_state_set(SUB_STATE_IDLE);
    }

    if (IS_EVENT(msg, app, APP_EVT_DATA_GET)) {
        gnss_start_search();
        SEND_EVENT(gnss, GNSS_EVT_ACTIVE);
        sub_state_set(SUB_STATE_IDLE);
    }
}


/**
 * @brief
 *
 * @param msg
 */
static void state_running_gnss_idle(struct gnss_msg_data *msg)
{
    if (IS_EVENT(msg, gnss, GNSS_EVT_INACTIVE)) {
        sub_state_set(SUB_STATE_SEARCH);
    }

    if (IS_EVENT(msg, app, APP_EVT_DATA_GET)) {
        ;
    }
}


/**
 * @brief
 *
 * @param msg
 */
static void on_all_states(struct gnss_msg_data *msg)
{
    int retval = 0;

    if (IS_EVENT(msg, app, APP_EVT_START)) {
        retval = module_start(&self);
        if (0 != retval) {
            SEND_ERROR(gnss, GNSS_EVT_ERROR_CODE, retval);
            return;
        }

        state_set(STATE_INIT);
    }
}


/**
 * @brief
 *
 * @param msg
 */
static void message_handler(struct gnss_msg_data *msg)
{
    printk("State = %d - sub_state: %d\n", state, sub_state);
    switch (state) {
        case STATE_INIT:
            state_init(msg);
            break;
        case STATE_RUNNING:
            switch (sub_state) {
                case SUB_STATE_SEARCH:
                    state_search(msg);
                case SUB_STATE_IDLE:
                    state_running_gnss_idle(msg);
                    break;
                    break;
                default:
                    break;
            }
            break;
        case STATE_SHUTDOWN:
            break;
        default:
            break;
    }
    // on_all_states(msg);
}


/**
 * @brief
 *
 */
static void print_fix_data(void)
{
    printk("Latitude:       %.06f\n", pvt_data.latitude);
    printk("Longitude:      %.06f\n", pvt_data.longitude);
    printk("Altitude:       %.01f m\n", pvt_data.altitude);
    printk("Accuracy:       %.01f m\n", pvt_data.accuracy);
    printk("Speed:          %.01f m/s\n", pvt_data.speed);
    printk("Speed accuracy: %.01f m/s\n", pvt_data.speed_accuracy);
    printk("Heading:        %.01f deg\n", pvt_data.heading);
    printk("Date:           %04u-%02u-%02u\n",
      pvt_data.datetime.year,
      pvt_data.datetime.month,
      pvt_data.datetime.day);
    printk("Time (UTC):     %02u:%02u:%02u.%03u\n",
      pvt_data.datetime.hour,
      pvt_data.datetime.minute,
      pvt_data.datetime.seconds,
      pvt_data.datetime.ms);
    printk("PDOP:           %.01f\n", pvt_data.pdop);
    printk("HDOP:           %.01f\n", pvt_data.hdop);
    printk("VDOP:           %.01f\n", pvt_data.vdop);
    printk("TDOP:           %.01f\n", pvt_data.tdop);
}


/**
 * @brief
 *
 */
static void print_pvt(void)
{
    if (pvt_data.sv[0].sv == 0) {
        LOG_DBG("No tracked satellites");
        return;
    }

    uint8_t tracked = 0;
    uint8_t in_fix = 0;
    uint8_t unhealthy = 0;

    for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i) {
        if (pvt_data.sv[i].sv > 0) {
            tracked++;

            if (pvt_data.sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX) {
                in_fix++;
            }

            if (pvt_data.sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY) {
                unhealthy++;
            }
        }
    }

    printk("Tracking: %2d Using: %2d Unhealthy: %d\n", tracked, in_fix, unhealthy);
}


/**
 * @brief
 *
 */
static void gnss_event_thread(void)
{
    int retval = 0;
    int event = 0;
    static bool gnss_fixed = false;

    while (1) {
        k_msgq_get(&event_msgq, &event, K_FOREVER);

        switch (event) {
            case NRF_MODEM_GNSS_EVT_PVT:
                retval = nrf_modem_gnss_read(&pvt_data, sizeof(struct nrf_modem_gnss_pvt_data_frame),
                    NRF_MODEM_GNSS_DATA_PVT);
                if (retval) {
                    break;
                }

                if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
                    gnss_fixed = true;
                    print_fix_data();
                }
                print_pvt();

                break;
            case NRF_MODEM_GNSS_EVT_NMEA:
                if (IS_ENABLED(CONFIG_GNSS_MODULE_PVT) && gnss_fixed) {
                    retval = nrf_modem_gnss_read(&nmea_data, sizeof(struct nrf_modem_gnss_nmea_data_frame),
                        NRF_MODEM_GNSS_DATA_NMEA);
                    if (0 != retval) {
                        return;
                    }
                }
                break;
            case NRF_MODEM_GNSS_EVT_BLOCKED:
                break;
            case NRF_MODEM_GNSS_EVT_UNBLOCKED:
                break;
            case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
                break;
            default:
                break;
        }
        k_msleep(10);
    }
} /* gnss_event_thread */


#define GNSS_EVENT_THREAD_STACK_SIZE    4096
#define GNSS_EVENT_THREAD_PRIORITY      7

K_THREAD_DEFINE(gnss_event_thread_id, GNSS_EVENT_THREAD_STACK_SIZE,
  gnss_event_thread, NULL, NULL, NULL,
  K_PRIO_PREEMPT(GNSS_EVENT_THREAD_PRIORITY), 0, 0);

/* Handler for events */
APP_EVENT_LISTENER(MODULE, app_event_handler);

/* Module */
APP_EVENT_SUBSCRIBE(MODULE, gnss_module_event);

/* Other modules */
APP_EVENT_SUBSCRIBE_EARLY(MODULE, app_module_event);
