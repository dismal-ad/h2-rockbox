# Rockbox `newui` fork — HiFi Walker H2

A stripped-down Rockbox tree for one player, the **HiFi Walker H2** (a rebrand of
the AIGO Eros Q native; Ingenic X1000, MIPS), used as the base for a complete UI
overhaul. Branch: `newui` (forked from upstream commit `212bb63ee4`).

## Device

- Target `erosqnative` (configure target 247), one firmware for all hardware
  revisions; the firmware reads the revision from the bootloader at runtime.
  The owner's unit is **hw4**.
- The official Rockbox bootloader is used. **This tree cannot build bootloaders.**
- LCD 320×240, 16 bpp colour. 32 MB RAM. 2 MiB plugin buffer (used as scratch
  memory by core code). No touchscreen.
- Buttons: POWER, MENU, BACK, PLAY, PREV/NEXT, VOL_UP/VOL_DOWN, and a scroll
  wheel (`BUTTON_SCROLL_BACK`/`BUTTON_SCROLL_FWD`). Keymap: `apps/keymaps/keymap-erosq.c`.

## Build

Toolchain (MIPS cross-compiler) is installed in `~/rockbox-toolchain`.

```bash
export PATH=$HOME/rockbox-toolchain/bin:$PATH

# firmware (outside the source tree, e.g. ../build-erosq)
../src/tools/configure --target=erosqnative --type=n && make -j8 && make zip

# simulator (../build-sim); its virtual SD card is simdisk/ (holds test music)
../src/tools/configure --target=erosqnative --type=s && make -j8 && make install && ./rockboxui
```

Other build types still supported: `--type=c` (checkwps), `--type=w` (warble
codec test tool), `--type=a<opts>` advanced builds (`l` logf, `i` logf to
serial, `d` debug, `u` USB serial, `m` small C lib, `x` LTO). Profiling (`p`)
does not build on MIPS (also broken upstream).

Simulator keys: arrows = scroll wheel / prev / next, Enter = play/select,
Backspace = back, Insert = menu, PgUp/PgDn = volume, Esc = power.

Install on the player: unzip `rockbox.zip` to the SD card root (it contains
`.rockbox/`). Remove the old `.rockbox` first, since unzip does not delete stale files.

## Verifying changes

A change is good when **all four** fresh builds pass: firmware, simulator,
checkwps, warble (plus a logf build for debug-path changes). For pure cleanups,
`mipsel-elf-size rockbox.elf` and the `.codec` sizes must be unchanged. Only
the version string may differ.

## What was removed from upstream (and must not come back by accident)

- All other targets, CPUs (ARM, ColdFire, other Ingenic), their drivers, configs
  and keymaps; bootloader code and tools; manual, android, utils.
- Dead preprocessor branches for features the H2 lacks (touchscreen, remote
  LCD, recording, FM radio, S/PDIF, line-in/mic, button lights, iPod accessory...).
- Codecs: game music (SID, NSF, SPC, SAP/ASAP, AY, VTX, GBS, HES, SGC, VGM,
  KSS), MOD, RealMedia, ATRAC3, AC3, Musepack, TTA, WMA Voice, Speex, ADX.
  Kept: MP1/2/3, AAC/HE-AAC, ALAC, FLAC, Vorbis, Opus, WAV/W64/AIFF, APE,
  WavPack, WMA/WMA Pro, Shorten, DSD, AU, VOX, SMAF.
- Voice (spoken menus): `apps/talk.h` and `apps/voice_thread.h` are **inline
  no-op stubs**; call sites remain and can be deleted as screens are rewritten.
- Plugins: no plugins and no loader. `plugin_load()` always fails;
  `plugin_get_buffer()` still returns the 2 MiB scratch buffer.

Known leftovers that still try to launch plugins (show "Plugin error"): Plugins
browser (`GO_TO_BROWSEPLUGINS` in `apps/root_menu.c`, `apps/menus/plugin_menu.c`),
"Open with…", Properties, PictureFlow, credits/licence viewer, pitch screen,
playing time, image viewer, DB folder select, main menu config, open-plugin
hotkey. Remove or replace these during the overhaul.

## Bluetooth (branch `newui-bt`)

Ported from https://github.com/thiagoelg/h2-rockbox-bt (branch `h2-bt`, git
remote `h2bt`): the 19 code commits from `4e3333fd06` (Skye's Bluedroid port)
through `bde005189c` (Thiago Lugli's manager/menu/settings), cherry-picked with
`-x`. Their README, GitHub workflow and manual commits were not taken.

- Bluedroid (ESP-IDF 6.0.1 Bluetooth stack) in `lib/bluedroid/`: ~200k code
  lines; A2DP source with SBC, AVRCP. Host side only: the H2's Broadcom
  BCM4343A1 is driven over UART (HCI H4) by
  `firmware/target/mips/ingenic_x1000/erosqnative/hci-erosqnative.c`, with its
  patchram firmware in `BCM4343A1_*.c`. **hw4 only.**
- `HAVE_BLUETOOTH` and `BT_HEAP_SIZE` (256 KiB, TLSF heap) are in
  `config/erosqnative.h`. Audio goes out through a second PCM sink,
  `PCM_SINK_BLUETOOTH`.
- App side: `apps/bluetooth/bluetooth.c` (manager thread, paired devices in
  `/.rockbox/bt_devices.txt`), `apps/menus/bluetooth_menu.c`
  (Settings > Bluetooth), settings `bt_enabled` (default off),
  `bt_high_quality`, `bt_volume_sync`. With `BT_DEBUG` it logs to `/bt_log.txt`.
- Simulator: `uisimulator/common/hci-sim.c` talks to a host serial HCI device.
- Porting notes: `uart_rx_timeout()` was re-applied by hand on top of
  upstream's DMA UART TX driver (`af9a81f5f6`), which their branch predates.
  That driver keeps one set of state for all UARTs, so only one UART can be in
  use at a time.
- Cost: firmware text +425 KB, `rockbox.zip` 2.4 → 2.7 MB. Most of Bluedroid
  sits behind its own feature switches (`sdkconfig.h`, BLE/GATT/other
  profiles), which are candidates for the same dead-code carving.

## UI overhaul: decisions

1. **Code-drawn UI, no themes.** The new UI is drawn directly in C. The skin
   engine is to be removed **once the new screens replace it** (the current
   UI still depends on it): `apps/gui/skin_engine/`, `lib/skin_parser/`,
   `apps/gui/statusbar-skinned.c`, `apps/gui/wps.c` (Now Playing),
   `tools/checkwps/`, `wps/`, `backdrops/`, theme settings and the `--type=c`
   build. Until then, keep checkwps building.
2. **Keep the music database and add iPod-style Cover Flow** album browsing
   (album covers that slide/scroll with the wheel). The database format may be
   changed for this.
   - Database: `apps/tagcache.c/.h` (on-disk format, `TAGCACHE_MAGIC`, files
     `database_*.tcd`), `apps/tagtree.c` + `apps/tagnavi.config` (browse menus).
     Any on-disk format change **must bump `TAGCACHE_MAGIC`** so old databases
     are rebuilt instead of misread.
   - Album art: `apps/recorder/albumart.c` (finds cover files),
     `apps/recorder/jpeg_load.c`, `bmp.c`, `resize.c` (decode + scale).
     `HAVE_ALBUMART` and `HAVE_JPEG` are enabled.
   - Decoding JPEGs while scrolling is too slow on the X1000; plan for
     pre-scaled cover thumbnails cached on disk (e.g. built alongside the
     database) and a small in-RAM cache for the covers on screen.
   - Upstream's PictureFlow plugin implements this effect and is a useful
     reference: `git show 212bb63ee4:apps/plugins/pictureflow/pictureflow.c`.
3. Layout for 320×240 driven by the scroll wheel; build and check every
   screen in the simulator first.

## Where the UI code lives

- `apps/root_menu.c`: top-level screen switching and the start screen
- `apps/menus/`, `apps/menu.c`: settings menus
- `apps/gui/list.c`, `line.c`, `viewport.c`, `scrollbar.c`, `icon.c`: list and
  drawing primitives
- `apps/tree.c`, `apps/filetree.c`: file browser; `apps/tagtree.c`: database browser
- `apps/gui/wps.c` + skin engine: Now Playing; `apps/onplay.c`: context menu
- `apps/settings_list.c`: all settings definitions
- `firmware/drivers/lcd-16bit*.c`, `firmware/export/lcd.h`: LCD drawing API
