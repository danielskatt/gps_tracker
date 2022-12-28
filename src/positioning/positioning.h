#ifndef POSITIONING_H
#define POSITIONING_H

/**
 * @brief
 *
 * @param latitude
 * @param longitude
 * @param altitude
 * @param accuracy
 * @param voltage_level
 * @return int
 */
int positioning_data_get(float *latitude, float *longitude, float *altitude, float *accuracy, int *voltage_level);

#endif /* POSITIONING_H */
