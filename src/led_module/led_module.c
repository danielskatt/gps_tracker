#include <zephyr.h>
#include "drivers/led/led.h"

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
 * @brief
 *
 * @param msg
 */
static void message_handler(struct led_msg_data *msg)
{
    ;
}


/**
 * @brief
 *
 */
static void led_module_thread(void)
{
    int retval = 0;

    retval = led_init();
    if (0 != retval) {
        return;
    }

    while (true) {
        // k_msgq_get(&event_msgq, &event, K_FOREVER); // Get state GNSS_SEARCHING;
        // switch (event) {
        //     case GNSS_SEARCHING:
        //         if (0 == k_timer_remaining_ticks(&led_search_timer)) {
        //             k_timer_start(&led_search_timer, K_MSEC(1), K_MSEC(500));
        //         }
        //         break;
        //     case GNSS_IDLE:
        //         k_timer_stop(&led_search_timer);
        //         led_blue_state_set(0);
        //         led_green_state_set(1);
        //         k_msleep(1000);
        //         led_green_state_set(0);
        //         break;
        //     default:
        //         break;
        // }
        k_msleep(1000);
    }
}


#define LED_MODULE_THREAD_STACK_SIZE    2048
#define LED_MODULE_THREAD_PRIORITY      7

// K_THREAD_DEFINE(led_module_thread_id, LED_MODULE_THREAD_STACK_SIZE,
//   led_module_thread, NULL, NULL, NULL,
//   K_PRIO_PREEMPT(LED_MODULE_THREAD_PRIORITY), 0, 0);
