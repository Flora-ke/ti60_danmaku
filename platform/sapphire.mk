# All outputs stay in game/build; vendor BSP, startup and hardware are read-only.
.DEFAULT_GOAL := all
ROOT := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/..)
GFX_ROOT ?= $(ROOT)/../bitblt-runtime
GFX_ROOT := $(realpath $(GFX_ROOT))
SOC_ROOT ?= $(ROOT)/../bitblt_soc-ysl-dev/embedded_sw/newSoC
SOC_ROOT := $(realpath $(SOC_ROOT))
ifeq ($(SOC_ROOT),)
$(error Matched SOC_ROOT is missing)
endif
ifndef RISCV_BIN
$(error Set RISCV_BIN to the existing bare-metal toolchain prefix)
endif
STANDALONE := $(SOC_ROOT)/software/standalone
BSP_PATH := $(SOC_ROOT)/bsp/efinix/EfxSapphireSoc
LDSCRIPT := $(ROOT)/platform/game_ddr.ld
OBJDIR := $(ROOT)/build/sapphire
PROJ_NAME := ti60_danmaku
TMPDIR := $(OBJDIR)/tmp
TMP := $(TMPDIR)
TEMP := $(TMPDIR)
export TMPDIR TMP TEMP
SRCS := $(ROOT)/src/game.c $(ROOT)/src/backend_control.c $(ROOT)/src/render.c \
 $(ROOT)/src/render_cpu.c $(ROOT)/src/hardware_assets.c $(ROOT)/src/hardware_render.c \
 $(ROOT)/assets/assets.c $(ROOT)/platform/board_port.c $(ROOT)/platform/accelerated_main.c \
 $(GFX_ROOT)/src/error.c $(GFX_ROOT)/src/draw.c $(GFX_ROOT)/src/job.c \
 $(GFX_ROOT)/src/cache_plan.c $(GFX_ROOT)/src/g2d_assets.c $(GFX_ROOT)/src/display.c \
 $(GFX_ROOT)/src/frame_session.c $(GFX_ROOT)/backends/cpu.c \
 $(GFX_ROOT)/backends/g2d_v3.c $(GFX_ROOT)/backends/g2d_stream.c $(GFX_ROOT)/backends/g2d_job.c \
 $(GFX_ROOT)/platform/sapphire/runtime.c $(GFX_ROOT)/platform/sapphire/heap.c \
 $(GFX_ROOT)/platform/sapphire/g2d_mmio.c $(GFX_ROOT)/platform/sapphire/display_mmio.c \
 $(GFX_ROOT)/platform/sapphire/cache_publish.c \
 $(STANDALONE)/common/start.S
CFLAGS += -std=gnu11 -Wall -Wextra -g -MMD -MP -I$(ROOT)/include -I$(GFX_ROOT)/include
include $(STANDALONE)/common/bsp.mk
include $(STANDALONE)/common/riscv64-unknown-elf.mk
NAMES := $(basename $(notdir $(SRCS)))
ifneq ($(words $(NAMES)),$(words $(sort $(NAMES))))
$(error Duplicate source names cannot be used with this vendor BSP)
endif
include $(STANDALONE)/common/standalone.mk
$(OBJS): | $(TMPDIR)
$(TMPDIR):
	@mkdir -p "$@"
