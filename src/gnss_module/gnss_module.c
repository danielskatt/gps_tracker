#include <zephyr.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <date_time.h>
#include <nrf_modem_at.h>
#include <nrf_modem_gnss.h>
#include <modem/lte_lc.h>

#include "src/lib/common_events.h"

#define MODULE  gnss_module

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_GNSS_MODULE_LOG_LEVEL);

static struct nrf_modem_gnss_nmea_data_frame nmea_data;
static struct nrf_modem_gnss_pvt_data_frame pvt_data;

K_MSGQ_DEFINE(event_msgq, sizeof(int), 10, 4);

static const char update_indicator[] = { '\\', '|', '/', '-' };

static int gnss_start_search(void)
{
    int retval = 0;

    retval |= nrf_modem_gnss_stop();

    /* Only use the gps, not qzss */
    uint8_t system_mask = NRF_MODEM_GNSS_SYSTEM_GPS_MASK;

    retval |= nrf_modem_gnss_fix_retry_set(CONFIG_GNSS_SAMPLE_PERIODIC_TIMEOUT);
    retval |= nrf_modem_gnss_fix_interval_set(CONFIG_GNSS_SAMPLE_PERIODIC_INTERVAL);
    retval |= nrf_modem_gnss_system_mask_set(system_mask);

    /* Enable all supported NMEA messages. */
    uint16_t nmea_mask = NRF_MODEM_GNSS_NMEA_RMC_MASK
      | NRF_MODEM_GNSS_NMEA_GGA_MASK
      | NRF_MODEM_GNSS_NMEA_GLL_MASK
      | NRF_MODEM_GNSS_NMEA_GSA_MASK
      | NRF_MODEM_GNSS_NMEA_GSV_MASK;

    retval = nrf_modem_gnss_nmea_mask_set(nmea_mask);

    retval |= nrf_modem_gnss_start();

    k_event_set_masked(&app_events, 0, ~APP_EVENT_GNSS_SEARCH);

    return retval;
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
        LOG_ERR("Failed to but GNSS event to message queue!");
    }
}


static int gnss_init(void)
{
    int retval = 0;

    retval = lte_lc_init();
    if (0 != retval) {
        LOG_WRN("Failed to init LTE link controller\n");
        return retval;
    }

    retval = lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_GPS, LTE_LC_SYSTEM_MODE_PREFER_AUTO);
    if (0 != retval) {
        LOG_WRN("Failed to activate GNSS system mode\n");
        return retval;
    }

    retval = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);
    if (0 != retval) {
        LOG_WRN("Failed to activate GNSS functional mode\n");
        return retval;
    }

    return retval;
} /* gnss_init */


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
        LOG_WRN("Failed to set GNSS event handler, retval: %d", retval);
        return retval;
    }

    retval = gnss_init();
    if (retval) {
        LOG_WRN("GNSS driver failed to init");
        return retval;
    }

    return retval;
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
    printk("Date:           %04u-%02u-%02u\n", pvt_data.datetime.year, pvt_data.datetime.month, pvt_data.datetime.day);
    printk("Time (UTC):     %02u:%02u:%02u.%03u\n", pvt_data.datetime.hour, pvt_data.datetime.minute,
      pvt_data.datetime.seconds, pvt_data.datetime.ms);
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
    static uint8_t cnt = 0;

    printk("\033[1;1H");
    printk("\033[2J");

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
    printk("Searching [%c]\n", update_indicator[cnt % 4]);
    cnt++;
}


/**
 * @brief
 *
 */
static void gnss_event_handler_thread(void)
{
    int retval = 0;

    retval = gnss_module_init();
    if (0 != retval) {
        return;
    }

    k_event_post(&app_events, APP_EVENT_GNSS_INITIALIZED);

    while (1) {
        if (0 < k_event_wait(&app_events, APP_EVENT_GNSS_SEARCH, 0, K_NO_WAIT)) {
            gnss_start_search();
        }

        if (0 < k_event_wait(&app_events, APP_EVENT_GNSS_STOP, 0, K_NO_WAIT)) {
            nrf_modem_gnss_stop();
        }

        if (0 < k_event_wait(&app_events, APP_EVENT_GNSS_POSITION_FIXED, 0, K_NO_WAIT)) {
            print_fix_data();
        }
        k_msleep(CONFIG_GNSS_MODULE_THREAD_SLEEP);
    } /* gnss_event_handler_thread */
} /* gnss_event_thread */


#define GNSS_EVENT_HANDLER_THREAD_STACK_SIZE    4096
#define GNSS_EVENT_HANDLER_THREAD_PRIORITY      7

K_THREAD_DEFINE(gnss_event_handler_thread_id, GNSS_EVENT_HANDLER_THREAD_STACK_SIZE,
  gnss_event_handler_thread, NULL, NULL, NULL,
  K_PRIO_PREEMPT(GNSS_EVENT_HANDLER_THREAD_PRIORITY), 0, 0);

/**
 * @brief
 *
 */
static void gnss_event_thread(void)
{
    int retval = 0;
    int event = 0;
    static bool gnss_fixed = false;

    k_event_wait(&app_events, APP_EVENT_GNSS_INITIALIZED, 0, K_FOREVER);

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
                    k_event_post(&app_events, APP_EVENT_GNSS_POSITION_FIXED);
                } else {
                    print_pvt();
                }

                break;
            case NRF_MODEM_GNSS_EVT_NMEA:
                if (IS_ENABLED(CONFIG_GNSS_MODULE_PVT) && gnss_fixed) {
                    retval = nrf_modem_gnss_read(&nmea_data, sizeof(struct nrf_modem_gnss_nmea_data_frame),
                        NRF_MODEM_GNSS_DATA_NMEA);
                    if (0 != retval) {
                        break;
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
        k_msleep(CONFIG_GNSS_MODULE_THREAD_SLEEP);
    }
} /* gnss_event_thread */


#define GNSS_EVENT_THREAD_STACK_SIZE    4096
#define GNSS_EVENT_THREAD_PRIORITY      7

K_THREAD_DEFINE(gnss_event_thread_id, GNSS_EVENT_THREAD_STACK_SIZE,
  gnss_event_thread, NULL, NULL, NULL,
  K_PRIO_PREEMPT(GNSS_EVENT_THREAD_PRIORITY), 0, 0);
