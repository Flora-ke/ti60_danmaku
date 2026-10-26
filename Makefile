CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror -pedantic
GFX_ROOT ?= $(if $(wildcard ../software/include/gfx_draw.h),../software,../bitblt-runtime)
CPPFLAGS += -Iinclude -I$(GFX_ROOT)/include
CORE = src/game.c src/backend_control.c
DRAW = src/render.c src/render_cpu.c assets/assets.c
GFX = $(GFX_ROOT)/src/error.c $(GFX_ROOT)/src/draw.c $(GFX_ROOT)/src/job.c $(GFX_ROOT)/backends/cpu.c
HW = src/hardware_assets.c src/hardware_render.c
G2D = $(GFX_ROOT)/src/cache_plan.c $(GFX_ROOT)/src/g2d_assets.c $(GFX_ROOT)/src/display.c $(GFX_ROOT)/src/frame_session.c $(GFX_ROOT)/backends/g2d_v3.c $(GFX_ROOT)/backends/g2d_stream.c $(GFX_ROOT)/backends/g2d_job.c
HW_TEST_SUPPORT = $(GFX_ROOT)/src/input.c $(GFX_ROOT)/src/input_text.c $(GFX_ROOT)/src/batch.c $(GFX_ROOT)/src/runtime_abi.c $(GFX_ROOT)/src/keys.c $(GFX_ROOT)/app/loop.c
HOST_RUNTIME = platform/gfx_host_runtime.c
GFX_HEADERS = $(wildcard $(GFX_ROOT)/include/*.h)
OUT = build
.PHONY: all test test-hardware headless preview board-check board clean help
all: headless
help:
	@echo "make test/headless/preview; shared graphics sources: GFX_ROOT=$(GFX_ROOT)"
	@echo "make board builds the matched RAM ELF; 1 CPU, 2 G2D, 3 same-scene comparison; no automatic board programming."
$(OUT):
	mkdir -p $(OUT)
headless: $(OUT)/danmaku-headless
$(OUT)/danmaku-headless: $(CORE) $(DRAW) $(GFX) $(HOST_RUNTIME) $(GFX_HEADERS) platform/host_main.c include/game.h include/render.h src/render_glyphs.h | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE) $(DRAW) $(GFX) $(HOST_RUNTIME) platform/host_main.c -o $@
preview: $(CORE) $(DRAW) $(GFX) $(HOST_RUNTIME) platform/host_main.c | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DGAME_HOST_SDL $(CORE) $(DRAW) $(GFX) $(HOST_RUNTIME) platform/host_main.c $$(pkg-config --cflags --libs sdl2) -o $(OUT)/danmaku-preview

test: | $(OUT)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/backend_control.c tests/test_backend_control.c -o $(OUT)/test-backend-control
	$(OUT)/test-backend-control
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE) tests/test_game.c -o $(OUT)/test-game
	$(OUT)/test-game
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE) $(DRAW) $(GFX) $(HOST_RUNTIME) tests/reference_render.c tests/test_render.c -o $(OUT)/test-render
	$(OUT)/test-render
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE) $(DRAW) $(GFX) $(HOST_RUNTIME) tests/test_commands.c -o $(OUT)/test-commands
	$(OUT)/test-commands
	$(CC) $(CPPFLAGS) -I$(GFX_ROOT)/src $(CFLAGS) $(CORE) $(DRAW) $(GFX) $(HOST_RUNTIME) tests/test_render_lifecycle.c -o $(OUT)/test-render-lifecycle
	$(OUT)/test-render-lifecycle
	$(CC) $(CPPFLAGS) $(CFLAGS) -DGFX_TEST_TRACK_ALLOC $(GFX) $(HOST_RUNTIME) tests/test_gfx_common.c -o $(OUT)/test-gfx-common
	$(OUT)/test-gfx-common
	$(CC) $(CPPFLAGS) $(CFLAGS) platform/board_port.c tests/test_board_uart.c -o $(OUT)/test-uart
	$(OUT)/test-uart
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE) assets/assets.c src/g2d_scene.c tests/test_g2d_scene.c -o $(OUT)/test-g2d-scene
	$(OUT)/test-g2d-scene
test: test-hardware
test-hardware: | $(OUT)
	$(CC) $(CPPFLAGS) -I$(GFX_ROOT)/app -I$(GFX_ROOT)/checks $(CFLAGS) $(CORE) $(DRAW) $(HW) $(GFX) $(G2D) $(HW_TEST_SUPPORT) tests/reference_render.c tests/test_hardware_render.c -o $(OUT)/test-hardware-render
	$(OUT)/test-hardware-render
board:
	python3 tools/build_sapphire.py --gfx-root "$(GFX_ROOT)"
# Existing game-specific G2D files remain as legacy source, not a new backend.
# Their former platform branch is preserved but must not be used for new A/B comparisons.
board-check:
	test -n "$(RISCV_CC)" && test -n "$(SOC_ROOT)"
	$(RISCV_CC) -march=rv32im_zicsr -mabi=ilp32 -std=gnu11 -ffreestanding -fsyntax-only -Wall -Wextra $(CPPFLAGS) -I$(SOC_ROOT)/bsp/efinix/EfxSapphireSoc/include -I$(SOC_ROOT)/software/standalone/driver $(CORE) $(DRAW) $(GFX) platform/board_port.c platform/sapphire_main.c $(GFX_ROOT)/platform/sapphire/runtime.c
	$(RISCV_CC) -march=rv32im_zicsr -mabi=ilp32 -std=gnu11 -ffreestanding -fsyntax-only -Wall -Wextra -DRAM_ONLY_DIAGNOSTIC $(CPPFLAGS) -I$(SOC_ROOT)/bsp/efinix/EfxSapphireSoc/include -I$(SOC_ROOT)/software/standalone/driver platform/sapphire_main.c
	$(RISCV_CC) -march=rv32im_zicsr -mabi=ilp32 -std=gnu11 -ffreestanding -fsyntax-only -Wall -Wextra $(CPPFLAGS) -I$(SOC_ROOT)/bsp/efinix/EfxSapphireSoc/include -I$(SOC_ROOT)/software/standalone/driver $(HW) $(G2D) platform/accelerated_main.c $(GFX_ROOT)/platform/sapphire/cache_publish.c
# No automatic clean in any build/test target.
clean:
	rm -rf $(OUT)
