CC       = gcc
CFLAGS   = -Wall -Wextra -std=gnu11 -g \
           -DMG_ENABLE_MBEDTLS=0 \
           -I../../vendor/mongoose \
           -I../../vendor/cjson \
           -I../../vendor/lua

# Vendor files compiled without -Wall to avoid noise from third-party code
VCFLAGS  = -std=gnu11 -g -DMG_ENABLE_MBEDTLS=0 \
           -I../../vendor/mongoose \
           -I../../vendor/lua

TARGET   = emu

APP_SRCS = main.c server.c peer.c state.c \
           ../../common/app.c ../../common/lua_bind.c \
           ../../common/character.c ../../common/scheduler.c

APP_OBJS = $(notdir $(APP_SRCS:.c=.o))

# Headers that, when changed, require a full rebuild of all app objects
APP_HDRS = ../../common/app.h ../../common/lua_bind.h \
           ../../common/character.h ../../common/scheduler.h

LUA_SRCS = $(wildcard ../../vendor/lua/*.c)
LUA_OBJS = $(notdir $(LUA_SRCS:.c=.o))

VENDOR_OBJS = mongoose.o cJSON.o $(LUA_OBJS)

all: $(TARGET)

$(TARGET): $(APP_OBJS) $(VENDOR_OBJS)
	$(CC) -o $@ $^ -lm

# App object rules (with relative paths resolved)
# All app objects depend on APP_HDRS so any header change triggers a rebuild
main.o:      main.c      $(APP_HDRS) ; $(CC) $(CFLAGS) -c -o $@ $<
server.o:    server.c    $(APP_HDRS) ; $(CC) $(CFLAGS) -c -o $@ $<
peer.o:      peer.c      $(APP_HDRS) ; $(CC) $(CFLAGS) -c -o $@ $<
state.o:     state.c     $(APP_HDRS) ; $(CC) $(CFLAGS) -c -o $@ $<
app.o:       ../../common/app.c       $(APP_HDRS) ; $(CC) $(CFLAGS) -c -o $@ $<
lua_bind.o:  ../../common/lua_bind.c  $(APP_HDRS) ; $(CC) $(CFLAGS) -c -o $@ $<
character.o: ../../common/character.c $(APP_HDRS) ; $(CC) $(CFLAGS) -c -o $@ $<
scheduler.o: ../../common/scheduler.c $(APP_HDRS) ; $(CC) $(CFLAGS) -c -o $@ $<

# Vendor object rules (warnings suppressed)
mongoose.o: ../../vendor/mongoose/mongoose.c
	$(CC) $(VCFLAGS) -c -o $@ $<

cJSON.o: ../../vendor/cjson/cJSON.c
	$(CC) $(VCFLAGS) -c -o $@ $<

# Lua library sources — compile each .c in vendor/lua/
define LUA_RULE
$(notdir $(1:.c=.o)): $(1)
	$(CC) $(VCFLAGS) -c -o $$@ $$<
endef
$(foreach src,$(LUA_SRCS),$(eval $(call LUA_RULE,$(src))))

test_scheduler: ../../common/test_scheduler.c ../../common/scheduler.c
	$(CC) $(CFLAGS) -o $@ $^

test: test_scheduler
	./test_scheduler

clean:
	rm -f $(TARGET) test_scheduler *.o

.PHONY: all test clean
