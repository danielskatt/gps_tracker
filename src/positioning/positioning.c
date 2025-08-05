#include <zephyr.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <date_time.h>
#include <nrf_modem_at.h>
#include <nrf_modem_gnss.h>
#include <modem/lte_lc.h>
#include <modem/modem_info.h>

#include "src/lib/common_events.h"

#define MODULE  gnss_module

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_POSITIONING_LOG_LEVEL);

#ifndef CONFIG_SMS
static const char update_indicator[] = { '\\', '|', '/', '-' };
#endif

static struct nrf_modem_gnss_nmea_data_frame nmea_data;
static struct nrf_modem_gnss_pvt_data_frame pvt_data;
static struct modem_param_info modem_param;

K_MSGQ_DEFINE(event_msgq, sizeof(int), 10, 4);

int positioning_data_get(float *latitude, float *longitude, float *altitude, float *accuracy, int *voltage_level)
{
    if (0 == k_event_wait(&app_events, APP_EVENT_GNSS_POSITION_FIXED, 0, K_NO_WAIT)) {
        return -1;
    }

    if (0 != modem_info_params_get(&modem_param)) {
        return -1;
    }

    *latitude = pvt_data.latitude;
    *longitude = pvt_data.longitude;
    *altitude = pvt_data.altitude;
    *accuracy = pvt_data.accuracy;
    *voltage_level = modem_param.device.battery.value;

    return 0;
}


/**
 * @brief Start a search sequence to search for GNSS position
 *
 * @return int 0 on success, negative on fail
 */
static int gnss_start_search(void)
{
    int retval = 0;

    k_event_post(&app_events, APP_EVENT_GNSS_SEARCHING);

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

    k_event_set_masked(&app_events, 0, ~APP_EVENT_GNSS_SEARCH_REQ);

    return retval;
}


/**
 * @brief Put an event from the GNSS driver to the message queue
 *
 * @param event the event received from the gnss driver
 */
static void gnss_event_handler(int event)
{
    int retval = 0;

    retval = k_msgq_put(&event_msgq, &event, K_NO_WAIT);
    if (retval) {
        LOG_ERR("%s: Failed to but GNSS event to message queue!", __func__);
    }
}


/**
 * @brief Init the needed gnss handler and mode to be able to use the GNSS driver
 *
 * @return int 0 success, negative on fail
 */
static int gnss_module_init(void)
{
    int retval = 0;

    /* Set the GNSS driver event handler */
    retval = nrf_modem_gnss_event_handler_set(gnss_event_handler);
    if (0 != retval) {
        LOG_WRN("%s: Failed to set GNSS event handler, retval: %d", __func__, retval);
        return retval;
    }

    /* Init and set the LTE mode */
    retval = lte_lc_init();
    if (0 != retval) {
        LOG_WRN("%s: Failed to init LTE link controller", __func__);
        return retval;
    }

    /* Why is this out-commented?, not needed? */
    // retval = lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_LTEM_GPS, LTE_LC_SYSTEM_MODE_PREFER_AUTO);
    // if (0 != retval) {
    //     LOG_WRN("%s: Failed to activate GNSS system mode", __func__);
    //     return retval;
    // }
    // retval = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);
    // if (0 != retval) {
    //     LOG_WRN("%s: Failed to activate GNSS functional mode", __func__);
    //     return retval;
    // }

    retval = modem_info_init();
    if (0 != retval) {
        LOG_WRN("modem_info_init, error: %d", retval);
        return retval;
    }

    retval = modem_info_params_init(&modem_param);
    if (0 != retval) {
        LOG_WRN("modem_info_params_init, error: %d", retval);
        return retval;
    }

    return retval;
} /* gnss_module_init */


#ifndef CONFIG_SMS

static void print_battery_voltage(void)
{
    int retval = 0;

    retval = modem_info_params_get(&modem_param);
    if (0 != retval) {
        LOG_ERR("modem_info_params_get, error: %d", retval);
        return;
    }

    /* Clear screen */
    LOG_DBG("\033[1;1H");
    LOG_DBG("\033[2J");

    LOG_DBG("Battery voltage: %d mV", modem_param.device.battery.value);
}


/**
 * @brief Print data from a fixed position from the GNSS
 *
 */
static void print_fix_data(void)
{
    /* Clear screen */
    LOG_DBG("\033[1;1H");
    LOG_DBG("\033[2J");

    LOG_DBG("Latitude:       %.06f", pvt_data.latitude);
    LOG_DBG("Longitude:      %.06f", pvt_data.longitude);
    LOG_DBG("Altitude:       %.01f m", pvt_data.altitude);
    LOG_DBG("Accuracy:       %.01f m", pvt_data.accuracy);
    LOG_DBG("Speed:          %.01f m/s", pvt_data.speed);
    LOG_DBG("Speed accuracy: %.01f m/s", pvt_data.speed_accuracy);
    LOG_DBG("Heading:        %.01f deg", pvt_data.heading);
    LOG_DBG("Date:           %04u-%02u-%02u", pvt_data.datetime.year, pvt_data.datetime.month, pvt_data.datetime.day);
    LOG_DBG("Time (UTC):     %02u:%02u:%02u.%03u", pvt_data.datetime.hour, pvt_data.datetime.minute,
      pvt_data.datetime.seconds, pvt_data.datetime.ms);
    LOG_DBG("PDOP:           %.01f", pvt_data.pdop);
    LOG_DBG("HDOP:           %.01f", pvt_data.hdop);
    LOG_DBG("VDOP:           %.01f", pvt_data.vdop);
    LOG_DBG("TDOP:           %.01f", pvt_data.tdop);
}


/**
 * @brief Print the current stats when searching for GNSS position
 *
 */
static void print_pvt(void)
{
    /* Clear screen */
    LOG_DBG("\033[1;1H");
    LOG_DBG("\033[2J");

    if (pvt_data.sv[0].sv == 0) {
        LOG_DBG("No tracked satellites");
        return;
    }

    uint8_t tracked = 0;
    uint8_t in_fix = 0;
    uint8_t unhealthy = 0;
    static uint8_t cnt = 0;

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

    LOG_DBG("Tracking: %2d Using: %2d Unhealthy: %d", tracked, in_fix, unhealthy);
    LOG_DBG("Searching [%c]", update_indicator[cnt % 4]);
    cnt++;
} /* print_pvt */


#endif /* ifndef CONFIG_SMS */


/*************************************************************/
/* Threads */
/*************************************************************/

/* Handle events from the application */
static void gnss_application_event_thread(void)
{
    int retval = 0;
    uint32_t events = 0;

    retval = gnss_module_init();
    if (0 != retval) {
        return;
    }

    k_event_post(&app_events, APP_EVENT_GNSS_INITIALIZED);
    k_event_wait(&app_events, APP_EVENT_APPLICATION_INITIALIZED, 0, K_FOREVER);

    while (1) {
        events = k_event_wait(&app_events, -1, 0, K_FOREVER);

        if (events & APP_EVENT_GNSS_SEARCH_REQ) {
            /* Only start seaching for position if it is not already searching */
            if (0 == (events & APP_EVENT_GNSS_SEARCHING)) {
                gnss_start_search();
            }
        }

        if (events & APP_EVENT_GNSS_STOP) {
            nrf_modem_gnss_stop();
            k_event_set_masked(&app_events, 0, ~(APP_EVENT_GNSS_STOP | APP_EVENT_GNSS_SEARCHING));
        }

        if (events & APP_EVENT_GNSS_POSITION_FIXED) {
#ifndef CONFIG_SMS
            print_fix_data();
            print_battery_voltage();
#endif
        }
        k_msleep(CONFIG_POSITIONING_THREAD_SLEEP);
    }
} /* gnss_application_event_thread */


#define GNSS_APPLICATION_EVENT_THREAD_STACK_SIZE    4096
#define GNSS_APPLICATION_EVENT_THREAD_PRIORITY      7

K_THREAD_DEFINE(gnss_application_event_thread_id, GNSS_APPLICATION_EVENT_THREAD_STACK_SIZE,
  gnss_application_event_thread, NULL, NULL, NULL,
  K_PRIO_PREEMPT(GNSS_APPLICATION_EVENT_THREAD_PRIORITY), 0, 0);

/* Handle events from the GNSS driver */
static void gnss_event_thread(void)
{
    int retval = 0;
    int event = 0;
    static bool gnss_fixed = false;

    k_event_wait(&app_events, APP_EVENT_GNSS_INITIALIZED, 0, K_FOREVER);

    LOG_INF("GNSS initialized successfully!");

    while (1) {
        k_msgq_get(&event_msgq, &event, K_FOREVER);

        switch (event) {
            case NRF_MODEM_GNSS_EVT_PVT:
                retval = nrf_modem_gnss_read(&pvt_data, sizeof(struct nrf_modem_gnss_pvt_data_frame),
                    NRF_MODEM_GNSS_DATA_PVT);
                if (0 != retval) {
                    LOG_WRN("%s: Failed to read from the gnss modem!", __func__);
                    break;
                }
                if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
                    gnss_fixed = true;
                    k_event_set_masked(&app_events, APP_EVENT_GNSS_POSITION_FIXED, ~(APP_EVENT_GNSS_SEARCHING));
                } else {
#ifndef CONFIG_SMS
                    print_pvt();
#endif
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
                LOG_INF("%s: GNSS timeout!", __func__);
                k_event_set_masked(&app_events, 0, ~APP_EVENT_GNSS_SEARCHING);
                break;
            default:
                break;
        }
        k_msleep(CONFIG_POSITIONING_THREAD_SLEEP);
    }
} /* gnss_event_thread */


#define GNSS_EVENT_THREAD_STACK_SIZE    4096
#define GNSS_EVENT_THREAD_PRIORITY      7

K_THREAD_DEFINE(gnss_event_thread_id, GNSS_EVENT_THREAD_STACK_SIZE,
  gnss_event_thread, NULL, NULL, NULL,
  K_PRIO_PREEMPT(GNSS_EVENT_THREAD_PRIORITY), 0, 0);
