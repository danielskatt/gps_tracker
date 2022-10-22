#include <zephyr.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <date_time.h>
#include <nrf_modem_at.h>
#include <nrf_modem_gnss.h>
#include "src/events/app_module_event.h"
#include "src/events/gnss_module_event.h"
#include "src/common_modules/common_modules.h"
#include <zephyr/logging/log.h>

#define GNSS_TIMEOUT_DEFAULT            60
#define GNSS_INACTIVITY_TIMEOUT_SEC     5
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
    STATE_START_UP,
    STATE_INIT,
    STATE_RUNNING,
    STATE_SHUTDOWN,
} state;

/* GNSS module sub-state */
static enum gnss_sub_state {
    SUB_STATE_IDLE,
    SUB_STATE_SEARCH
} sub_state;

static void message_handler(struct gnss_msg_data *msg);

static uint16_t gnss_timeout = GNSS_TIMEOUT_DEFAULT;
static struct nrf_modem_gnss_nmea_data_frame nmea_data;
static struct nrf_modem_gnss_pvt_data_frame pvt_data;

K_MSGQ_DEFINE(event_msgq, sizeof(int), 10, 4);

static struct module_stats {
    uint64_t startup_time;
    uint8_t satellites_tracked;
} stats;

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
 * @param sv
 * @param sv_entries
 * @return uint8_t
 */
static uint8_t satellites_tracked_cnt(struct nrf_modem_gnss_sv *sv, uint8_t sv_entries)
{
    uint8_t satellite_tracked = 0;

    for (int i = 0 ; i < sv_entries ; i++) {
        if (sv[i].sv) {
            satellite_tracked++;
        }
    }

    return satellite_tracked;
}


/**
 * @brief
 *
 */
static void data_send_pvt(void)
{
    struct gnss_module_event *gnss_module_event = new_gnss_module_event();

    gnss_module_event->data.gnss.pvt.longitude = pvt_data.longitude;
    gnss_module_event->data.gnss.pvt.latitude = pvt_data.latitude;
    gnss_module_event->data.gnss.pvt.altitude = pvt_data.altitude;
    gnss_module_event->data.gnss.pvt.accuracy = pvt_data.accuracy;
    gnss_module_event->data.gnss.pvt.speed = pvt_data.speed;
    gnss_module_event->data.gnss.pvt.heading = pvt_data.heading;
    gnss_module_event->data.gnss.timestamp = k_uptime_get();
    gnss_module_event->type = GNSS_EVT_DATA_READY;
    gnss_module_event->data.gnss.format = GNSS_MODULE_DATA_FORMAT_PVT;
    gnss_module_event->data.gnss.satellites_tracked = satellites_tracked_cnt(pvt_data.sv, ARRAY_SIZE(pvt_data.sv));
    gnss_module_event->data.gnss.search_time = (uint32_t) (k_uptime_get() - stats.startup_time);

    APP_EVENT_SUBMIT(gnss_module_event);
}


/**
 * @brief
 *
 */
static void data_send_nmea(void)
{
    struct gnss_module_event *gnss_module_event = new_gnss_module_event();

    strncpy(gnss_module_event->data.gnss.nmea, nmea_data.nmea_str, sizeof(gnss_module_event->data.gnss.nmea) - 1);

    gnss_module_event->data.gnss.nmea[sizeof(gnss_module_event->data.gnss.nmea) - 1] = '\0';
    gnss_module_event->data.gnss.timestamp = k_uptime_get();
    gnss_module_event->type = GNSS_EVT_DATA_READY;
    gnss_module_event->data.gnss.format = GNSS_MODULE_DATA_FORMAT_NMEA;
    gnss_module_event->data.gnss.satellites_tracked = satellites_tracked_cnt(pvt_data.sv, ARRAY_SIZE(pvt_data.sv));
    gnss_module_event->data.gnss.search_time = (uint32_t) (k_uptime_get() - stats.startup_time);

    APP_EVENT_SUBMIT(gnss_module_event);
}


/**
 * @brief
 *
 */
static void search_start(void)
{
    int retval = 0;

    printk("Start searching!\n");

    /* When GNSS is used in single fix mode, it needs to be stopped before it can be started again */
    retval = nrf_modem_gnss_stop();
    if (0 != retval) {
        printk("Failed to stop GNSS, retval: %d\n", retval);
        return;
    }

    retval = nrf_modem_gnss_fix_interval_set(0);
    if (0 != retval) {
        printk("Failed to set GNSS fix interval, retval: %d\n", retval);
        return;
    }

    retval = nrf_modem_gnss_fix_retry_set(gnss_timeout);
    if (0 != retval) {
        printk("Failed to set GNSS timeout, retval: %d\n", retval);
        return;
    }

    if (IS_ENABLED(CONFIG_GNSS_MODULE_NMEA)) {
        retval = nrf_modem_gnss_nmea_mask_set(NRF_MODEM_GNSS_NMEA_GGA_MASK);
        if (0 != retval) {
            printk("Failed to set GNSS NMEA mask, retval: %d\n", retval);
            return;
        }
    }

    retval = nrf_modem_gnss_start();
    if (0 != retval) {
        printk("Failed to start GNSS, retval: %d", retval);
        return;
    }

    SEND_EVENT(gnss, GNSS_EVT_ACTIVE);
    stats.startup_time = k_uptime_get();
} /* search_start */


/**
 * @brief
 *
 */
static void inactive_send(void)
{
    SEND_EVENT(gnss, GNSS_EVT_INACTIVE);
}


/**
 * @brief
 *
 * @param timer_id
 */
static void inactivity_timeout_handler(struct k_timer *timer_id)
{
    /* Simulate a sleep event from GNSS with older MFWs. */
    int event = NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT;

    k_msgq_put(&event_msgq, &event, K_NO_WAIT);
}


static K_TIMER_DEFINE(inactivity_timer, inactivity_timeout_handler, NULL);

/**
 * @brief
 *
 */
static void time_set(void)
{
    /* datetime.year and datetime.month changed to the correct input format */
    struct tm gnss_time = {
        .tm_year = pvt_data.datetime.year - 1900,
        .tm_mon = pvt_data.datetime.month - 1,
        .tm_mday = pvt_data.datetime.day,
        .tm_hour = pvt_data.datetime.hour,
        .tm_min = pvt_data.datetime.minute,
        .tm_sec = pvt_data.datetime.seconds,
    };

    // Set time
    // date_time_set(&gnss_time);
}


/**
 * @brief
 *
 * @param data_list
 * @param count
 * @return true
 * @return false
 */
static bool gnss_data_requested(enum app_module_data_type *data_list, size_t count)
{
    for (size_t i = 0 ; i < count ; i++) {
        if (APP_DATA_GNSS == data_list[i]) {
            return true;
        }
    }

    return false;
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

        message_handler(&msg);
    }

    /* Handle GNSS module events */
    if (is_gnss_module_event(app_ev_header)) {
        struct gnss_module_event *event = cast_gnss_module_event(app_ev_header);
        struct gnss_msg_data msg = {
            .module.gnss = *event
        };

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

    // retval = gnss_init();
    // if (retval) {
    //     LOG_WRN("GNSS driver failed to init");
    //     printk("Failed to init gnss!\n");
    // }

    retval = gnss_module_init();
    if (retval) {
        printk("GNSS module failed to init");
        return;
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
    if (IS_EVENT(msg, gnss, GNSS_EVT_INACTIVE)) {
        sub_state_set(SUB_STATE_IDLE);
    }

    if (IS_EVENT(msg, app, APP_EVT_DATA_GET)) {
        if (false == gnss_data_requested(msg->module.app.data_list, msg->module.app.count)) {
            return;
        }
    }
}


/**
 * @brief
 *
 * @param msg
 */
static void state_running_gnss_idle(struct gnss_msg_data *msg)
{
    if (IS_EVENT(msg, gnss, GNSS_EVT_ACTIVE)) {
        sub_state_set(SUB_STATE_SEARCH);
    }

    if (IS_EVENT(msg, app, APP_EVT_DATA_GET)) {
        if (false == gnss_data_requested(msg->module.app.data_list, msg->module.app.count)) {
            return;
        }

        search_start();
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
        case STATE_START_UP:
            break;
        case STATE_INIT:
            state_init(msg);
            break;
        case STATE_RUNNING:
            switch (sub_state) {
                case SUB_STATE_IDLE:
                    state_running_gnss_idle(msg);
                    break;
                case SUB_STATE_SEARCH:
                    state_search(msg);
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
    on_all_states(msg);
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

    LOG_DBG("Tracked satellites:");

    for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; i++) {
        if (pvt_data.sv[i].sv == 0) {
            break;
        }

        struct nrf_modem_gnss_sv *sv_data = &pvt_data.sv[i];

        LOG_DBG("PRN: %2d, C/N0: %4.1f, in fix: %d, unhealthy: %d",
          sv_data->sv,
          sv_data->cn0 / 10.0,
          sv_data->flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX ? 1 : 0,
          sv_data->flags & NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY ? 1 : 0);
    }
}


/**
 * @brief
 *
 */
static void timeout_send(void)
{
    struct gnss_module_event *gnss_module_event = new_gnss_module_event();

    gnss_module_event->data.gnss.search_time = (uint32_t) (k_uptime_get() - stats.startup_time);
    gnss_module_event->data.gnss.satellites_tracked = satellites_tracked_cnt(pvt_data.sv, ARRAY_SIZE(pvt_data.sv));
    gnss_module_event->type = GNSS_EVT_TIMEOUT;

    APP_EVENT_SUBMIT(gnss_module_event);
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
                    k_timer_stop(&inactivity_timer);
                    inactive_send();
                    time_set();
                    gnss_fixed = true;

                    if (IS_ENABLED(CONFIG_GNSS_MODULE_PVT)) {
                        data_send_pvt();
                    }
                } else {
                    k_timer_start(&inactivity_timer, K_SECONDS(GNSS_INACTIVITY_TIMEOUT_SEC), K_NO_WAIT);
                    gnss_fixed = false;
                }

                print_pvt();

                break;
            case NRF_MODEM_GNSS_EVT_NMEA:
                if (IS_ENABLED(CONFIG_GNSS_MODULE_PVT) && gnss_fixed) {
                    retval = nrf_modem_gnss_read(nmea_data.nmea_str, sizeof(struct nrf_modem_gnss_nmea_data_frame),
                        NRF_MODEM_GNSS_DATA_NMEA);
                    if (0 != retval) {
                        return;
                    }
                    data_send_nmea();
                }
                break;
            case NRF_MODEM_GNSS_EVT_BLOCKED:
                k_timer_stop(&inactivity_timer);
                inactive_send();
                break;
            case NRF_MODEM_GNSS_EVT_UNBLOCKED:
                break;
            case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
                break;
            default:
                break;
        }
    }
} /* gnss_event_thread */


#define GNSS_EVENT_THREAD_STACK_SIZE    1024
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
