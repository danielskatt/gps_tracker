#ifndef COMMON_EVENTS_H
#define COMMON_EVENTS_H

#include <zephyr.h>

typedef enum {
    APP_EVENT_GNSS_INITIALIZED        = 1 << 0,
    APP_EVENT_GNSS_SEARCH_REQ         = 1 << 1,
    APP_EVENT_GNSS_SEARCHING          = 1 << 2,
    APP_EVENT_GNSS_STOP               = 1 << 3,
    APP_EVENT_GNSS_POSITION_FIXED     = 1 << 4,
    APP_EVENT_SMS_INITIALIZED         = 1 << 5,
    APP_EVENT_SMS_LOG_SEND            = 1 << 6,
    APP_EVENT_MOVEMENT_TRIGGERED      = 1 << 7,
    APP_EVENT_APPLICATION_INITIALIZED = 1 << 8,
} app_events_t;

extern struct k_event app_events;

/**
 * @brief Set or clear the events in an event object.
 *
 * This routine sets the events stored in event object to the specified value. All tasks waiting on the event object
 *   event whose waiting conditions become met by this immediately unpend. Unlike k_event_set, this routine allows
 *   specific event bits to be set and cleared as determined by the mask.
 * @param event address of the event object
 * @param events events to set
 * @param events_mask events to keep
 */
void k_event_set_masked(struct k_event *event, uint32_t events, uint32_t events_mask);

#endif /* COMMON_EVENTS_H */
