# Build the bundled audsrv sources with the installed PS2SDK toolchain.
ifndef PS2SDK
$(error Set PS2DEV and PS2SDK before building)
endif
IOP_TOOL_PREFIX ?= $(PS2DEV)/iop/bin/mipsel-none-elf-
IOP_BIN ?= audsrv.irx
IOP_OBJS_DIR ?= obj/
IOP_SRC_DIR := src/
IOP_INC_DIR := include/
IOP_INCS := -Isrc
IOP_DBGINFOFLAGS :=
IOP_OBJS := audsrv.o upsamplers.o hw.o rpc_server.o rpc_client.o common.o cdrom.o imports.o exports.o adpcm.o
.PHONY: all
all: $(IOP_BIN)
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
$(IOP_OBJS): $(wildcard src/*.h include/*.h) Makefile
