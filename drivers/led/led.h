#ifndef LED_H
#define LED_H

/**
 * @brief
 *
 */
int led_init(void);

/**
 * @brief
 *
 * @param state
 */
int led_red_state_set(uint8_t state);

/**
 * @brief
 *
 * @return int
 */
int led_red_state_toggle(void);

/**
 * @brief
 *
 * @param state
 * @return int
 */
int led_green_state_set(uint8_t state);

/**
 * @brief
 *
 * @return int
 */
int led_green_state_toggle(void);

/**
 * @brief
 *
 * @param state
 * @return int
 */
int led_blue_state_set(uint8_t state);

/**
 * @brief
 *
 * @return int
 */
int led_blue_state_toggle(void);

#endif /* LED_H */
