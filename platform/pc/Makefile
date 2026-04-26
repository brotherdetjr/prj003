CC       = gcc
CFLAGS   = -Wall -Wextra -std=gnu11 -Os -ffunction-sections -fdata-sections \
           -DMG_ENABLE_MBEDTLS=0 \
           -I../../vendor/mongoose \
           -I../../vendor/cjson \
           -I../../vendor/lua

# Vendor files compiled without -Wall to avoid noise from third-party code
VCFLAGS  = -std=gnu11 -Os -ffunction-sections -fdata-sections \
           -DMG_ENABLE_MBEDTLS=0 \
           -I../../vendor/mongoose \
           -I../../vendor/lua

# LUA_USE_POSIX makes lua_tmpnam use mkstemp instead of the deprecated tmpnam
LUA_VCFLAGS = $(VCFLAGS) -DLUA_USE_POSIX

TARGET   = emu

APP_SRCS = main.c peer.c \
           ../../common/app.c ../../common/lua_bind.c \
           ../../common/server.c ../../common/state.c \
           ../../common/character.c ../../common/scheduler.c

APP_OBJS = $(notdir $(APP_SRCS:.c=.o))

# Headers that, when changed, require a full rebuild of all app objects
APP_HDRS = ../../common/app.h ../../common/lua_bind.h \
           ../../common/server.h ../../common/state.h \
           ../../common/character.h ../../common/scheduler.h

LUA_SRCS = $(wildcard ../../vendor/lua/*.c)
LUA_OBJS = $(notdir $(LUA_SRCS:.c=.o))

VENDOR_OBJS = mongoose.o cJSON.o $(LUA_OBJS)

# All sources in one list for single-compilation targets (emu-asan)
ALL_SRCS = $(APP_SRCS) $(LUA_SRCS) \
           ../../vendor/mongoose/mongoose.c ../../vendor/cjson/cJSON.c

ASAN_FLAGS = -std=gnu11 -O1 -g -fno-omit-frame-pointer \
             -fsanitize=address,undefined \
             -DMG_ENABLE_MBEDTLS=0 -DLUA_USE_POSIX \
             -I../../vendor/mongoose -I../../vendor/cjson -I../../vendor/lua

ASAN_LOG = /tmp/emu-asan

all: format-check test_scheduler emu-asan
	./test_scheduler
	rm -f $(ASAN_LOG).*
	cd ../../tests && EMU_BIN=$(CURDIR)/emu-asan \
	    ASAN_OPTIONS=detect_leaks=1:log_path=$(ASAN_LOG) \
	    python3 -m behave
	@if ls $(ASAN_LOG).* 2>/dev/null; then \
	    echo "ASan errors detected — see $(ASAN_LOG).<pid>"; \
	    exit 1; \
	fi

distr: $(APP_OBJS) $(VENDOR_OBJS)
	$(CC) -o $(TARGET) $^ -lm -Wl,--gc-sections -s
	cd ../../tests && python3 -m behave

# App object rules — auto-generated from APP_SRCS
# All app objects depend on APP_HDRS so any header change triggers a rebuild
define APP_RULE
$(notdir $(1:.c=.o)): $(1) $(APP_HDRS)
	$(CC) $(CFLAGS) -c -o $$@ $$<
endef
$(foreach src,$(APP_SRCS),$(eval $(call APP_RULE,$(src))))

# Vendor object rules (warnings suppressed)
mongoose.o: ../../vendor/mongoose/mongoose.c
	$(CC) $(VCFLAGS) -c -o $@ $<

cJSON.o: ../../vendor/cjson/cJSON.c
	$(CC) $(VCFLAGS) -c -o $@ $<

# Lua library sources — compile each .c in vendor/lua/
define LUA_RULE
$(notdir $(1:.c=.o)): $(1)
	$(CC) $(LUA_VCFLAGS) -c -o $$@ $$<
endef
$(foreach src,$(LUA_SRCS),$(eval $(call LUA_RULE,$(src))))

test_scheduler: ../../common/test_scheduler.c ../../common/scheduler.c
	$(CC) $(CFLAGS) -o $@ $^

# Single-compilation ASan build — vendor warnings are acceptable in a debug build
emu-asan: $(ALL_SRCS) $(APP_HDRS)
	$(CC) $(ASAN_FLAGS) -o $@ $(ALL_SRCS) -lm

clean:
	rm -f $(TARGET) emu-asan test_scheduler *.o

FORMAT_SRCS = $(wildcard *.c *.h) $(wildcard ../../common/*.c ../../common/*.h)

format-check:
	clang-format --dry-run --Werror $(FORMAT_SRCS)

format:
	clang-format -i $(FORMAT_SRCS)

.PHONY: all distr clean format-check format
