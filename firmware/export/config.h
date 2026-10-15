/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Daniel Stenberg
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "autoconf.h"

/* symbolic names for multiple choice configurations: */

/* CONFIG_STORAGE (note these are combineable bit-flags) */
#define STORAGE_ATA_NUM     0
#define STORAGE_MMC_NUM     1
#define STORAGE_SD_NUM      2
#define STORAGE_NAND_NUM    3
#define STORAGE_RAMDISK_NUM 4
#define STORAGE_HOSTFS_NUM  6
#define STORAGE_NUM_TYPES   7

#define STORAGE_ATA         (1 << STORAGE_ATA_NUM)
#define STORAGE_MMC         (1 << STORAGE_MMC_NUM)
#define STORAGE_SD          (1 << STORAGE_SD_NUM)
#define STORAGE_NAND        (1 << STORAGE_NAND_NUM)
#define STORAGE_RAMDISK     (1 << STORAGE_RAMDISK_NUM)
 /* meant for APPLICATION targets (implicit for SIMULATOR) */
#define STORAGE_HOSTFS      (1 << STORAGE_HOSTFS_NUM)

/* CONFIG_TUNER (note these are combineable bit-flags) */

/* CONFIG_CPU */
#define MCF5249      5249
#define MCF5250      5250
#define PP5002       5002
#define PP5020       5020
#define PP5022       5022
#define PP5024       5024
#define PP6100       6100
#define S3C2440      2440
#define DSC25          25
#define DM320         320
#define IMX31L         31
#define TCC7801      7801
#define S5L8700      8700
#define S5L8701      8701
#define S5L8702      8702
#define S5L8720      8720
#define S5L8723      8723
#define S5L8730      8730
#define S5L8740      8740
#define JZ4732       4732
#define JZ4760B     47602
#define AS3525       3525
#define AS3525v2    35252
#define IMX233        233
#define RK27XX       2700
#define X1000        1000
#define STM32H743   32743

/* platforms
 * bit fields to allow PLATFORM_HOSTED to be OR'ed e.g. with a
 * possible future PLATFORM_ANDROID (some OSes might need totally different
 * handling to run on them than a stand-alone application) */
#define PLATFORM_NATIVE       (1<<0)
#define PLATFORM_HOSTED       (1<<1)
#define PLATFORM_ANDROID      (1<<2)
#define PLATFORM_SDL          (1<<3)
#define PLATFORM_GAME_CONSOLE (1<<4)

/* CONFIG_KEYPAD */
#define EROSQ_PAD          72

/* CONFIG_REMOTE_KEYPAD */

/* CONFIG_BACKLIGHT_FADING */
/* No fading capabilities at all (yet) */
#define BACKLIGHT_NO_FADING         0x0
/* Backlight fading is controlled using a hardware PWM mechanism */
#define BACKLIGHT_FADING_PWM        0x1
/* Backlight is controlled using a software implementation
 * BACKLIGHT_FADING_SW_SETTING means that backlight is turned on by only setting
 * the brightness (i.e. no real difference between backlight_on and
 * backlight_set_brightness)
 * BACKLIGHT_FADING_SW_HW_REG means that backlight brightness is restored
 * "in hardware", from a hardware register upon backlight_on
 * Both types need to have minor adjustments in the software fading code */
#define BACKLIGHT_FADING_SW_SETTING 0x2
#define BACKLIGHT_FADING_SW_HW_REG  0x4
/* Backlight fading is done in a target specific way
 * for example in hardware, but not controllable*/
#define BACKLIGHT_FADING_TARGET     0x8

/* CONFIG_CHARGING */

/* Generic types */
#define CHARGING_MONITOR 2 /* Hardware controlled charging with monitoring
                            * (CPU is able to read HW charging state and
                            *  when power is plugged-in). */

/* Mostly target-specific code in the /target tree */

/* CONFIG_BATTERY_MEASURE bits */
/* If both VOLTAGE_MEASURE and PERCENTAGE_MEASURE are defined,
 * _battery_level() (percentage) will be preferred, unless _battery_level()
 * returns -1, then voltage will be used from _voltage_level(). */
#define VOLTAGE_MEASURE     1 /* Target can report battery voltage
                               * Usually native ports */
#define PERCENTAGE_MEASURE  2 /* Target can report remaining capacity in %
                               * Usually application/hosted ports */
#define CURRENT_MEASURE     8 /* Target can report battery charge and/or
                               * discharge current */
/* CONFIG_LCD */
#define LCD_EROSQ         71 /* as used by the ErosQ (native) */

/* LCD_PIXELFORMAT */
#define RGB565 565
#define RGB888 888
#define XRGB8888 8888

/* LCD_STRIDEFORMAT */
#define VERTICAL_STRIDE     1
#define HORIZONTAL_STRIDE   2

/* CONFIG_ORIENTATION */
#define SCREEN_PORTRAIT     0
#define SCREEN_LANDSCAPE    1
#define SCREEN_SQUARE       2

/* CONFIG_I2C */
#define I2C_NONE     0 /* For targets that do not use I2C - as the
Lyre prototype 1 */

/* CONFIG_LED */
#define LED_VIRTUAL  2 /* Virtual LED (icon) (Archos Ondio) */
/* else                   HW controlled LED (iRiver H1x0) */

/* CONFIG_NAND */
#define NAND_RK27XX  5

/* CONFIG_RK27XX_FTL: the on-flash format of an rk27xx target's NAND. A
 * target that does not define it builds only the FTL scheme finder. */
#define RK27XX_FTL_SCHEME_A 1 /* ftl-scheme-a.c: zone tables, remap logs */

/* CONFIG_RTC */
#define RTC_AS3514   6 /* Sandisk Sansa series */
#define RTC_MC13783  13 /* Freescale MC13783 PMIC */
#define RTC_X1000    23 /* Ingenic X1000 */

/* USB On-the-go */
#define USBOTG_M66591   6591 /* M:Robe 500 */
#define USBOTG_ISP1583  1583 /* Creative Zen Vision:M */
#define USBOTG_ARC      5020 /* PortalPlayer 502x and IMX233 */
#define USBOTG_JZ4740   4740 /* Ingenic Jz4740/Jz4732 */
#define USBOTG_JZ4760   4760 /* Ingenic Jz4760/Jz4760B */
#define USBOTG_AS3525   3525 /* AMS AS3525 */
#define USBOTG_S3C6400X 6400 /* Samsung S3C6400X, also used in the S5L8701/S5L8702/S5L8720 */
#define USBOTG_DESIGNWARE 6401 /* Synopsys DesignWare OTG, used in S5L8701/S5L8702/S5L8720/AS3252v2 */
#define USBOTG_RK27XX   2700 /* Rockchip rk27xx */
#define USBOTG_TNETV105 105  /* TI TNETV105 */

/* Multiple cores */
#define CPU 0
#define COP 1

/* CONFIG_BUFLIB_BACKEND */
#define BUFLIB_BACKEND_MEMPOOL      0 /* Default memory pool backed buflib */

/* CONFIG_BINFMT */
#define BINFMT_ROCK                 0 /* Rockbox ".rock" format */
#define BINFMT_DLOPEN               1 /* dlopen-based */

/* now go and pick yours */
#if defined(EROS_QN)
#include "config/erosqnative.h"
#else
#error "unknown hardware platform!"
#endif

#ifndef CONFIG_CPU
#define CONFIG_CPU 0
#endif

// NOTE: should be placed before sim.h (where CONFIG_CPU is undefined)
#if !(CONFIG_CPU >= PP5002 && CONFIG_CPU <= PP5022) && CODEC_SIZE >= 0x80000
#define CODEC_AAC_SBR_DEC
#endif

#ifdef __PCTOOL__
#undef CONFIG_CPU
#define CONFIG_CPU 0
#undef HAVE_MULTIVOLUME
#undef HAVE_MULTIDRIVE
#undef CONFIG_STORAGE_MULTI
#undef CONFIG_STORAGE
#define CONFIG_STORAGE 0
#endif

#ifndef CONFIG_BUFLIB_BACKEND
# define CONFIG_BUFLIB_BACKEND BUFLIB_BACKEND_MEMPOOL
#endif

#if defined(APPLICATION)
#ifndef CONFIG_CPU
#define CONFIG_CPU 0
#endif
#endif

/* keep this include after the target configs */
#ifdef SIMULATOR
#include "config/sim.h"
#ifndef HAVE_POWEROFF_WHILE_CHARGING
    #define HAVE_POWEROFF_WHILE_CHARGING
#endif
#endif

#if defined(__PCTOOL__) || defined(SIMULATOR)
#ifndef CONFIG_PLATFORM
#define CONFIG_PLATFORM PLATFORM_HOSTED
#endif
#endif

#ifndef CONFIG_PLATFORM
#define CONFIG_PLATFORM PLATFORM_NATIVE
#endif

#ifndef CONFIG_BINFMT
# if (CONFIG_PLATFORM & PLATFORM_NATIVE)
#  define CONFIG_BINFMT BINFMT_ROCK
# elif (CONFIG_PLATFORM & PLATFORM_HOSTED)
#  define CONFIG_BINFMT BINFMT_DLOPEN
# else
#  error "CONFIG_BINFMT not defined!"
# endif
#endif

#if CONFIG_BINFMT == BINFMT_ROCK
# define HAVE_LC_OPEN_FROM_MEM
#endif

/* Codec buffering requires the ability to load code from RAM */
#if defined(HAVE_LC_OPEN_FROM_MEM)
# define HAVE_CODEC_BUFFERING
#endif

/* setup basic macros from capability masks */
#include "config_caps.h"

/* setup CPU-specific defines */

#ifndef __PCTOOL__

/* define for all cpus from coldfire family */
#if (ARCH == ARCH_M68K) && ((CONFIG_CPU == MCF5249) || (CONFIG_CPU == MCF5250))
#define CPU_COLDFIRE
#endif

/* define for all cpus from PP family */
#if (CONFIG_CPU == PP5002)
#define CPU_PP
#elif (CONFIG_CPU == PP5020) || (CONFIG_CPU == PP5022) \
    || (CONFIG_CPU == PP5024) || (CONFIG_CPU == PP6100)
#define CPU_PP
#define CPU_PP502x
#endif

/* define for all cpus from S5L87XX family */
#if (CONFIG_CPU == S5L8700) || (CONFIG_CPU == S5L8701) \
    || (CONFIG_CPU == S5L8702) || (CONFIG_CPU == S5L8720) \
    || (CONFIG_CPU == S5L8723) || (CONFIG_CPU == S5L8730) \
    || (CONFIG_CPU == S5L8740)
#define CPU_S5L87XX
#endif

/* define for all cpus from TCC780 family */
#if (CONFIG_CPU == TCC7801)
#define CPU_TCC780X
#endif

/* define for all cpus from ARM7TDMI family (for specific optimisations) */
#if defined(CPU_PP) || (CONFIG_CPU == DSC25)
#define CPU_ARM7TDMI
#endif

/* define for all cpus from ARM family */
#if ARCH == ARCH_ARM
#define CPU_ARM
#define ARM_ARCH    ARCH_VERSION /* ARMv{4,5,6,7,8,9} */
#define ARM_PROFILE ARCH_PROFILE /* Classic, Microcontroller, Application [,Realtime] */
# if ARM_PROFILE == ARM_PROFILE_MICRO
#  define CPU_ARM_MICRO
# elif ARM_PROFILE == ARM_PROFILE_CLASSIC
#  define CPU_ARM_CLASSIC
# elif ARM_PROFILE == ARM_PROFILE_APPLICATION
#  define CPU_ARM_APPLICATION
# endif
#if defined(__ARM_FEATURE_IDIV)
# define ARM_HAVE_HW_DIV
#endif
# if (CONFIG_PLATFORM & PLATFORM_NATIVE) && !defined(ARM_HAVE_HW_DIV)
#  define ARM_NEED_DIV0
# endif
#endif

#if ARCH == ARCH_MIPS
#define CPU_MIPS ARCH_VERSION /* 32, 64 */
#endif

#endif /*__PCTOOL__*/

/* now set any CONFIG_ defines correctly if they are not used,
   No need to do this on CONFIG_'s which are compulsory (e.g CONFIG_CODEC ) */

#if !defined(CONFIG_BACKLIGHT_FADING)
#define CONFIG_BACKLIGHT_FADING BACKLIGHT_NO_FADING
#endif

#ifndef CONFIG_I2C
#define CONFIG_I2C I2C_NONE
#endif

#ifndef CONFIG_TUNER
#define CONFIG_TUNER 0
#endif

#ifndef CONFIG_USBOTG
#define CONFIG_USBOTG 0
#endif

#ifndef CONFIG_LED
#define CONFIG_LED LED_VIRTUAL
#endif

#ifndef CONFIG_CHARGING
#define CONFIG_CHARGING 0
#endif

#ifndef CONFIG_BATTERY_MEASURE
#define CONFIG_BATTERY_MEASURE 0
#define NO_LOW_BATTERY_SHUTDOWN
#endif

#ifndef CONFIG_RTC
#define CONFIG_RTC 0
#endif

#ifndef BATTERY_CAPACITY_DEFAULT
#define BATTERY_CAPACITY_DEFAULT 0
#endif

#ifndef BATTERY_CAPACITY_MIN
#define BATTERY_CAPACITY_MIN BATTERY_CAPACITY_DEFAULT
#endif

#ifndef BATTERY_CAPACITY_MAX
#define BATTERY_CAPACITY_MAX BATTERY_CAPACITY_DEFAULT
#endif

#ifndef BATTERY_CAPACITY_INC
#define BATTERY_CAPACITY_INC 0
#endif

#ifdef HAVE_RDS_CAP
/* combinable bitflags */
/* 0x01 can be reused, was RDS_CFG_ISR */
#define RDS_CFG_PROCESS 0x2 /* uses raw packet processing */
#define RDS_CFG_PUSH    0x4 /* pushes processed information */
#define RDS_CFG_POLL    0x8 /* tuner driver provides a polling function */
#ifndef CONFIG_RDS
#define CONFIG_RDS  RDS_CFG_PROCESS /* thread processing+raw processing */
#endif /* CONFIG_RDS */
#endif /* HAVE_RDS_CAP */

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
#if CONFIG_RTC == RTC_AS3514
#if CONFIG_CPU == AS3525 || CONFIG_CPU == AS3525v2
#define HAVE_RTC_IRQ
#endif
#elif CONFIG_RTC == RTC_MC13783
#define HAVE_RTC_IRQ
#endif
#endif /* (CONFIG_PLATFORM & PLATFORM_NATIVE) */

#ifndef CONFIG_ORIENTATION
#if LCD_HEIGHT > LCD_WIDTH
#define CONFIG_ORIENTATION SCREEN_PORTRAIT
#elif LCD_HEIGHT < LCD_WIDTH
#define CONFIG_ORIENTATION SCREEN_LANDSCAPE
#else
#define CONFIG_ORIENTATION SCREEN_SQUARE
#endif
#endif

/* Pixel aspect ratio is defined in terms of a multiplier for pixel width and
 * height, and is set to 1:1 if the target does not set a value
 */
#ifndef LCD_PIXEL_ASPECT_HEIGHT
#define LCD_PIXEL_ASPECT_HEIGHT 1
#endif
#ifndef LCD_PIXEL_ASPECT_WIDTH
#define LCD_PIXEL_ASPECT_WIDTH 1
#endif

/* Used for split displays (Sansa Clip). Set to 0 otherwise */
#ifndef LCD_SPLIT_LINES
#define LCD_SPLIT_LINES 0
#endif

/* Most displays have a horizontal stride */
#ifndef LCD_STRIDEFORMAT
# define LCD_STRIDEFORMAT HORIZONTAL_STRIDE
#endif

/* Simulator LCD dimensions. Set to standard dimensions if undefined */
#ifndef SIM_LCD_WIDTH
#define SIM_LCD_WIDTH LCD_WIDTH
#endif
#ifndef SIM_LCD_HEIGHT
#define SIM_LCD_HEIGHT (LCD_HEIGHT + LCD_SPLIT_LINES)
#endif

#ifdef HAVE_REMOTE_LCD
#ifndef SIM_REMOTE_WIDTH
#define SIM_REMOTE_WIDTH LCD_REMOTE_WIDTH
#endif
#ifndef SIM_REMOTE_HEIGHT
#define SIM_REMOTE_HEIGHT LCD_REMOTE_HEIGHT
#endif
#endif /* HAVE_REMOTE_LCD */

/* define this in the target config.h to use a different size */
#ifndef CONFIG_DEFAULT_ICON_HEIGHT
#define CONFIG_DEFAULT_ICON_HEIGHT 8
#endif
#ifndef CONFIG_DEFAULT_ICON_WIDTH
#define CONFIG_DEFAULT_ICON_WIDTH 6
#endif
#ifndef CONFIG_REMOTE_DEFAULT_ICON_HEIGHT
#define CONFIG_REMOTE_DEFAULT_ICON_HEIGHT 8
#endif
#ifndef CONFIG_REMOTE_DEFAULT_ICON_WIDTH
#define CONFIG_REMOTE_DEFAULT_ICON_WIDTH 6
#endif

#if LCD_DEPTH > 1 || defined(HAVE_REMOTE_LCD) && LCD_REMOTE_DEPTH > 1
#define HAVE_BACKDROP_IMAGE
#endif

#if (CONFIG_TUNER & (CONFIG_TUNER - 1)) != 0
/* Multiple possible tuners */
#define CONFIG_TUNER_MULTI
#endif

/* deactivate fading in bootloader */
#if defined(BOOTLOADER)
#undef CONFIG_BACKLIGHT_FADING
#define CONFIG_BACKLIGHT_FADING BACKLIGHT_NO_FADING
#endif

/* determine which setting/manual text to use */
#if (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_PWM)

/* possibly overridden in target config */
#if !defined(HAVE_BACKLIGHT_FADING_BOOL_SETTING) \
    && !defined(HAVE_BACKLIGHT_FADING_INT_SETTING)
#define HAVE_BACKLIGHT_FADING_INT_SETTING
#endif

#elif  (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_SW_SETTING) \
    || (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_SW_HW_REG) \
    || (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_TARGET)

/* possibly overridden in target config */
#if !defined(HAVE_BACKLIGHT_FADING_BOOL_SETTING) \
    && !defined(HAVE_BACKLIGHT_FADING_INT_SETTING)
#define HAVE_BACKLIGHT_FADING_BOOL_SETTING
#endif

#endif /* CONFIG_BACKLIGHT_FADING */

/* Storage related config handling */

/* The rk27xx NAND's Scheme A flash translation layer holds part-written
 * pages in RAM (ftl-scheme-a.c) until a later write completes them;
 * storage_flush() commits them at shutdown, ROLO and wherever else it is
 * called. */
#if (CONFIG_STORAGE & STORAGE_NAND) && defined(CONFIG_NAND) \
    && (CONFIG_NAND == NAND_RK27XX) \
    && (CONFIG_RK27XX_FTL == RK27XX_FTL_SCHEME_A) \
    && !defined(HAVE_STORAGE_FLUSH)
#define HAVE_STORAGE_FLUSH
#endif

#if (CONFIG_STORAGE & (CONFIG_STORAGE - 1)) != 0
/* Multiple storage drivers */
#define CONFIG_STORAGE_MULTI
#endif

#if defined(CONFIG_STORAGE_MULTI) && !defined(HAVE_MULTIDRIVE)
#define HAVE_MULTIDRIVE
#endif

#if defined(HAVE_MULTIDRIVE) && !defined(NUM_DRIVES)
#error HAVE_MULTIDRIVE needs to have an explicit NUM_DRIVES
#endif

#ifndef NUM_DRIVES
#define NUM_DRIVES 1
#endif

#if !defined(HAVE_MULTIVOLUME)
#if defined(HAVE_MULTIDRIVE)
/* Multidrive strongly implies multivolume */
#define HAVE_MULTIVOLUME
#elif (CONFIG_STORAGE & STORAGE_SD)
/* SD routinely have multiple partitions */
#elif (CONFIG_STORAGE & STORAGE_ATA) && defined(HAVE_LBA48)
/* ATA routinely haves multiple partitions, but don't bother if we can't do LBA48 */
#define HAVE_MULTIVOLUME
#endif
#endif

/* Bootloaders don't need multivolume awareness */
#if defined(BOOTLOADER) && defined(HAVE_MULTIVOLUME) \
    && !(CONFIG_PLATFORM & PLATFORM_HOSTED) && !defined(BOOT_REDIR)
#undef HAVE_MULTIVOLUME
#endif

/* Number of volumes per drive */
#if defined(HAVE_MULTIVOLUME) && !defined(SIMULATOR) && !(CONFIG_PLATFORM & PLATFORM_HOSTED)
#define NUM_VOLUMES_PER_DRIVE 4
#else
#define NUM_VOLUMES_PER_DRIVE 1
#endif

/* note to remove multi-partition booting this could be changed to MULTIDRIVE */
#if defined(HAVE_BOOTDATA) && defined(BOOT_REDIR) && defined(HAVE_MULTIVOLUME)
#define HAVE_MULTIBOOT
#endif

/* The lowest numbered volume to read a multiboot redirect from; default is to
 * allow any volume but some targets may wish to exclude the internal drive. */
#if defined(HAVE_MULTIBOOT) && !defined(MULTIBOOT_MIN_VOLUME)
# define MULTIBOOT_MIN_VOLUME 0
#endif

#define NUM_VOLUMES (NUM_DRIVES * NUM_VOLUMES_PER_DRIVE)

/* Sanity check sector size options */
#if defined(MAX_VARIABLE_LOG_SECTOR) && defined(MAX_VIRT_SECTOR_SIZE)
#if (MAX_VIRT_SECTOR_SIZE < MAX_VARIABLE_LOG_SECTOR)
#error "optional MAX_VIRT_SECTOR_SIZE must be at least as large as MAX_VARIABLE_LOG_SECTOR"
#endif
#endif

#if defined(BOOTLOADER) && defined(HAVE_ADJUSTABLE_CPU_FREQ)
/* Bootloaders don't use CPU frequency adjustment */
#undef HAVE_ADJUSTABLE_CPU_FREQ
#endif

#if defined(__PCTOOL__) && defined(HAVE_ADJUSTABLE_CPU_FREQ)
/* PCTOOLs don't use CPU frequency adjustment */
#undef HAVE_ADJUSTABLE_CPU_FREQ
#endif

/* Enable the directory cache and tagcache in RAM if we have
 * plenty of RAM. Both features can be enabled independently. */
#if (MEMORYSIZE >= 8) && !defined(BOOTLOADER) && (defined(CHECKWPS) || !defined(__PCTOOL__)) \
    && !defined(APPLICATION)
#ifndef SIMULATOR
#define HAVE_DIRCACHE
#endif
#ifdef HAVE_TAGCACHE
#define HAVE_TC_RAMCACHE
#endif
#endif

#if defined(HAVE_TAGCACHE)
#define HAVE_PICTUREFLOW_INTEGRATION
#endif

#ifdef BOOTLOADER

#ifdef HAVE_BOOTLOADER_USB_MODE
/* Priority in bootloader is wanted */
#define HAVE_PRIORITY_SCHEDULING

#if (CONFIG_CPU == S5L8702) || (CONFIG_CPU == S5L8720)
#define USB_DRIVER_CLOSE
#else
#define USB_STATUS_BY_EVENT
#define USB_DETECT_BY_REQUEST
#endif

#if CONFIG_USBOTG == USBOTG_ARC
#define INCLUDE_TIMEOUT_API
#define USB_DRIVER_CLOSE
#endif

#if CONFIG_USBOTG == USBOTG_TNETV105
#define INCLUDE_TIMEOUT_API
#define USB_DRIVER_CLOSE
#endif

#if CONFIG_CPU == X1000
#define USB_DRIVER_CLOSE
#endif

#endif /* BOOTLOADER_USB_MODE */

#else /* !BOOTLOADER */

#define HAVE_EXTENDED_MESSAGING_AND_NAME
#define HAVE_WAKEUP_EXT_CB

#if defined(ASSEMBLER_THREADS) \
    || defined(HAVE_WIN32_FIBER_THREADS) \
    || defined(HAVE_SIGALTSTACK_THREADS) \
    || defined(N3DS)
#define HAVE_PRIORITY_SCHEDULING
#endif

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
#define HAVE_PRIORITY_SCHEDULING
#define HAVE_SCHEDULER_BOOSTCTRL
#endif /* PLATFORM_NATIVE */

#ifdef HAVE_USBSTACK
#if CONFIG_USBOTG == USBOTG_ARC
#define USB_STATUS_BY_EVENT
#define USB_DETECT_BY_REQUEST
#define INCLUDE_TIMEOUT_API
#elif CONFIG_USBOTG == USBOTG_AS3525
#define USB_STATUS_BY_EVENT
#define USB_DETECT_BY_REQUEST
#elif CONFIG_USBOTG == USBOTG_S3C6400X /* FIXME */ && CONFIG_CPU != S5L8701
#define USB_STATUS_BY_EVENT
#define USB_DETECT_BY_REQUEST
#elif CONFIG_USBOTG == USBOTG_DESIGNWARE /* FIXME */ && CONFIG_CPU != S5L8701
#define USB_STATUS_BY_EVENT
#define USB_DETECT_BY_REQUEST
#elif CONFIG_USBOTG == USBOTG_RK27XX
#define USB_DETECT_BY_REQUEST
#elif CONFIG_USBOTG == USBOTG_TNETV105
#define USB_STATUS_BY_EVENT
#define USB_DETECT_BY_REQUEST
#endif /* CONFIG_USB == */
#endif /* HAVE_USBSTACK */

#endif /* !BOOTLOADER */

#if defined(HAVE_USBSTACK) || (CONFIG_CPU == JZ4732) || (CONFIG_CPU == JZ4760B) \
    || (CONFIG_CPU == AS3525) || (CONFIG_CPU == AS3525v2) \
    || defined(CPU_S5L87XX) || (CONFIG_CPU == S3C2440) \
    || defined(APPLICATION) || (CONFIG_CPU == PP5002) \
    || (CONFIG_CPU == RK27XX) || (CONFIG_CPU == IMX233) ||              \
    (defined(HAVE_LCD_COLOR) && (LCD_STRIDEFORMAT == HORIZONTAL_STRIDE))
#define HAVE_SEMAPHORE_OBJECTS
#endif

/*include support for crossfading - requires significant PCM buffer space*/
#if MEMORYSIZE > 2
#define HAVE_CROSSFADE
#endif

/* Determine if accesses should be strictly long aligned. */
#if defined(CPU_ARM) || defined(CPU_MIPS)
#define ROCKBOX_STRICT_ALIGN 1
#endif

/* -Wunterminates-string-initialization will complain if we try to shove
  a "string" into an array that is too small.  Sometimes this actually
  intentional, where you are merely using "string" as a standin for
  "non-terminated sequence of bytes" -- in which case we need to mark
  the "string" as "not actually a string" with an attribute.  Applies to
  GCC >=8, but this warning isn't pulled in by -Wextra until >= 15.
*/
#if __GNUC__ >= 8
#define __NONSTRING __attribute__((__nonstring__))
#else
#define __NONSTRING
#endif

/*
 * These macros are for switching on unified syntax in inline assembly.
 * Older versions of GCC emit assembly in divided syntax with no option
 * to enable unified syntax.
 */
#if (__GNUC__ < 8) && (defined(CPU_ARM_CLASSIC)||defined(CPU_ARM_APPLICATION))
#define BEGIN_ARM_ASM_SYNTAX_UNIFIED ".syntax unified\n"
#define END_ARM_ASM_SYNTAX_UNIFIED   ".syntax divided\n"
#else
#define BEGIN_ARM_ASM_SYNTAX_UNIFIED
#define END_ARM_ASM_SYNTAX_UNIFIED
#endif

#if defined(CPU_ARM) && defined(__ASSEMBLER__)
#if (__GNUC__ < 8)
.syntax unified
#endif
/* ARMv4T doesn't switch the T bit when popping pc directly, we must use BX */
.macro ldmpc cond="", order="ia", regs
#if ARM_ARCH == 4 && defined(USE_THUMB)
    ldm\order\cond sp!, { \regs, lr }
    bx\cond lr
#else
    ldm\order\cond sp!, { \regs, pc }
#endif
.endm
.macro ldrpc cond=""
#if ARM_ARCH == 4 && defined(USE_THUMB)
    ldr\cond lr, [sp], #4
    bx\cond  lr
#else
    ldr\cond pc, [sp], #4
#endif
.endm
#if (__GNUC__ < 8)
.syntax divided
#endif
#endif

#if defined(CPU_COLDFIRE) && defined(__ASSEMBLER__)
/* Assembler doesn't support these as mnemonics but does tpf */
.macro tpf.w
.word 0x51fa
.endm

.macro tpf.l
.word 0x51fb
.endm
#endif

#ifndef CODEC_SIZE
#define CODEC_SIZE 0
#endif

/* This attribute can be used to ensure that certain symbols are never profiled
 * which can be important as profiling a function de-inlines it */
#ifdef RB_PROFILE
#define NO_PROF_ATTR __attribute__ ((no_instrument_function))
#else
#define NO_PROF_ATTR
#endif

/* IRAM usage; not for hosted environments */
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)

/* Core, plugins, and codecs for straightforward targets */
#if defined(CPU_COLDFIRE) || \
    defined(CPU_PP) || \
    defined(CPU_S5L87XX) || \
    (CONFIG_CPU == TCC7801)
# define USE_IRAM

/* AS3525 +2MB: core, plugins, codecs */
#elif (CONFIG_CPU == AS3525) && (MEMORYSIZE > 2) && !defined(BOOTLOADER)
# define USE_IRAM

/* AS3525 2MB: core only */
#elif (CONFIG_CPU == AS3525) && (MEMORYSIZE <= 2) && !defined(BOOTLOADER) && \
      !defined(CODEC) && !defined(PLUGIN)
# define USE_IRAM

/* AS2525v2: core only */
#elif (CONFIG_CPU == AS3525v2) && !defined(BOOTLOADER) && !defined(CODEC) && !defined(PLUGIN)
# define USE_IRAM

/* IMX233: core only */
#elif (CONFIG_CPU == IMX233) && !defined(PLUGIN) && !defined(CODEC)
# define USE_IRAM

/* JZ47xx: core only */
#elif (CONFIG_CPU == JZ4732 || CONFIG_CPU == JZ4760B) && !defined(PLUGIN) && !defined(CODEC)
# define USE_IRAM
#endif

#endif /* (CONFIG_PLATFORM & PLATFORM_NATIVE) */

#ifdef USE_IRAM
# define ICODE_ATTR     __attribute__ ((section(".icode")))
# define ICONST_ATTR    __attribute__ ((section(".irodata")))
# define IDATA_ATTR     __attribute__ ((section(".idata")))
# define IBSS_ATTR      __attribute__ ((section(".ibss")))
#else
# define ICODE_ATTR
# define ICONST_ATTR
# define IDATA_ATTR
# define IBSS_ATTR
#endif

#if (defined(CPU_PP) || (CONFIG_CPU == AS3525) || (CONFIG_CPU == AS3525v2) || \
    (CONFIG_CPU == IMX31L) || (CONFIG_CPU == IMX233) || \
     (CONFIG_CPU == RK27XX) || defined(CPU_MIPS) || defined(CPU_COLDFIRE)) \
    && (CONFIG_PLATFORM & PLATFORM_NATIVE) && !defined(BOOTLOADER)
/* Functions that have INIT_ATTR attached are NOT guaranteed to survive after
 * root_menu() has been called. Their code may be overwritten by other data or
 * code in order to save RAM, and references to them might point into
 * zombie area.
 *
 * It is critical that you make sure these functions are only called before
 * the final call to root_menu() (see apps/main.c) is called (i.e. basically
 * only while main() runs), otherwise things may go wild,
 * from crashes to freezes to exploding daps.
 */

#if defined(__APPLE__) && defined(__MACH__)
    #define INIT_ATTR __attribute__((section ("__INIT,.init")))
    #define INITDATA_ATTR __attribute__((section ("__INITDATA,.initdata")))
#else
    #define INIT_ATTR       __attribute__ ((section(".init")))
    #define INITDATA_ATTR   __attribute__ ((section(".initdata")))
#endif

#define HAVE_INIT_ATTR
#else
#define INIT_ATTR
#define INITDATA_ATTR
#endif

/* We need to call storage_init more than once only if USB storage mode is
 * handled in hardware:
 * Deinit storage -> let hardware handle USB mode -> storage_init() again
 */
#if defined(HAVE_USBSTACK) || defined(USB_NONE)
#define STORAGE_INIT_ATTR INIT_ATTR
#else
#define STORAGE_INIT_ATTR
#endif

#if (CONFIG_PLATFORM & PLATFORM_HOSTED) && defined(__APPLE__)
#define DATA_ATTR       __attribute__ ((section("__DATA, .data")))
#else
#define DATA_ATTR       __attribute__ ((section(".data")))
#endif

#ifndef IRAM_LCDFRAMEBUFFER
/* if the LCD framebuffer has not been moved to IRAM, define it empty here */
#define IRAM_LCDFRAMEBUFFER
#endif

/* Change this if you want to build a single-core firmware for a multicore
 * target for debugging */
#if defined(BOOTLOADER) || (CONFIG_CPU == PP6100)
#define FORCE_SINGLE_CORE
#endif

#if defined(CPU_PP)
#define IDLE_STACK_SIZE  0x80
#define IDLE_STACK_WORDS 0x20

/* Attributes to place data in uncached DRAM */
/* These are useful beyond dual-core and ultimately beyond PP since they may
 * be used for DMA buffers and such without cache maintenence calls. */
#define NOCACHEBSS_ATTR     __attribute__((section(".ncbss"),nocommon))
#define NOCACHEDATA_ATTR    __attribute__((section(".ncdata"),nocommon))

#if !defined(FORCE_SINGLE_CORE)

#define NUM_CORES 2
#define HAVE_CORELOCK_OBJECT
#define CURRENT_CORE current_core()
/* Attributes for core-shared data in DRAM where IRAM is better used for other
 * purposes. */
#define SHAREDBSS_ATTR     NOCACHEBSS_ATTR
#define SHAREDDATA_ATTR    NOCACHEDATA_ATTR

#define IF_COP(...)         __VA_ARGS__
#define IF_COP_VOID(...)    __VA_ARGS__
#define IF_COP_CORE(core)   core

#endif /* !defined(FORCE_SINGLE_CORE) */

#endif /* CPU_PP */

#if CONFIG_CPU == IMX31L || CONFIG_CPU == IMX233
#define NOCACHEBSS_ATTR     __attribute__((section(".ncbss"),nocommon))
#define NOCACHEDATA_ATTR    __attribute__((section(".ncdata"),nocommon))
#endif

#ifndef NUM_CORES
/* Default to single core */
#define NUM_CORES 1
#define CURRENT_CORE    CPU
/* Attributes for core-shared data in DRAM - no caching considerations */
#define SHAREDBSS_ATTR
#define SHAREDDATA_ATTR
#ifndef NOCACHEBSS_ATTR
#define NOCACHEBSS_ATTR
#define NOCACHEDATA_ATTR
#endif

#define IF_COP(...)
#define IF_COP_VOID(...)    void
#define IF_COP_CORE(core)   CURRENT_CORE

#endif /* NUM_CORES */

#ifdef HAVE_HEADPHONE_DETECTION
/* Timeout objects required if headphone detection is enabled */
#define INCLUDE_TIMEOUT_API
#endif /* HAVE_HEADPHONE_DETECTION */

#ifdef HAVE_TOUCHSCREEN
/* Timeout objects required for kinetic list scrolling */
#define INCLUDE_TIMEOUT_API
/* Enable skin variable system, may not be the best place for this #define. */
#define HAVE_SKIN_VARIABLES
#endif /* HAVE_TOUCHSCREEN */

#if defined(HAVE_USB_CHARGING_ENABLE) && defined(HAVE_USBSTACK)
/* USB charging support in the USB stack requires timeout objects */
#define INCLUDE_TIMEOUT_API
#endif /* HAVE_USB_CHARGING_ENABLE && HAVE_USBSTACK */

#if defined(USB_STATUS_BY_EVENT) && defined(HAVE_USBSTACK)
/* Status by event requires timeout for debouncing */
# define INCLUDE_TIMEOUT_API
#endif

#if (!defined(SIMULATOR) && !defined(HAVE_HOSTFS) && !(CONFIG_STORAGE & STORAGE_HOSTFS))
#define STORAGE_GET_INFO
#endif

#if defined(HAVE_SIGALTSTACK_THREADS)
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600   /* For sigaltstack */
#endif
#endif

#ifdef CPU_MIPS
#include <stdbool.h> /* MIPS GCC fix? */
#endif

#if defined(HAVE_USBSTACK)
/* Define the implemented USB transport classes */
#if CONFIG_USBOTG == USBOTG_ISP1583
#define USB_HAS_BULK
#elif (CONFIG_USBOTG == USBOTG_DESIGNWARE)
#define USB_HAS_BULK
#define USB_HAS_INTERRUPT
#define USB_HAS_ISOCHRONOUS
#elif (CONFIG_USBOTG == USBOTG_ARC) ||  \
    (CONFIG_USBOTG == USBOTG_JZ4740) || \
    (CONFIG_USBOTG == USBOTG_JZ4760) || \
    (CONFIG_USBOTG == USBOTG_M66591) || \
    (CONFIG_USBOTG == USBOTG_AS3525) || \
    (CONFIG_USBOTG == USBOTG_RK27XX) || \
    (CONFIG_USBOTG == USBOTG_TNETV105)
#define USB_HAS_BULK
#define USB_HAS_INTERRUPT
#if (CONFIG_USBOTG == USBOTG_ARC)
#define USB_HAS_ISOCHRONOUS
#endif
#elif defined(CPU_TCC780X)
#define USB_HAS_BULK
#elif CONFIG_USBOTG == USBOTG_S3C6400X
#define USB_HAS_BULK
//#define USB_HAS_INTERRUPT -- seems to be broken
#endif /* CONFIG_USBOTG */

#if CONFIG_USBOTG == USBOTG_ARC
#define USB_BATCH_SLOTS 16
#else
#define USB_BATCH_NON_NATIVE
#define USB_BATCH_SLOTS 1
#endif

/* define the class drivers to enable */
#ifdef BOOTLOADER

/* enable usb storage for targets that do bootloader usb */
#if defined(HAVE_BOOTLOADER_USB_MODE)
#define USB_ENABLE_STORAGE
#endif

#else /* BOOTLOADER */

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
#ifdef USB_HAS_BULK
//#define USB_ENABLE_SERIAL
#define USB_ENABLE_STORAGE
#endif /* USB_HAS_BULK */

#ifdef USB_HAS_INTERRUPT
#define USB_ENABLE_HID
#else
#define USB_ENABLE_CHARGING_ONLY
#endif
#endif

#ifdef USB_HAS_ISOCHRONOUS
#define USB_ENABLE_AUDIO
#endif

#if defined(USB_HAS_INTERRUPT) && defined(USB_HAS_ISOCHRONOUS) && USB_VENDOR_ID == 0x05ac
#define USB_ENABLE_IAP
#endif

#if defined(USB_ENABLE_IAP)
#define HAVE_MULTIMEDIA_KEYS
#endif

#endif /* BOOTLOADER */

#endif /* HAVE_USBSTACK */

/* This attribute can be used to enable to detection of plugin file handles leaks.
 * When enabled, the plugin core will monitor open/close/creat and when the plugin exits
 * will display an error message if the plugin leaked some file handles */
#if (CONFIG_PLATFORM & PLATFORM_NATIVE) || defined (SIMULATOR)
#define HAVE_PLUGIN_CHECK_OPEN_CLOSE
#endif

#if defined(CPU_COLDIRE) || CONFIG_CPU == IMX31L
/* Can record and play simultaneously */
#define HAVE_PCM_FULL_DUPLEX
#endif

#if !defined(BOOTLOADER)
#define HAVE_PITCHCONTROL
#endif

/* enable logging messages to disk*/
#if !defined(BOOTLOADER) && !defined(__PCTOOL__)
#define ROCKBOX_HAS_LOGDISKF
#endif

#if defined(HAVE_SDL_AUDIO) \
    && !defined(HAVE_SW_VOLUME_CONTROL)
/* SW volume is needed for accurate control and no double buffering should be
 * required. If target uses SW volume, then its definitions are used instead
 * so things are as on target. */
#define HAVE_SW_VOLUME_CONTROL
#define PCM_SW_VOLUME_UNBUFFERED /* pcm driver itself is buffered */
#ifdef SIMULATOR
/* For sim, nice res for ~ -127dB..+36dB that so far covers all targets */
#define PCM_SW_VOLUME_FRACBITS  (24)
#else
/* For app, use fractional-only setup for -79..+0, no large-integer math */
#define PCM_SW_VOLUME_FRACBITS  (16)
#endif /* SIMULATOR */
#endif /* default SDL SW volume conditions */

#if !defined(BOOTLOADER) || defined(HAVE_BOOTLOADER_SCREENDUMP)
# define HAVE_SCREENDUMP
#endif

#if !defined(BOOTLOADER) && MEMORYSIZE > 2
# define HAVE_PERCEPTUAL_VOLUME
#endif

#if defined(SDMMC_HOST_NUM_SD_CONTROLLERS) || \
    defined(SDMMC_HOST_NUM_MMC_CONTROLLERS)
# define HAVE_SDMMC_HOST
#endif

#if defined(HAVE_SDMMC_HOST) && defined(HAVE_HOTSWAP)
# define HAVE_HOTSWAP_IN_THREAD
#endif

/*
 * Turn off legacy codepage handling in the filesystem code for bootloaders,
 * and support ISO-8859-1 (Latin-1) only. This only affects DOS 8.3 filename
 * parsing when FAT32 long names are unavailable; long names are Unicode and
 * can always be decoded properly regardless of this setting.
 *
 * In reality, bootloaders never supported codepages other than Latin-1 in
 * the first place. They did contain the code to load codepages from disk,
 * but had no way to actually change the codepage away from Latin-1.
 */
#if !defined(BOOTLOADER)
# define HAVE_FILESYSTEM_CODEPAGE
#endif

/* null audiohw setting macro for when codec header is included for reasons
   other than audio support */
#define AUDIOHW_SETTING(name, us, nd, st, minv, maxv, defv, expr...)

/* Trying to enable the setting without the underlying functions doesn't work */
#if defined(HAVE_LCD_SLEEP_SETTING) && !defined(HAVE_LCD_SLEEP)
#error "HAVE_LCD_SLEEP_SETTING requires HAVE_LCD_SLEEP"
#endif

/* Support for unicode codepoints > U+FFFF */
#if (MEMORYSIZE > 2) && !defined(BOOTLOADER)
#define UNICODE32
#endif

#ifdef UNICODE32
#define ucschar_t unsigned int
#else
#define ucschar_t unsigned short
#endif

#ifdef HAVE_SW_VOLUME_CONTROL
#define WANT_SWVOL
#endif

#if defined(PCM_NATIVE_BITDEPTH) && (PCM_NATIVE_BITDEPTH > 16)
#define WANT_SWVOL_32
#endif

#endif /* __CONFIG_H__ */
