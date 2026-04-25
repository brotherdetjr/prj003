#include "scheduler.h"
#include <stddef.h>

void scheduler_init(scheduler_t *s) { s->count = 0; }
void scheduler_clear(scheduler_t *s) { s->count = 0; }

static void sift_up(scheduler_t *s, int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (s->heap[parent].fire_at_ms > s->heap[i].fire_at_ms) {
            scheduled_event_t tmp = s->heap[parent];
            s->heap[parent] = s->heap[i];
            s->heap[i] = tmp;
            i = parent;
        } else {
            break;
        }
    }
}

static void sift_down(scheduler_t *s, int i)
{
    int n = s->count;
    for (;;) {
        int smallest = i;
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        if (l < n && s->heap[l].fire_at_ms < s->heap[smallest].fire_at_ms)
            smallest = l;
        if (r < n && s->heap[r].fire_at_ms < s->heap[smallest].fire_at_ms)
            smallest = r;
        if (smallest == i) break;
        scheduled_event_t tmp = s->heap[smallest];
        s->heap[smallest] = s->heap[i];
        s->heap[i] = tmp;
        i = smallest;
    }
}

int scheduler_add(scheduler_t *s, uint64_t fire_at_ms, uint32_t tag)
{
    if (s->count >= SCHEDULER_MAX_EVENTS) return -1;
    int i = s->count++;
    s->heap[i].fire_at_ms = fire_at_ms;
    s->heap[i].tag = tag;
    sift_up(s, i);
    return 0;
}

const scheduled_event_t *scheduler_peek(const scheduler_t *s)
{
    return s->count > 0 ? &s->heap[0] : NULL;
}

int scheduler_pop(scheduler_t *s, scheduled_event_t *out)
{
    if (s->count == 0) return -1;
    *out = s->heap[0];
    s->count--;
    if (s->count > 0) {
        s->heap[0] = s->heap[s->count];
        sift_down(s, 0);
    }
    return 0;
}
