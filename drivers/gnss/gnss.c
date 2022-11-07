#include <zephyr.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include <date_time.h>
#include <nrf_modem_at.h>
#include <nrf_modem_gnss.h>
#include <modem/lte_lc.h>
#include "gnss.h"

int gnss_set_power_saving_mode(void)
{
    return nrf_modem_gnss_power_mode_set(NRF_MODEM_GNSS_PSM_DUTY_CYCLING_POWER);
}


int gnss_unset_power_saving_mode(void)
{
    return nrf_modem_gnss_power_mode_set(NRF_MODEM_GNSS_PSM_DISABLED);
}


int gnss_stop(void)
{
    return nrf_modem_gnss_stop();
}


int gnss_start_search(void)
{
    int retval = 0;

    retval |= gnss_stop();

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

    printk("Starting searching... %d\n", retval);

    return retval;
}


int gnss_init(void)
{
    int retval = 0;

    retval = lte_lc_init();
    if (0 != retval) {
        printk("Failed to init LTE link controller\n");
        return retval;
    }

    retval = lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_GPS, LTE_LC_SYSTEM_MODE_PREFER_AUTO);
    if (0 != retval) {
        printk("Failed to activate GNSS system mode\n");
        return retval;
    }

    retval = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);
    if (0 != retval) {
        printk("Failed to activate GNSS functional mode\n");
        return retval;
    }

    printk("Init GNSS %d\n", retval);

    return retval;
} /* gnss_init */
