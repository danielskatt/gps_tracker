#include <zephyr.h>
#include <drivers/gpio.h>
#include "led.h"

static struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

int led_init(void)
{
    int retval = 0;

    retval |= gpio_pin_configure_dt(&red_led, GPIO_OUTPUT);
    retval |= gpio_pin_configure_dt(&green_led, GPIO_OUTPUT);
    retval |= gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT);

    return retval;
}


int led_red_state_set(uint8_t state)
{
    return gpio_pin_set_dt(&red_led, state);
}


int led_red_state_toggle(void)
{
    return gpio_pin_toggle_dt(&red_led);
}


int led_green_state_set(uint8_t state)
{
    return gpio_pin_set_dt(&green_led, state);
}


int led_green_state_toggle(void)
{
    return gpio_pin_toggle_dt(&green_led);
}


int led_blue_state_set(uint8_t state)
{
    return gpio_pin_set_dt(&blue_led, state);
}


int led_blue_state_toggle(void)
{
    return gpio_pin_toggle_dt(&blue_led);
}
