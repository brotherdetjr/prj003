#include <stdio.h>
#include <stdlib.h>
#include "scheduler.h"

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr); \
            exit(1); \
        } \
    } while (0)

static void test_empty(void)
{
    scheduler_t s;
    scheduler_init(&s);
    CHECK(s.count == 0);
    CHECK(scheduler_peek(&s) == NULL);
    scheduled_event_t ev;
    CHECK(scheduler_pop(&s, &ev) == -1);
}

static void test_single(void)
{
    scheduler_t s;
    scheduler_init(&s);
    CHECK(scheduler_add(&s, 1000, 7) == 0);
    CHECK(s.count == 1);
    const scheduled_event_t *p = scheduler_peek(&s);
    CHECK(p != NULL && p->fire_at_ms == 1000 && p->tag == 7);
    /* peek does not remove */
    CHECK(s.count == 1);
    scheduled_event_t ev;
    CHECK(scheduler_pop(&s, &ev) == 0);
    CHECK(ev.fire_at_ms == 1000 && ev.tag == 7);
    CHECK(s.count == 0);
    CHECK(scheduler_peek(&s) == NULL);
    CHECK(scheduler_pop(&s, &ev) == -1);
}

static void test_ordering(void)
{
    scheduler_t s;
    scheduler_init(&s);
    /* insert out of order */
    scheduler_add(&s, 300, 3);
    scheduler_add(&s, 100, 1);
    scheduler_add(&s, 200, 2);

    scheduled_event_t ev;
    scheduler_pop(&s, &ev); CHECK(ev.fire_at_ms == 100 && ev.tag == 1);
    scheduler_pop(&s, &ev); CHECK(ev.fire_at_ms == 200 && ev.tag == 2);
    scheduler_pop(&s, &ev); CHECK(ev.fire_at_ms == 300 && ev.tag == 3);
    CHECK(s.count == 0);
}

static void test_equal_timestamps(void)
{
    scheduler_t s;
    scheduler_init(&s);
    scheduler_add(&s, 100, 1);
    scheduler_add(&s, 100, 2);
    scheduler_add(&s, 100, 3);

    /* all fire at the same time; order among equals is unspecified */
    int seen[4] = {0};
    scheduled_event_t ev;
    for (int i = 0; i < 3; i++) {
        CHECK(scheduler_pop(&s, &ev) == 0);
        CHECK(ev.fire_at_ms == 100);
        CHECK(ev.tag >= 1 && ev.tag <= 3);
        seen[ev.tag]++;
    }
    CHECK(seen[1] == 1 && seen[2] == 1 && seen[3] == 1);
    CHECK(s.count == 0);
}

static void test_clear(void)
{
    scheduler_t s;
    scheduler_init(&s);
    scheduler_add(&s, 100, 0);
    scheduler_add(&s, 200, 0);
    scheduler_clear(&s);
    CHECK(s.count == 0);
    CHECK(scheduler_peek(&s) == NULL);
}

static void test_full(void)
{
    scheduler_t s;
    scheduler_init(&s);
    for (int i = 0; i < SCHEDULER_MAX_EVENTS; i++)
        CHECK(scheduler_add(&s, (uint64_t)i + 1, 0) == 0);
    CHECK(scheduler_add(&s, 999, 0) == -1);  /* full */
}

static void test_reverse_insert_ordering(void)
{
    /* insert in descending order, verify ascending pop order */
    scheduler_t s;
    scheduler_init(&s);
    for (int i = SCHEDULER_MAX_EVENTS; i >= 1; i--)
        scheduler_add(&s, (uint64_t)i * 10, 0);

    uint64_t prev = 0;
    scheduled_event_t ev;
    while (s.count > 0) {
        CHECK(scheduler_pop(&s, &ev) == 0);
        CHECK(ev.fire_at_ms > prev);
        prev = ev.fire_at_ms;
    }
}

int main(void)
{
    test_empty();                   puts("OK  empty");
    test_single();                  puts("OK  single");
    test_ordering();                puts("OK  ordering");
    test_equal_timestamps();        puts("OK  equal_timestamps");
    test_clear();                   puts("OK  clear");
    test_full();                    puts("OK  full");
    test_reverse_insert_ordering(); puts("OK  reverse_insert_ordering");
    puts("All tests passed.");
    return 0;
}
