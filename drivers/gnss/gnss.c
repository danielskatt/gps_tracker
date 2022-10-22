#include "gnss.h"
#include <zephyr/kernel.h>
#include <stdio.h>
#include <date_time.h>
#include <nrf_modem_at.h>
#include <nrf_modem_gnss.h>

int gnss_set_power_saving_mode(void)
{
    return nrf_modem_gnss_power_mode_set(NRF_MODEM_GNSS_PSM_DUTY_CYCLING_POWER);
}


int gnss_unset_power_saving_mode(void)
{
    return nrf_modem_gnss_power_mode_set(NRF_MODEM_GNSS_PSM_DISABLED);
}


int gnss_start(void)
{
    return nrf_modem_gnss_start();
}


int gnss_stop(void)
{
    return nrf_modem_gnss_stop();
}


int gnss_init(void)
{
    int retval = 0;
    uint8_t system_mask;

    /* Only use the gps, not qzss */
    system_mask = NRF_MODEM_GNSS_SYSTEM_GPS_MASK;

    retval |= nrf_modem_gnss_fix_interval_set(1);
    retval |= nrf_modem_gnss_fix_retry_set(1);
    retval |= nrf_modem_gnss_system_mask_set(system_mask);

    return retval;
}
