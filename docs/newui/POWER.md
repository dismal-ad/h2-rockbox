# Battery life: review and action plan

Status: **planned, not started.** To be done after the UI work. Written
2026-10-19 from a review of the `newui-bt` branch.

## What is already good

- **Idle CPU:** with no thread runnable, the kernel calls `core_sleep()`,
  which executes the MIPS `wait` instruction
  (`firmware/target/mips/ingenic_x1000/system-target.h`).
- **Screen off:** when the backlight goes off, the LCD panel is put to sleep
  and its refresh DMA stops (`lcd_enable(false)` in `lcd-x1000.c`, called
  from `backlight-erosqnative.c`).
- **Now Playing and the status bar** skip drawing while the screen is off
  (`lcd_active()` checks in `apps/gui/wps.c`, `apps/gui/statusbar.c`).
- **Buffering** reads in bursts: it fills the whole audio buffer, then
  refills only when about 1 s of audio is left (`seconds = 1` for flash in
  `audio_update_filebuf_watermark()`, `apps/playback.c`). The SD card idles
  between bursts.

## What wastes power

| # | Problem | Where | Impact |
|---|---|---|---|
| 1 | The CPU always runs at 1008 MHz; frequency scaling is not enabled for this target. Decoding music needs roughly 10-20% of that. | `firmware/export/config/erosqnative.h` (no `HAVE_ADJUSTABLE_CPU_FREQ`), `firmware/target/mips/ingenic_x1000/clk-x1000.c` | High |
| 2 | Home's cover slideshow keeps rendering at 15 frames/s when the screen is off (nothing playing, backlight timed out). | `apps/newui/scr_home.c`, `apps/newui/ui_slides.c`: no `lcd_active()` check | High while left on Home |
| 3 | Cover Flow's background cover building (JPEG decode + SD reads) also continues with the screen off. | `apps/newui/scr_coverflow.c` | Medium |
| 4 | Every screen update busy-waits for the panel's frame (up to ~16 ms of the CPU spinning flat out) instead of sleeping. The driver says `// TODO: This may need a yield?`. | `lcd_wait()` in `firmware/target/mips/ingenic_x1000/lcd-x1000.c` | Medium during animation (Cover Flow, slideshow, scrolling) |
| 5 | The SD controller is never idled: `storage_sleep()` / `storage_sleepnow()` are empty for SD (`//sd_sleepnow()`). Its clock runs between buffer refills. | `firmware/storage.c`, `firmware/target/mips/ingenic_x1000/msc-x1000.c` | Low-medium, all the time |
| 6 | Bluetooth is not put into a low-power state (sniff mode) when on but paused or idle; only the 32 kHz sleep clock is set. | `apps/bluetooth/bluetooth.c`, `hci-erosqnative.c` | Medium when Bluetooth is on |
| 7 | Settings: backlight timeout and brightness are the largest drain per minute of screen use; the database is not held in RAM, so browsing reads the card. | `apps/settings_list.c` defaults | Depends on use |

## Buffering options

| Option | Today | Proposal | Effect |
|---|---|---|---|
| Audio buffer size | All RAM left over (~20-24 MB: ~10 min of 320 kbps MP3, ~3 min of FLAC) | Free ~1.5-2 MB: size the new UI's buffers (slideshow 2x115 KB, covers 16x45 KB, Cover Flow names/paths 256 KB, art 180 KB) to need and free them when their screens close; review the 2 MB plugin scratch buffer | Fewer refills, the SD wakes less often |
| When refilling starts | ~1 s of audio left | Keep | Already best for flash |
| Read chunk size | 32 KB per SD read (`BUFFERING_DEFAULT_FILECHUNK`, `apps/buffering.c`) | 128-256 KB | Fewer SD commands per refill; the card idles sooner |
| SD idle after a refill | `storage_sleep()` does nothing | Gate the MSC clock after a refill; ungate on the next access | Saves controller idle current between refills |
| Decoded-audio (PCM) buffer | 3 s minimum, refilled below 2 s (`apps/pcmbuf.c`) | Keep | Already large; keeps playback smooth |
| UI card reads while playing | Cover Flow reads covers; browsing reads the database | Database "Load to RAM: On"; keep covers cached in RAM | Fewer card wake-ups |
| Database update at boot | Off | Keep off | No full card scan |

## Plan, in order

1. **Stop rendering while the screen is off** (quick, low risk). Pause the
   Home slideshow and Cover Flow's background work whenever `lcd_active()`
   is false; resume on wake.
2. **Make screen updates sleep instead of spin** (medium). Change
   `lcd_wait()` to give up the CPU until the frame is done (interrupt, or a
   short sleep), freeing CPU time during every animation.
3. **Enable CPU frequency scaling** (largest gain, most care). Add
   `HAVE_ADJUSTABLE_CPU_FREQ` with two speeds (~1 GHz and ~200-400 MHz);
   Rockbox's existing `cpu_boost()` calls then raise the clock only for
   decoding bursts, UI animation and cover decoding. Needs X1000
   clock-switching code (PLL dividers) and testing that the audio, SD and
   LCD clocks stay correct. New low-level code: do it last, carefully.
4. **Let the SD controller idle between refills** (medium). Implement
   `storage_sleep()` for SD (gate the MSC clock) and enlarge read chunks to
   128-256 KB.
5. **Give the audio buffer more RAM** (low risk). Size the new UI's buffers
   to need; free them when their screens close.
6. **Bluetooth low power** (medium). Sniff mode when paused or idle;
   optionally turn Bluetooth off after a few minutes with no device
   connected.
7. **Settings the owner can change now:** shorter backlight timeout, lower
   brightness, Database "Load to RAM: On", the auto power-off timer.

## How to measure

The PMU's battery current is not read (`CURRENT_MEASURE` is disabled in
`config/erosqnative.h`), so:

- **CPU busy time:** enable the X1000 CPU-idle statistics
  (`X1000_CPUIDLE_STATS`) and show idle % in the debug menu; compare each
  screen before and after a change.
- **Battery runtime:** play the same album with the screen off and record
  battery % per hour, before and after each step.

Suggested order: steps 1 and 2 (safe, quick) with the idle-% readout to see
the gain, then step 4, and CPU scaling (step 3) last.
