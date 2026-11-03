VENDOR := vendor/smw
ENGINE := $(filter-out $(VENDOR)/src/tracing.c $(VENDOR)/src/util.c,$(wildcard $(VENDOR)/src/*.c))
HARDWARE := $(wildcard $(VENDOR)/src/snes/*.c)
CORE := $(ENGINE) $(HARDWARE) src/runtime.c src/runtime_helpers.c src/asset_index.c src/bus.c src/save_journal.c src/audio_queue.c src/widescreen.c src/localization.c src/super_system.c src/save_icon.c
CPPFLAGS := -Iinclude -I$(VENDOR) -I$(VENDOR)/src -I$(VENDOR)/src/snes
CFLAGS := -O2 -std=gnu11 -fno-strict-aliasing -fwrapv -ffunction-sections -fdata-sections -Wno-unknown-pragmas
BUILD := build/host
HOST_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(CORE) src/headless.c)
WIDE_GS_ENGINE := $(filter-out $(BUILD)/src/headless.o $(BUILD)/$(VENDOR)/src/snes/ppu.o $(BUILD)/$(VENDOR)/src/snes/ppu_old.o,$(HOST_OBJS))

.PHONY: all host ps2 test test-volume test-yoshi test-yoshi-gs clean
all: host
host: $(BUILD)/smw-headless $(BUILD)/menu-preview
$(BUILD)/menu-preview: src/menu.c src/menu_preview.c include/menu.h
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/menu.c src/menu_preview.c -o $@
$(BUILD)/smw-headless: $(HOST_OBJS)
	$(CC) $(CFLAGS) $^ -Wl,--gc-sections -lm -o $@
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
ps2:
	$(MAKE) -f Makefile.ps2
test: host
	python3 tests/test_assets.py
test-volume:
	python3 tests/test_audsrv_volume.py
test-water: $(BUILD)/test-water
	@mkdir -p $(BUILD)/v014-water
	$(BUILD)/test-water data/smw_assets.dat $(BUILD)/v014-water
$(BUILD)/test-water: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_water_math.c tests/native_fixture.h tests/reference/ppu_v013.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_water_math.c tests/reference/ppu_v013.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--wrap=ppu_runLine -Wl,--gc-sections -lm -o $@
test-water-gs: $(BUILD)/test-water-gs
	@mkdir -p $(BUILD)/v014-water-gs
	$(BUILD)/test-water-gs data/smw_assets.dat $(BUILD)/v014-water-gs
$(BUILD)/test-water-gs: $(WIDE_GS_ENGINE) tests/test_water_math.c tests/native_fixture.h tests/reference/ppu_v013.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_PS2_PIXEL_FORMAT tests/test_water_math.c tests/reference/ppu_v013.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c $(WIDE_GS_ENGINE) -Wl,--wrap=ppu_runLine -Wl,--gc-sections -lm -o $@
test-arcade: $(BUILD)/test-arcade
	@mkdir -p $(BUILD)/v013-fixtures
	$(BUILD)/test-arcade data/smw_assets.dat data/smw_super_system.dat data/smw_japanese.dat tests/reference/ss_program.dat $(BUILD)/v013-fixtures
$(BUILD)/ss-reference-cpu.o: tests/reference/ss_cpu.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/test-arcade: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) $(BUILD)/ss-reference-cpu.o tests/test_super_system.c tests/ss_reference.c tests/native_fixture.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_super_system.c tests/ss_reference.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) $(BUILD)/ss-reference-cpu.o -Wl,--gc-sections -lm -o $@
test-arcade-gs: $(BUILD)/test-arcade-gs
	@mkdir -p $(BUILD)/v013-fixtures-gs
	$(BUILD)/test-arcade-gs data/smw_assets.dat data/smw_super_system.dat data/smw_japanese.dat tests/reference/ss_program.dat $(BUILD)/v013-fixtures-gs
$(BUILD)/test-arcade-gs: $(WIDE_GS_ENGINE) $(BUILD)/ss-reference-cpu.o tests/test_super_system.c tests/ss_reference.c tests/native_fixture.h $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_PS2_PIXEL_FORMAT tests/test_super_system.c tests/ss_reference.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c $(WIDE_GS_ENGINE) $(BUILD)/ss-reference-cpu.o -Wl,--gc-sections -lm -o $@
test-polish: $(BUILD)/test-polish
	@mkdir -p build/host/v012-fixtures
	$(BUILD)/test-polish data/smw_assets.dat data/smw_japanese.dat build/host/v012-fixtures
$(BUILD)/test-polish: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_wide_polish.c tests/test_v010.c tests/native_fixture.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_wide_polish.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--gc-sections -lm -o $@
test-polish-gs: $(BUILD)/test-polish-gs
	@mkdir -p build/host/v012-fixtures-gs
	$(BUILD)/test-polish-gs data/smw_assets.dat data/smw_japanese.dat build/host/v012-fixtures-gs
$(BUILD)/test-polish-gs: $(WIDE_GS_ENGINE) tests/test_wide_polish.c tests/test_v010.c tests/native_fixture.h $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_PS2_PIXEL_FORMAT tests/test_wide_polish.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c $(WIDE_GS_ENGINE) -Wl,--gc-sections -lm -o $@
test-overworld: $(BUILD)/test-overworld
	@mkdir -p build/host/v011-fixtures
	$(BUILD)/test-overworld data/smw_assets.dat data/smw_japanese.dat build/host/v011-fixtures
$(BUILD)/test-overworld: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_overworld.c tests/test_v010.c tests/native_fixture.h tests/reference/ppu_v010.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_overworld.c tests/reference/ppu_v010.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--wrap=ppu_runLine -Wl,--gc-sections -lm -o $@
test-overworld-gs: $(BUILD)/test-overworld-gs
	@mkdir -p build/host/v011-fixtures-gs
	$(BUILD)/test-overworld-gs data/smw_assets.dat data/smw_japanese.dat build/host/v011-fixtures-gs
$(BUILD)/test-overworld-gs: $(WIDE_GS_ENGINE) tests/test_overworld.c tests/test_v010.c tests/native_fixture.h tests/reference/ppu_v010.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_PS2_PIXEL_FORMAT tests/test_overworld.c tests/reference/ppu_v010.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c $(WIDE_GS_ENGINE) -Wl,--wrap=ppu_runLine -Wl,--gc-sections -lm -o $@
test-v010: $(BUILD)/test-v010
	@mkdir -p build/host/v010-fixtures
	$(BUILD)/test-v010 data/smw_assets.dat data/smw_japanese.dat build/host/v010-fixtures
	python3 tests/test_save_icon.py
$(BUILD)/test-v010: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_v010.c tests/native_fixture.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_v010.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--gc-sections -lm -o $@
test-v010-gs: $(BUILD)/test-v010-gs
	@mkdir -p build/host/v010-fixtures-gs
	$(BUILD)/test-v010-gs data/smw_assets.dat data/smw_japanese.dat build/host/v010-fixtures-gs
$(BUILD)/test-v010-gs: $(WIDE_GS_ENGINE) tests/test_v010.c tests/native_fixture.h $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_PS2_PIXEL_FORMAT tests/test_v010.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c $(WIDE_GS_ENGINE) -Wl,--gc-sections -lm -o $@
test-wide: $(BUILD)/test-widescreen
	@mkdir -p build/host/wide-fixtures
	$(BUILD)/test-widescreen data/smw_assets.dat build/host/wide-fixtures
test-wide-reference: $(BUILD)/test-wide-reference
	$(BUILD)/test-wide-reference data/smw_assets.dat
$(BUILD)/test-wide-reference: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_widescreen.c tests/native_fixture.h tests/test_wide_reference.c tests/reference/ppu_v006.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_WIDE_COMPARE tests/test_widescreen.c tests/test_wide_reference.c tests/reference/ppu_v006.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--wrap=ppu_runLine -Wl,--gc-sections -lm -o $@
test-wide-reference-gs: $(BUILD)/test-wide-reference-gs
	$(BUILD)/test-wide-reference-gs data/smw_assets.dat
$(BUILD)/test-wide-reference-gs: $(WIDE_GS_ENGINE) tests/test_widescreen.c tests/native_fixture.h tests/test_wide_reference.c tests/reference/ppu_v006.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_WIDE_COMPARE -DSMW_PS2_PIXEL_FORMAT tests/test_widescreen.c tests/test_wide_reference.c tests/reference/ppu_v006.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c $(WIDE_GS_ENGINE) -Wl,--wrap=ppu_runLine -Wl,--gc-sections -lm -o $@
$(BUILD)/test-widescreen: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_widescreen.c tests/native_fixture.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_widescreen.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--gc-sections -lm -o $@
test-transitions: $(BUILD)/test-transitions
	$(BUILD)/test-transitions data/smw_assets.dat
$(BUILD)/test-transitions: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_transitions.c tests/native_fixture.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_transitions.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--gc-sections -lm -o $@
test-transitions-gs: $(BUILD)/test-transitions-gs
	$(BUILD)/test-transitions-gs data/smw_assets.dat
$(BUILD)/test-transitions-gs: $(WIDE_GS_ENGINE) tests/test_transitions.c tests/native_fixture.h $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_PS2_PIXEL_FORMAT tests/test_transitions.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c $(WIDE_GS_ENGINE) -Wl,--gc-sections -lm -o $@
test-platform: $(BUILD)/test-audio-pacer
	$(BUILD)/test-audio-pacer
	python3 tests/test_platform.py
$(BUILD)/test-audio-pacer: tests/test_audio_pacer.c src/audio_queue.c src/frame_pacer.c $(VENDOR)/src/snes/dsp.c include/audio_queue.h include/frame_pacer.h
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_audio_pacer.c src/audio_queue.c src/frame_pacer.c $(VENDOR)/src/snes/dsp.c -Wl,--gc-sections -o $@
test-ppu: $(BUILD)/test-ppu $(BUILD)/test-ppu-gs
	$(BUILD)/test-ppu
	$(BUILD)/test-ppu-gs
RASTER_TEST := tests/test_ppu.c tests/reference/ppu_reference.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
$(BUILD)/test-ppu: $(RASTER_TEST) $(VENDOR)/src/snes/ppu.h $(VENDOR)/src/snes/ppu_decode.h
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RASTER_TEST) -Wl,--gc-sections -o $@
$(BUILD)/test-ppu-gs: $(RASTER_TEST) $(VENDOR)/src/snes/ppu.h $(VENDOR)/src/snes/ppu_decode.h
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_PS2_PIXEL_FORMAT $(RASTER_TEST) -Wl,--gc-sections -o $@
clean:
	rm -rf build/host build/ee build/iop dist
-include $(HOST_OBJS:.o=.d)

test-v014: $(BUILD)/test-v014
	@mkdir -p $(BUILD)/v014-fixtures
	$(BUILD)/test-v014 data/smw_assets.dat data/smw_super_system.dat data/smw_japanese.dat $(BUILD)/v014-fixtures
$(BUILD)/test-v014: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_v014_reports.c tests/native_fixture.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_v014_reports.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--gc-sections -lm -o $@
test-v014-gs: $(BUILD)/test-v014-gs
	@mkdir -p $(BUILD)/v014-fixtures-gs
	$(BUILD)/test-v014-gs data/smw_assets.dat data/smw_super_system.dat data/smw_japanese.dat $(BUILD)/v014-fixtures-gs
$(BUILD)/test-v014-gs: $(WIDE_GS_ENGINE) tests/test_v014_reports.c tests/native_fixture.h $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_PS2_PIXEL_FORMAT tests/test_v014_reports.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c $(WIDE_GS_ENGINE) -Wl,--gc-sections -lm -o $@

test-yoshi: $(BUILD)/test-yoshi
	@mkdir -p $(BUILD)/v015-fixtures
	$(BUILD)/test-yoshi data/smw_assets.dat data/smw_super_system.dat data/smw_japanese.dat $(BUILD)/v015-fixtures
$(BUILD)/test-yoshi: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_yoshi_wide.c tests/test_v014_reports.c tests/native_fixture.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_yoshi_wide.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--gc-sections -lm -o $@
test-yoshi-gs: $(BUILD)/test-yoshi-gs
	@mkdir -p $(BUILD)/v015-fixtures-gs
	$(BUILD)/test-yoshi-gs data/smw_assets.dat data/smw_super_system.dat data/smw_japanese.dat $(BUILD)/v015-fixtures-gs
$(BUILD)/test-yoshi-gs: $(WIDE_GS_ENGINE) tests/test_yoshi_wide.c tests/test_v014_reports.c tests/native_fixture.h $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSMW_PS2_PIXEL_FORMAT tests/test_yoshi_wide.c $(VENDOR)/src/snes/ppu.c $(VENDOR)/src/snes/ppu_old.c $(WIDE_GS_ENGINE) -Wl,--gc-sections -lm -o $@

$(BUILD)/test-rooms: $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) tests/test_rooms.c tests/test_v014_reports.c tests/native_fixture.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_rooms.c $(filter-out $(BUILD)/src/headless.o,$(HOST_OBJS)) -Wl,--gc-sections -lm -o $@
