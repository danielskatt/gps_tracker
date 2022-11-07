#ifndef GNSS_H
#define GNSS_H

/**
 * @brief
 *
 * @return int
 */
int gnss_set_power_saving_mode(void);

/**
 * @brief
 *
 * @return int
 */
int gnss_unset_power_saving_mode(void);

/**
 * @brief
 *
 * @return int
 */
int gnss_stop(void);

/**
 * @brief
 *
 * @return int
 */
int gnss_start_search(void);

/**
 * @brief
 *
 * @return int
 */
int gnss_init(void);

#endif /* GNSS_H */
