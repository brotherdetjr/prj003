CC       = gcc
CFLAGS   = -Wall -Wextra -std=gnu11 -g \
           -DMG_ENABLE_MBEDTLS=0 \
           -I../../vendor/mongoose \
           -I../../vendor/cjson

# Vendor files compiled without -Wall to avoid noise from third-party code
VCFLAGS  = -std=gnu11 -g -DMG_ENABLE_MBEDTLS=0 \
           -I../../vendor/mongoose

TARGET   = emu

APP_SRCS = main.c server.c peer.c state.c \
           ../../core/world.c ../../core/character.c ../../core/scheduler.c

APP_OBJS = $(notdir $(APP_SRCS:.c=.o))

VENDOR_OBJS = mongoose.o cJSON.o

all: $(TARGET)

$(TARGET): $(APP_OBJS) $(VENDOR_OBJS)
	$(CC) -o $@ $^

# App object rules (with relative paths resolved)
main.o:      main.c      ; $(CC) $(CFLAGS) -c -o $@ $<
server.o:    server.c    ; $(CC) $(CFLAGS) -c -o $@ $<
peer.o:      peer.c      ; $(CC) $(CFLAGS) -c -o $@ $<
state.o:     state.c     ; $(CC) $(CFLAGS) -c -o $@ $<
world.o:     ../../core/world.c     ; $(CC) $(CFLAGS) -c -o $@ $<
character.o: ../../core/character.c ; $(CC) $(CFLAGS) -c -o $@ $<
scheduler.o: ../../core/scheduler.c ; $(CC) $(CFLAGS) -c -o $@ $<

# Vendor object rules (warnings suppressed)
mongoose.o: ../../vendor/mongoose/mongoose.c
	$(CC) $(VCFLAGS) -c -o $@ $<

cJSON.o: ../../vendor/cjson/cJSON.c
	$(CC) $(VCFLAGS) -c -o $@ $<

test_scheduler: ../../core/test_scheduler.c ../../core/scheduler.c
	$(CC) $(CFLAGS) -o $@ $^

test: test_scheduler
	./test_scheduler

clean:
	rm -f $(TARGET) test_scheduler *.o

.PHONY: all test clean
