CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude
CORE = src/game.c
DRAW = src/render.c assets/assets.c
OUT = build

.PHONY: all test headless preview board-check clean help
all: headless

help:
	@echo "make test        编译并运行三组单元测试"
	@echo "make headless    构建无窗口版 build/danmaku-headless"
	@echo "make preview     构建 SDL2 预览版 build/danmaku-preview"
	@echo "make board-check 仅做 RV32 源码语法检查（需 RISCV_CC 与 SOC_ROOT）"
	@echo "make clean       删除 build/ 及全部中间产物"

$(OUT):
	mkdir -p $(OUT)

headless: $(OUT)/danmaku-headless
$(OUT)/danmaku-headless: $(CORE) $(DRAW) platform/host_main.c include/game.h include/assets.h include/render.h | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE) $(DRAW) platform/host_main.c -o $@

preview: $(CORE) $(DRAW) platform/host_main.c | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DGAME_HOST_SDL $(CORE) $(DRAW) platform/host_main.c $$(pkg-config --cflags --libs sdl2) -o $(OUT)/danmaku-preview

test: | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE) tests/test_game.c -o $(OUT)/test-game
	$(OUT)/test-game
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE) $(DRAW) tests/test_render.c -o $(OUT)/test-render
	$(OUT)/test-render
	$(CC) $(CPPFLAGS) $(CFLAGS) platform/board_port.c tests/test_board_uart.c -o $(OUT)/test-uart
	$(OUT)/test-uart

# Checks only C compilation. It does not link ELF, load hardware, or claim video works.
board-check:
	test -n "$(RISCV_CC)" && test -n "$(SOC_ROOT)"
	$(RISCV_CC) -march=rv32im_zicsr -mabi=ilp32 -std=gnu11 -ffreestanding -fsyntax-only -Wall -Wextra -Iinclude -I$(SOC_ROOT)/bsp/efinix/EfxSapphireSoc/include -I$(SOC_ROOT)/software/standalone/driver $(CORE) $(DRAW) platform/board_port.c platform/sapphire_main.c
	$(RISCV_CC) -march=rv32im_zicsr -mabi=ilp32 -std=gnu11 -ffreestanding -fsyntax-only -Wall -Wextra -DRAM_ONLY_DIAGNOSTIC -Iinclude -I$(SOC_ROOT)/bsp/efinix/EfxSapphireSoc/include -I$(SOC_ROOT)/software/standalone/driver platform/sapphire_main.c

# Removes every generated artifact. No source, docs or assets are touched.
clean:
	rm -rf $(OUT)
