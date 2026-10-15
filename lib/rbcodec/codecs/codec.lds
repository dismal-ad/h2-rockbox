#define __ASSEMBLER__
#include "config.h"
#include "cpu.h"

/* These output formats should be in the config-files */

#if   defined(CPU_MIPS)
OUTPUT_FORMAT(elf32-littlemips)
#else
/* We can have an #error here we don't use this file when build sims! */
#error Unknown CPU architecture
#endif

/* MCF5249 have 96KB of IRAM */
#if   CONFIG_CPU == X1000
#define DRAMORIG X1000_DRAM_BASE
#define DRAMSIZE (X1000_DRAM_SIZE - PLUGIN_BUFFER_SIZE - CODEC_SIZE)

#else
# error "DRAM/IRAM memory map not defined!"
#endif

#define PLUGIN_LENGTH PLUGIN_BUFFER_SIZE


#ifndef CODEC_ORIGIN /* targets can specify another origin */
#define CODEC_ORIGIN (DRAMORIG + (DRAMSIZE))
#endif

#ifndef PLUGIN_ORIGIN /* targets can specify another origin */
#define PLUGIN_ORIGIN (CODEC_ORIGIN + CODEC_SIZE)
#endif

#ifdef CODEC
#define THIS_LENGTH CODEC_SIZE
#define THIS_ORIGIN CODEC_ORIGIN
#elif defined OVERLAY_OFFSET
#define THIS_LENGTH (DRAMSIZE - OVERLAY_OFFSET)
#define THIS_ORIGIN (DRAMORIG + OVERLAY_OFFSET)
#elif defined IMGVDECODER_OFFSET
#define THIS_LENGTH (PLUGIN_LENGTH - IMGVDECODER_OFFSET)
#define THIS_ORIGIN (PLUGIN_ORIGIN + IMGVDECODER_OFFSET)
#else /* plugin */
#define THIS_LENGTH PLUGIN_LENGTH
#define THIS_ORIGIN PLUGIN_ORIGIN
#endif

MEMORY
{
   PLUGIN_RAM : ORIGIN = THIS_ORIGIN, LENGTH = THIS_LENGTH
}

SECTIONS
{
    .header : {
        _plugin_start_addr = .;
        plugin_start_addr = .;
        KEEP(*(.header))
    } > PLUGIN_RAM

    .text :
    {
        *(.text*)
    } > PLUGIN_RAM

    .rodata :
    {
        *(.rodata*)
    } > PLUGIN_RAM

    .data :
    {
        *(.data*)
    } > PLUGIN_RAM

#if defined(NOCACHE_BASE)
    /*
     * Allocate .ncdata based on the following constraints:
     *
     * 1. the LMA must be allocated at the next free LMA in
     *    PLUGIN_RAM and aligned to a cacheline boundary.
     * 2. the VMA - LMA offset must equal NOCACHE_BASE.
     */
    .ncdata ALIGN(. + NOCACHE_BASE, CACHEALIGN_SIZE) : ALIGN(CACHEALIGN_SIZE)
    {
        *(.ncdata*)
        . = ALIGN(CACHEALIGN_SIZE);
    } AT> PLUGIN_RAM
#endif

    /DISCARD/ :
    {
        *(.eh_frame)
		*(.icode)
        *(.irodata)
        *(.idata)
        *(.ibss)
#ifdef CPU_MIPS
        *(.MIPS.abiflags)
        *(.rel.dyn)
#endif
    }

    .bss (NOLOAD) :
    {
    	plugin_bss_start = .;
    	_plugin_bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(0x4);
    } > PLUGIN_RAM

#if defined(NOCACHE_BASE)
    /*
     * .ncbss has the same constraints as the .ncdata section
     * above but there is an extra complication: because it is
     * a NOLOAD section, unlike .ncdata it cannot consume any
     * VMAs or LMAs in PLUGIN_RAM itself.
     *
     * Because free VMA/LMAs in PLUGIN_RAM are unchanged, any
     * subsequent sections will get placed at the end of the
     * .bss section, overlapping .ncbss, which we don't want.
     *
     * One solution is to manually place the next sections to
     * account for the size of .ncbss, but a cleaner solution
     * is to define a dummy section (.ncbss_vma) allocated at
     * the same VMA as .ncbss and equal in size, and placed in
     * the PLUGIN_RAM region. This allocates the correct VMA
     * range in plugin RAM so that the next section will not
     * overlap .ncbss.
     */
    .ncbss ALIGN(. + NOCACHE_BASE, CACHEALIGN_SIZE) (NOLOAD) : ALIGN(CACHEALIGN_SIZE)
    {
    	*(.ncbss*)
        . = ALIGN(CACHEALIGN_SIZE);
    } AT> PLUGIN_RAM

    .ncbss_vma (NOLOAD) : ALIGN(CACHEALIGN_SIZE)
    {
        . += SIZEOF(.ncbss);
    } > PLUGIN_RAM
#endif

    /* Final end of plugin after IRAM setup. The plugin or codec buffer
       is considered unused by the in-RAM image at this point once IRAM
       is copied. */
    .pluginend (NOLOAD) :
    {
        _plugin_end_addr = .;
        plugin_end_addr = .;
    } > PLUGIN_RAM

    loadendaddr = plugin_end_addr;

    /* This is for ovl_offset.pl and is the highest address that must
       be loaded into the plugin buffer (past the end of last data in
       stored image). */
    .pluginloadend loadendaddr :
    {
    	_plugin_load_end_addr = .;
    	plugin_load_end_addr = .;
    }

    _plugin_ram_end = ORIGIN(PLUGIN_RAM) + LENGTH(PLUGIN_RAM);
    plugin_ram_end = ORIGIN(PLUGIN_RAM) + LENGTH(PLUGIN_RAM);

    /* Special trick to avoid a linker error when no other sections are
       left after garbage collection (plugin not for this platform) */
    .comment 0 :
    {
        KEEP(*(.comment))
    }
}

#if defined(NOCACHE_BASE)
/* Some asserts to make sure nocache sections appear correctly defined */
ASSERT(LOADADDR(.ncdata) == ADDR(.ncdata) - NOCACHE_BASE, ".ncdata has incorrect LMA/VMA address");
ASSERT(LOADADDR(.ncdata) % CACHEALIGN_SIZE == 0,          ".ncdata incorrectly aligned");
ASSERT(LOADADDR(.ncbss) == ADDR(.ncbss) - NOCACHE_BASE,   ".ncbss has incorrect LMA/VMA address");
ASSERT(LOADADDR(.ncbss) % CACHEALIGN_SIZE == 0,           ".ncbss incorrectly aligned");
ASSERT(ADDR(.ncbss_vma) == LOADADDR(.ncbss),              ".ncbss_vma has wrong address");
ASSERT(SIZEOF(.ncbss_vma) == SIZEOF(.ncbss),              ".ncbss_vma has wrong size");
#endif
