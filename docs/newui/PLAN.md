# New UI: master plan

An iPod-classic-style, code-drawn UI for Rockbox on the HiFi Walker H2.
Status: **plan only**. Nothing here is implemented yet.

Design references: the iPod classic (which has the same 320×240 screen as the
H2) and Q2 Pod's written spec (`docs/ipod.md` in github.com/DiamondBond/q2-pod,
MIT, 375×320). Measurements below are re-derived for 320×240; nothing is
copied from Q2 Pod's code (it patches Shanling's binary and can't be reused).

---

## 1. Approach

**The new UI is part of the core, not plugins.**

- The plugin loader is gone from this tree, and re-adding it only to host the
  UI would put the main screens behind an API boundary they constantly need
  to cross (playback, database, settings, lists, album art).
- The simulator runs core code directly, so every screen can be built and
  tested on the PC first, with no plugin needed.
- New code lives in its own directory, `apps/newui/`, so it stays easy to
  review, measure and debug.

Instead of plugins, three test aids are part of the plan:

| Test aid | What it is | Where it runs |
|---|---|---|
| **UI gallery** | A debug-menu screen that shows each widget and screen with fake data: lists, Now Playing, Coverflow, pop-ups | Simulator and device |
| **Simulator script runner** | Plays a scripted key sequence and saves a screenshot at each step. Lets me check layouts visually by reading the screenshots | Simulator (and `Xvfb`, see §7) |
| **Frame timer** | Logs how long each frame takes to draw (logf), to keep animations smooth | Device |

**Existing menus are restyled, not rewritten.** About 40 places use Rockbox's
list widget and 27 use its menu system, including every settings menu. Giving
that list widget the new look restyles all of them at once. Only the main
screens (Home, Now Playing, Library, Coverflow) get new code.

**The theme engine stays until the new screens replace it**, then goes
(phase 8). Until then, a setting switches between the old and new Now Playing
so the device always has a working UI.

---

## 2. Design system (320×240)

### Screen grid

```
y   0 ┌────────────────────────────────────────┐
      │ ▶          12:41               ᛒ ▮▮▮▯  │  status bar, 22 px
y  22 ├────────────────────────────────────────┤
      │                                        │
      │          content area 320 × 218        │
      │                                        │
y 240 └────────────────────────────────────────┘
```

- Side margin **12 px**. Gap between elements **8 px**.
- Everything is drawn on black. The H2's screen has square corners, so unlike
  Q2 Pod no corner insets are needed.

### Colours (Minimal theme, from Q2 Pod)

| Use | Colour |
|---|---|
| Background | `#000000` |
| Primary text | `#FFFFFF` |
| Secondary text (artist, counts, times) | `#AAAAAA` |
| Muted text | `#8C8C8C` |
| Selection card | `#EEEEEC`, text on it `#171717`, secondary `#484848` |
| Progress track / fill | `#1C1C1C` / `#EEEEEC` |
| Accent (Classic theme, user choice) | default Graphite `#6E6E6E` |

### Fonts

Rockbox supports anti-aliased fonts, and `tools/convttf.c` converts TrueType
to Rockbox's format. Plan: one open-licensed sans-serif (Inter or Source
Sans 3, both OFL), converted at fixed sizes:

| Name | Size | Used for |
|---|---|---|
| `ui-small` | 12 px | Times, "3 of 12", status bar clock |
| `ui-body` | 15 px | List rows, artist/album |
| `ui-title` | 18 px | Now Playing title, screen titles |
| `ui-big` | 40 px | Fast-scroll letter, volume number |

### Rows

| Row type | Height | Visible | Contents |
|---|---|---|---|
| Text row | 27 px | 8 | Label, optional value or chevron `›` on the right |
| Art row | 54 px | 4 | 46 px cover, title, grey second line |

Selection: a 6 px inset card with 6 px rounded corners (Minimal), or a
full-width accent bar (Classic). Long text is shortened with "…" and scrolls
only on the selected row.

### Motion

| Motion | Spec |
|---|---|
| List scrolling | Smooth, pixel-based; selection moves, the list follows |
| Wheel speed-up | Steps per tick grow with spin speed (like Q2 Pod's ramp) |
| Page change | Slide left/right, ~150 ms; skipped if the device can't keep 30 fps |
| Fast-scroll letter | Shown after 300 ms of fast spinning in lists of 16+ rows; hides 400 ms after |

---

## 3. Controls

The H2 has a wheel with a centre button (`PLAY`), and Menu, Back, Prev,
Next, Vol+, Vol−, Power.

| Control | Lists | Now Playing |
|---|---|---|
| Wheel | Move selection (with speed-up) | Volume (overlay) |
| Centre (`PLAY`) | Open | Play/pause |
| Centre double press | — | Scrub mode: wheel seeks, centre confirms |
| Back | Up one level | Back to where you came from |
| Hold Back | Jump to Now Playing | Return to previous screen |
| Menu | Context menu for the selection | Track options (queue, add to playlist, shuffle, repeat) |
| Prev / Next | Page up / page down | Previous / next track; hold to seek |
| Vol+ / Vol− | Volume (overlay) | Volume (overlay) |
| Power | Lock screen / sleep | Lock screen / sleep |

---

## 4. Screens

Measurements are in pixels, as (x, y, width × height).

### 4.1 Status bar (every screen)

```
┌────────────────────────────────────────┐
│ ▶          12:41               ᛒ ▮▮▮▯  │ 22 px
└────────────────────────────────────────┘
```

- Left (12, 5): play/pause/stop glyph.
- Centre: clock in `ui-small`; the screen title instead when the clock is off.
- Right: Bluetooth (when connected), battery (icon, optional %).
- Replaces both of today's status bars (`statusbar.c` and the themed one).

### 4.2 Home

```
┌────────────────────────────────────────┐
│ ▶          12:41               ᛒ ▮▮▮▯  │
├───────────────────┬────────────────────┤
│ • Now Playing     │                    │
│   Music           │   ┌────────────┐   │
│   Coverflow       │   │  current   │   │
│   Files           │   │  album     │   │
│   Playlists       │   │  cover     │   │
│   Shuffle Songs   │   └────────────┘   │
│   Settings        │                    │
│   Bluetooth       │                    │
└───────────────────┴────────────────────┘
```

- Left menu (0, 22, 160 × 218), 27 px rows, a small white dot for the
  selection. Wraps at both ends.
- Right panel (160, 22, 160 × 218): current album's cover, 148 px, centred;
  a placeholder when nothing is playing.
- **Full** layout option: menu full width, no art.
- Items: Now Playing (only while something is loaded), Music, Coverflow,
  Files, Playlists, Shuffle Songs, Settings, Bluetooth. Back on Home goes to
  the first row.
- Replaces `root_menu`'s main menu as the start screen.

### 4.3 Music (library)

Text list: Artists, Albums, Songs, Genres, Composers, Recently Added, Most
Played, Update Library. Driven by today's database browser (`tagtree.c` +
`tagnavi.config`), shown with the new list style:

```
┌────────────────────────────────────────┐
│ ◀ Music            12:41         ▮▮▮▯  │
├────────────────────────────────────────┤
│ ┌────────────────────────────────────┐ │  ← selection card
│ │ Artists                          › │ │
│ └────────────────────────────────────┘ │
│   Albums                           ›   │
│   Songs                            ›   │
│   Genres                           ›   │
│   ...                                  │
└────────────────────────────────────────┘
```

- **Albums** list uses art rows (46 px cover, album, grey artist).
- **Songs in an album**: a header row (cover 64 px, album, artist, year,
  count), then text rows with track number and duration.
- Long lists get the fast-scroll letter.

### 4.4 Coverflow

```
┌────────────────────────────────────────┐
│ ▶          12:41               ▮▮▮▯    │
├────────────────────────────────────────┤
│   ╱▏ ╱▏ ┌────────────┐ ▕╲ ▕╲           │
│  ▕ ▏▕ ▏ │  centre    │ ▕ ▏▕ ▏          │
│  ▕ ▏▕ ▏ │  cover     │ ▕ ▏▕ ▏          │
│   ╲▏ ╲▏ └────────────┘ ▕╱ ▕╱           │
│         ░░ reflection ░░               │
│        The Dark Side of the Moon       │
│              Pink Floyd                │
└────────────────────────────────────────┘
```

- Centre cover 120 px at (100, 32); two angled covers each side; faint
  reflection under all of them down to y 186.
- Album name `ui-body` white at y 192; artist `ui-small` grey at y 212.
- Wheel moves one album per tick, animated. Centre opens the album's tracks
  in a slide-up list over the covers; Back returns to the covers.
- Covers come from a **thumbnail cache** on the card (`/.rockbox/covers/`,
  120 px and 46 px versions, keyed by artist + album). Built on first use
  with a progress screen, updated incrementally. Real art is loaded only for
  ±3 albums around the centre; the rest show a placeholder until loaded.
- Album list comes from the database API (`tagcache_search`), the way
  Rockbox's PictureFlow did. **No database format change is needed** for
  this (see §8).

### 4.5 Now Playing

```
┌────────────────────────────────────────┐
│ ▶          12:41               ᛒ ▮▮▮▯  │ 0–22
│ 3 of 12                            ••• │ 24–38  ui-small grey
│ ╭──────────────╮                       │
│ │              │ Money                 │ title ui-title white
│ │    album     │ Pink Floyd            │ artist ui-body #CCCCCC
│ │     art      │ The Dark Side of …    │ album ui-body #BBBBBB
│ │   140×140    │                       │
│ │              │          ⇄  ♡         │ shuffle / favourite
│ ╰──────────────╯                       │
│              ● ○ ○                     │ page dots y 192
│ ━━━━━━━━━━━━━━━━━━━━━──────────────────│ 4 px line y 204
│ 0:15                             -6:05 │ ui-small grey y 212
└────────────────────────────────────────┘
```

- Art (12, 42, 140 × 140), 10 px rounded corners. Text column from x 164.
- Pages (wheel turns volume, so pages change with **Menu → View** or Prev/
  Next long-press; to be decided): 1 art + text, 2 track info (format,
  bitrate, sample rate, path), 3 big art. Visualiser is optional (phase 9).
- Volume overlay when the wheel or volume keys turn: centred rounded panel,
  speaker icon, bar, value; hides after 1.5 s.
- Scrub mode (centre double-press): bar turns white, wheel seeks with a ramp
  (5 s per tick, faster when spinning), centre or 3 s idle confirms.
- Replaces `apps/gui/wps.c` and the theme engine's Now Playing.

### 4.6 Context menu (Menu button)

Slide-up sheet over the current screen, text rows, items depend on the
selection: Play Next, Add to Queue, Add to Playlist, Show Artist, Show
Album, Delete (files), Properties. Uses today's `onplay.c` actions.

### 4.7 Settings

Today's settings menus (`settings_list.c`, `apps/menus/`) in the new list
style. Values show in grey on the right of each row. Choice screens and
sliders (`option_select.c`) restyled: sliders as a full-width bar with the
value above it. No settings logic changes.

Removed from Settings: Theme Settings (themes go in phase 8), plugin-based
entries (see `CLAUDE.md`). New: Display → Home layout, Theme
(Minimal/Classic), Accent colour, Clock on/off, Battery %.

### 4.8 Pop-ups and system screens

| Screen | Plan |
|---|---|
| Yes/No (`yesno.c`) | Centred rounded panel, title, two choices as a 2-row list |
| Splash (`splash.c`) | Centred rounded panel, text only, fades out |
| Progress (database scan, cover cache) | Panel with title, bar, count |
| Text entry (`keyboard.c`) | Wheel-driven character strip (iPod style) instead of today's grid |
| Quick screen (`quickscreen.c`) | Removed; its settings move to Now Playing's Menu |
| USB connected (`usb_screen.c`) | Black screen, USB glyph, "Connected", battery |
| Boot / shutdown | Logo + version / "Goodbye" |
| Lock screen | Clock, current track, cover; centre double-press unlocks |
| Bluetooth devices | List with status on the right (Connected / Paired / Searching) |

---

## 5. Code structure

```
apps/newui/
  ui_draw.c     drawing primitives: rounded rects, AA edges, text with
                ellipsis and fade, alpha blending, image blit and scale
  ui_font.c     font loading for the four sizes
  ui_anim.c     animation timing, frame pacing, easing
  ui_wheel.c    wheel speed-up and fast-scroll letter
  ui_list.c     pixel-smooth list widget (text and art rows), used for
                everything list-shaped
  ui_status.c   status bar
  ui_art.c      album art loading, thumbnail cache, in-RAM cache
  scr_home.c    Home
  scr_np.c      Now Playing (+ volume overlay, scrub)
  scr_cf.c      Coverflow
  scr_lib.c     library list views over tagtree
  scr_popup.c   yes/no, splash, progress, context sheet
  scr_gallery.c UI gallery (debug menu)
```

Existing code it connects to: `root_menu.c` (screen switching),
`gui/list.c` (redirected to `ui_list`), `tagtree.c`, `playback.c` events,
`recorder/albumart.c` + `jpeg_load.c` + `resize.c`, `settings_list.c`.

---

## 6. Phases

Every phase is a series of small commits. Each commit must build firmware,
simulator and (until phase 8) checkwps, with no new warnings. Screens are
checked in the simulator by screenshot before anything goes on the device.
**Device checkpoint** = the user installs the build and tests a short list.

| Phase | Goal | Main steps (≈ one commit each) | Done when |
|---|---|---|---|
| **0. Tooling** | Make work checkable | Simulator script runner + screenshot capture; font build (TTF → `.fnt` via convttf) for 4 sizes; frame timer (logf); UI gallery shell in debug menu | I can script keys and read screenshots; fonts load on the device |
| **1. Foundations** | Drawing and the new status bar | `ui_draw`, `ui_font`, `ui_anim`; `ui_status` replacing both status bars; gallery pages for each primitive | Status bar looks right everywhere; gallery renders on device at full speed. **Device checkpoint** |
| **2. Lists** | New look for every menu | `ui_list` with text rows and selection card; route `gui/list.c` drawing to it; wheel speed-up; value column; chevrons; art rows | All settings and browser menus show the new style; no menu behaves differently. **Device checkpoint** |
| **3. Home** | New start screen | `scr_home` split + full layouts; hook into `root_menu`; Back/hold-Back rules; Home settings | Device boots to Home |
| **4. Now Playing** | Replace the theme Now Playing | `scr_np` layout; art loading; progress/time; volume overlay; info page; scrub; switch setting (old/new); then default to new | Plays, seeks, volume, shows art on device. **Device checkpoint** |
| **5. Library** | Database browsing | Music menu; albums with art rows; album track page with header; songs list; fast-scroll letter | Browse artist → album → track and play |
| **6. Coverflow** | Coverflow | Thumbnail cache builder + progress screen; album list from database; 3D-ish rendering + reflection; animation; album track sheet | Smooth at 30 fps on device with a real library. **Device checkpoint** |
| **7. Pop-ups** | System screens | Yes/no, splash, progress, context sheet, keyboard, USB, boot/shutdown, lock screen, Bluetooth device list | No old-style screen left in normal use |
| **8. Removal** | Delete what the new UI replaced | Theme engine, `skin_parser`, checkwps, `wps/`, backdrops, theme settings; old status bar; quick screen; plugin menu leftovers; voice call sites; unused fonts and icons | Builds clean; measure code lines (expected large drop) |
| **9. Polish** | Extras | Page transitions; Classic theme + accent colours; visualiser (optional); translations of new strings | — |

Phases 3–7 can be reordered once phase 2 is in. Each phase starts with a short
design check (screenshots of the gallery) before wiring it into real screens.

---

## 7. How I'll test

- **Simulator first.** Every screen is built and checked in `build-sim`. The
  script runner sends key presses and saves screenshots, which I can view to
  check alignment, sizes and colours. It needs either `xvfb` + `xdotool`
  (`sudo apt install xvfb xdotool`, simplest), or a small key-script mode built
  into the simulator (no extra packages; I can add this in phase 0).
- **Golden screenshots.** Each finished screen gets reference screenshots in
  `newui-tools/shots/`; later changes are compared against them to catch
  layout regressions.
- **Device checkpoints** at the end of phases 1, 2, 4 and 6, with a checklist.
  The device's own screen-dump feature can send me exact device screenshots.
- **Speed.** The frame timer logs drawing time on the device. Targets: lists
  and Coverflow at 30 fps or better, Now Playing updates under 10 ms.

---

## 8. Hurdles and risks

| # | Risk | Impact | Plan |
|---|---|---|---|
| 1 | **Drawing speed on the X1000.** Smooth scrolling, Coverflow and slides redraw most of the 320×240 screen each frame; Rockbox's drawing is not built for that. | Could make animation choppy | Measure in phase 1 with the frame timer before building on it. Redraw only changed areas; precompute Coverflow's slanted covers; drop animations that can't hit 30 fps |
| 2 | **Fonts.** Good-looking anti-aliased text needs a converted TrueType font; `convttf` needs the FreeType library (`sudo apt install libfreetype-dev`). Apple's own fonts can't be used. | Look and feel | Use Inter or Source Sans 3 (free licence); check the converted sizes on the device in phase 0 |
| 3 | **Theme engine is wired into 37 files.** Lists, menus, the USB screen, yes/no and Now Playing all go through it. | Phase 8 is a big, risky removal | Keep it working until every screen is replaced; remove it last, in small steps |
| 4 | **Album art.** Covers come from `cover.jpg` files or embedded tags (MP3, M4A, FLAC); large JPEGs take time to decode; libraries without art look empty. | Coverflow speed, first-run wait | Thumbnail cache built once with a progress screen; placeholder art; only ±3 covers in full size |
| 5 | **Database dependency.** Library and Coverflow need Rockbox's database built (slow on the first scan). | First-run experience | "Update Library" on Home's Music menu with a progress screen; Coverflow offers to build it if missing. No format change planned; only if the album query turns out too slow |
| 6 | **Memory (32 MB).** Cover caches, fonts and screen buffers compete with the audio buffer. | Stability | Fixed budget: ~1 MB for covers (≈20 × 120 px + 60 × 46 px), fonts ~200 KB; measured in the debug menu |
| 7 | **Visual checking.** I can check the simulator myself only once screenshots can be scripted (risk-free once tooling is in); device-only problems (colours, speed, font sharpness) need you. | Slower iteration on device issues | Device checkpoints with short checklists; device screen dumps |
| 8 | **Upstream updates.** The UI rewrite moves us further from upstream Rockbox. | Hard to take upstream fixes later | Accept it as a fork; keep new code in `apps/newui/` so conflicts stay in a few files |
| 9 | **Behaviour hidden in old screens.** Old screens handle many events (USB, low battery, database busy, playlist errors). | Missed edge cases | Reuse the existing event handling (`default_event_handler`), and keep the old screens reachable until phase 8 |
| 10 | **Translations.** New screens add English strings; other languages fall back to English until translated. | Minor | Translate later (phase 9) |

None of these is a blocker. #1 (drawing speed) decides how much animation is
possible and is measured first.

**Measured on the H2 (2026-10-15, UI gallery):** clearing the full screen in
memory takes 0.64 ms; a full-screen `lcd_update()` brings a frame to 15.4 ms
(65 fps); a page of eight anti-aliased text rows adds about 0.2 ms. Sending the
frame to the display is the limit, not drawing, so 60 fps animation is possible
when a frame's drawing stays near 1 ms, and 30 fps leaves ~18 ms of drawing per
frame. Risk #1 is resolved; small changes should use `lcd_update_rect()`.

---

## 9. Decisions

Made (2026-10-15):

1. **Font**: Source Sans 3.
2. **Home items and menu layout**: as drafted in §4, refined during the work.
3. **Back** always goes to the parent screen, including from Now Playing.
   Now Playing's extra pages (track info, big art) open from its Menu sheet
   instead of being switched in place.
4. **Default theme**: Classic (accent-coloured selection bar). Minimal stays
   available as an option.
5. **Branch**: `newui-bt`.

Open:

6. Simulator test tool: `xvfb` + `xdotool`, or a key-script mode built into
   the simulator. `libfreetype-dev` is needed in either case (font conversion).
