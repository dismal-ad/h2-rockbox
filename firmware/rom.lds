#include "config.h"

ENTRY(start)

#error "Unknown CPU"


#if MEMORYSIZE >= 32
#define PLUGINSIZE PLUGIN_BUFFER_SIZE
#else
#define PLUGINSIZE 0x8000
#endif

#define DRAMORIG 0x09000000
#define IRAMORIG 0x0f000000
#define IRAMSIZE 0x1000
#define FLASHORIG 0x02000000 + ROM_START
#define FLASHSIZE 256K - ROM_START

#ifdef CODECSIZE
#define DRAMSIZE (MEMORYSIZE * 0x100000) - PLUGINSIZE - CODECSIZE
/* Where the codec buffer ends, and the plugin buffer starts */
#define ENDADDR (ENDAUDIOADDR + CODECSIZE)
#else
#define DRAMSIZE (MEMORYSIZE * 0x100000) - PLUGINSIZE
/* Where the audio buffer ends, and the plugin buffer starts */
#define ENDADDR ENDAUDIOADDR
#endif

/* End of the audio buffer, where the codec/plugin buffer starts */
#define ENDAUDIOADDR  (DRAMORIG + DRAMSIZE)
#define CODECORIG ENDAUDIOADDR

MEMORY
{
   DRAM : ORIGIN = DRAMORIG, LENGTH = DRAMSIZE
   IRAM : ORIGIN = IRAMORIG, LENGTH = IRAMSIZE
   FLASH : ORIGIN = FLASHORIG, LENGTH = FLASHSIZE
}

SECTIONS
{
    .flashheader : 
    {
	/* place flash link address first, so the plugin can check */	
        LONG(FLASHORIG);
        /* followed by the start address, the first vector takes care of this. */
        /* If we ever place the table elsewhere, put a constant here. */
    } > FLASH

    .vectors :
    {
        _datacopy = .;
    } > FLASH

    .data : AT ( _datacopy )
    {
        loadaddress = .;
        _loadaddress = .;
        _datastart = .;
        KEEP(*(.resetvectors));
        KEEP(*(.vectors));
        . = ALIGN(0x200);
#ifdef HAVE_INIT_ATTR
        *(.initdata*)
#endif
        *(.data*)
        . = ALIGN(0x4);
        _dataend = .;        
        . = ALIGN(0x10);  /* Maintain proper alignment for .text section */
    } > DRAM
    
    /DISCARD/ : 
    {
        *(.eh_frame)
    }

    /* TRICK ALERT! Newer versions of the linker don't allow output sections
       to overlap even if one of them is empty, so advance the location pointer
       "by hand" */
    .text LOADADDR(.data) + SIZEOF(.data) :
    {
        *(.init.text)
        KEEP(*(.startup*));
#ifdef HAVE_INIT_ATTR
        /* all this symbols are set to the same address so .init copy loop
           will be skiped in crt0.S */
        _initstart = .;
        _initend = .;
        _initcopy = .;
        *(.init*)
#endif
        *(.text*)
        . = ALIGN(0x4);
    } > FLASH

    .rodata :
    {
        *(.rodata*)
        *(.rodata.str1.1)
        *(.rodata.str1.4)
        . = ALIGN(0x4);
        _iramcopy = .;
    } > FLASH

    .iram IRAMORIG : AT ( _iramcopy )
    {
        _iramstart = .;
        *(.icode)
        *(.irodata)
        *(.idata)
        _iramend = .;
    } > IRAM

    .ibss (NOLOAD) :
    {
        _iedata = .;
        *(.ibss)
        . = ALIGN(0x4);
        _iend = .;
    } > IRAM

    .stack :
    {
       *(.stack)
       _stackbegin = .;
       stackbegin = .;
       . += 0x2000;
       _stackend = .;
       stackend = .;
#if IRAMSIZE > 0x1000
    } > IRAM
#else
    } > DRAM
#endif

    .bss :
    {
       _edata = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(0x4);
       _end = .;
    } > DRAM

    .audiobuf ALIGN(4):
    {
        _audiobuffer = .;
        audiobuffer = .;
    } > DRAM

#ifdef CODECSIZE
    .audiobufend ENDAUDIOADDR:
#else
    .audiobufend ENDADDR:
#endif
    {
        _audiobufend = .;
        audiobufend = .;
    } > DRAM

#ifdef CODECSIZE
    .codec ENDAUDIOADDR:
    {
        codecbuf = .;
        _codecbuf = .;
    }
#endif
    
    .plugin ENDADDR:
    {
        _pluginbuf = .;
        pluginbuf = .;
    }
}
