#ifndef LED_H
#define LED_H

/**
 * @brief Init the leds (set the pins to output)
 *
 */
int led_init(void);

/**
 * @brief Set the red LED to wanted state
 *
 * @param state 0 for OFF, 1 for ON
 * @return int 0 on sucess, negative on fail
 */
int led_red_state_set(uint8_t state);

/**
 * @brief Toggle the state of the red LED
 *
 * @return int 0 on sucess, negative on fail
 */
int led_red_state_toggle(void);

/**
 * @brief Set the green LED to wanted state
 *
 * @param state 0 for OFF, 1 for ON
 * @return int 0 on sucess, negative on fail
 */
int led_green_state_set(uint8_t state);

/**
 * @brief Toggle the state of the green LED
 *
 * @return int 0 on sucess, negative on fail
 */
int led_green_state_toggle(void);

/**
 * @brief Set the blue LED to wanted state
 *
 * @param state 0 for OFF, 1 for ON
 * @return int 0 on sucess, negative on fail
 */
int led_blue_state_set(uint8_t state);

/**
 * @brief Toggle the state of the blue LED
 *
 * @return int 0 on sucess, negative on fail
 */
int led_blue_state_toggle(void);

#endif /* LED_H */
