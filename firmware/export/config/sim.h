/* This config file is for all simulators, and is used in addition to the
   target specific configs */

#undef CONFIG_CPU
#define CONFIG_CPU 0



#undef CONFIG_I2C




#undef CONFIG_LCD
#define CONFIG_LCD 0

#undef LCD_DPI /* likely to be too different on a PC */

#undef CONFIG_LED



#undef HAVE_HOTSWAP_STORAGE_AS_MAIN
#undef HAVE_STORAGE_FLUSH


#if defined(CONFIG_CHARGING) && CONFIG_CHARGING > CHARGING_MONITOR
#undef CONFIG_CHARGING
#define CONFIG_CHARGING CHARGING_MONITOR
#endif

/*
 * Effectively disable battery smoothing, our simulated charge/ discharge
 * cycle is way too fast for it otherwise
 */
#define BATT_AVE_SAMPLES 1

#undef CONFIG_USBOTG


#undef HAVE_USBSTACK
#undef USB_VENDOR_ID
#undef USB_PRODUCT_ID


#undef BOOTFILE_EXT
#undef BOOTFILE
#undef BOOTDIR






#undef HAVE_POWEROFF_WHILE_CHARGING

#undef INCLUDE_TIMEOUT_API






#if defined(CONFIG_BACKLIGHT_FADING)
#if     (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_SW_HW_REG)
#undef  CONFIG_BACKLIGHT_FADING
        /* simulate SW_SETTING, as we handle sdl very similary */
#define CONFIG_BACKLIGHT_FADING BACKLIGHT_FADING_SW_SETTING

#elif  (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_PWM || \
        CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_TARGET)
        /* we don't simulate that yet */
#undef  CONFIG_BACKLIGHT_FADING
#endif /* CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_SW_HW_REG */
#endif /* defined(CONFIG_BACKLIGHT_FADING) */

#ifdef  HAVE_BACKLIGHT_BRIGHTNESS
#undef  DEFAULT_BRIGHTNESS_SETTING
/* default for 100% in the sim */
#define DEFAULT_BRIGHTNESS_SETTING MAX_BRIGHTNESS_SETTING
#endif

#define CONFIG_PLATFORM (PLATFORM_HOSTED|PLATFORM_SDL)
#define HAVE_SDL
#define HAVE_SDL_AUDIO

#define _ISOC99_SOURCE 1
