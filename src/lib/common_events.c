#include <zephyr.h>

K_EVENT_DEFINE(app_events);

void k_event_set_masked(
    struct k_event *event,
    uint32_t events,
    uint32_t events_mask)
{
    uint32_t curr_events = k_event_wait(event, -1, 0, K_NO_WAIT);

    curr_events &= events_mask;
    events |= curr_events;
    k_event_set(event, events);
}
