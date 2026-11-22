PROJECT_NAME = tb

ifndef CC
CC := clang
endif

# build mode
ifndef BUILD
BUILD := release
endif

# build flags
ifeq ($(BUILD)",debug)
$(info - building in DEBUG mode)
CFLAGS += -g3 -O0 -DDEBUG
else
$(info - building in RELEASE mode)
CFLAGS += -O3 -DNDEBUG
endif

# more warnings
CFLAGS += -Wall -Wextra -Werror -Wpedantic --ansi -Wconversion -Wno-unused-but-set-parameter -Wno-incompatible-function-pointer-types -Wno-incompatible-pointer-types -Wno-unused-result -Wno-unknown-warning-option

CFLAGS += -D__linux__

$(PROJECT_NAME): main.c
	@mkdir -p build
	$(CC) main.c -o build/$(PROJECT_NAME)

