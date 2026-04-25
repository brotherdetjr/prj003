#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#define SCHEDULER_MAX_EVENTS 64

typedef struct {
    uint64_t fire_at_ms;
    uint32_t tag; /* opaque; interpreted by the caller */
} scheduled_event_t;

typedef struct {
    scheduled_event_t heap[SCHEDULER_MAX_EVENTS];
    int count;
} scheduler_t;

void scheduler_init(scheduler_t *s);
void scheduler_clear(scheduler_t *s);

/* Add an event. Returns 0 on success, -1 if full. */
int scheduler_add(scheduler_t *s, uint64_t fire_at_ms, uint32_t tag);

/* Peek at the earliest scheduled event without removing it. NULL if empty. */
const scheduled_event_t *scheduler_peek(const scheduler_t *s);

/* Remove the earliest event into *out. Returns 0 on success, -1 if empty. */
int scheduler_pop(scheduler_t *s, scheduled_event_t *out);

#endif /* SCHEDULER_H */
