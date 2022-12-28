#include <zephyr.h>
#include "src/lib/common_events.h"
#include "drivers/led/led.h"

static bool led_searching = false;

static void led_search_work_fn(struct k_work *work)
{
    led_blue_state_toggle();
}


static K_WORK_DEFINE(led_search_work, led_search_work_fn);

static void led_search_timer_fn(struct k_timer *timer_id)
{
    k_work_submit(&led_search_work);
}


static K_TIMER_DEFINE(led_search_timer, led_search_timer_fn, NULL);

/**
 * @brief Function to flash green LED indicating system initialized
 *
 */
static void led_module_init_led_set(void)
{
    led_green_state_set(1);
    k_msleep(100);
    led_green_state_set(0);
    k_msleep(100);
    led_green_state_set(1);
    k_msleep(100);
    led_green_state_set(0);
}


/* Thread for handling LED based on application events */
static void led_module_thread(void)
{
    if (0 != led_init()) {
        return;
    }

    k_event_wait(&app_events, APP_EVENT_APPLICATION_INITIALIZED, 0, K_FOREVER);

    led_module_init_led_set();

    while (true) {
        if (0 < k_event_wait(&app_events, APP_EVENT_GNSS_SEARCHING, 0, K_NO_WAIT)) {
            if (0 == led_searching) {
                led_green_state_set(0);
                k_timer_start(&led_search_timer, K_MSEC(1000), K_MSEC(1000));
                led_searching = true;
            }
        }

        if (0 < k_event_wait(&app_events, APP_EVENT_GNSS_POSITION_FIXED, 0, K_NO_WAIT)) {
            k_timer_stop(&led_search_timer);
            led_searching = false;
            led_blue_state_set(0);
            led_green_state_set(1);
        }
        k_msleep(CONFIG_LED_MODULE_THREAD_SLEEP_MS);
    }
} /* led_module_thread */


#define LED_MODULE_THREAD_STACK_SIZE    1024
#define LED_MODULE_THREAD_PRIORITY      7

K_THREAD_DEFINE(led_module_thread_id, LED_MODULE_THREAD_STACK_SIZE,
  led_module_thread, NULL, NULL, NULL,
  K_PRIO_PREEMPT(LED_MODULE_THREAD_PRIORITY), 0, 0);
