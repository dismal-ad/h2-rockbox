#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#

CODECDIR = $(RBCODEC_BLD)/codecs
CODECS_SRC := $(call preprocess, $(RBCODECLIB_DIR)/codecs/SOURCES)
OTHER_SRC += $(CODECS_SRC)

CODECS := $(CODECS_SRC:.c=.codec)
CODECS := $(subst $(RBCODECLIB_DIR),$(RBCODEC_BLD),$(CODECS))

# the codec helper library
include $(RBCODECLIB_DIR)/codecs/lib/libcodec.make
OTHER_INC += -I$(RBCODECLIB_DIR)/codecs/lib

# extra libraries.  libarm_support has the division routines, which are much
# faster than what libgcc may supply; it is empty when not building for ARM.
CODEC_LIBS := $(CODECLIB) $(FIXEDPOINTLIB) $(ARMSUPPORTLIB)

# compile flags for codecs
CODECFLAGS := $(CFLAGS) $(RBCODEC_CFLAGS) -fstrict-aliasing \
			 -I$(RBCODECLIB_DIR)/codecs -I$(RBCODECLIB_DIR)/codecs/lib -DCODEC

ifdef APP_TYPE
 CODECLDFLAGS = $(SHARED_LDFLAGS)
 ifneq ($(UNAME), Darwin)
  CODECLDFLAGS += -Wl,--gc-sections
 endif
 CODECFLAGS += $(SHARED_CFLAGS) # <-- from Makefile
else
 CODECLDFLAGS = -T$(CODECLINK_LDS) -Wl,--gc-sections
endif
CODECLDFLAGS += -Wl,$(LDMAP_OPT),$(CODECDIR)/$*.map $(GLOBAL_LDOPTS)

ifdef USE_LTO
 CODECLDFLAGS += -fno-builtin -ffreestanding
 CODECFLAGS += -fno-builtin -ffreestanding
 CODECLDFLAGS += -e __header
endif


# the codec libraries
include $(RBCODECLIB_DIR)/codecs/demac/libdemac.make
include $(RBCODECLIB_DIR)/codecs/libalac/libalac.make
include $(RBCODECLIB_DIR)/codecs/libasf/libasf.make
include $(RBCODECLIB_DIR)/codecs/libfaad/libfaad.make
include $(RBCODECLIB_DIR)/codecs/libffmpegFLAC/libffmpegFLAC.make
include $(RBCODECLIB_DIR)/codecs/libm4a/libm4a.make
include $(RBCODECLIB_DIR)/codecs/libmad/libmad.make
include $(RBCODECLIB_DIR)/codecs/libtremor/libtremor.make
include $(RBCODECLIB_DIR)/codecs/libwavpack/libwavpack.make
include $(RBCODECLIB_DIR)/codecs/libwma/libwma.make
include $(RBCODECLIB_DIR)/codecs/libwmapro/libwmapro.make
include $(RBCODECLIB_DIR)/codecs/libpcm/libpcm.make
include $(RBCODECLIB_DIR)/codecs/libopus/libopus.make

ifeq ($(shell expr $(GCCNUM) \> 600),1)
$(WMALIB): CODECFLAGS += -Wno-shift-negative-value
endif

ifndef DEBUG
# set CODECFLAGS per codec lib, since gcc takes the last -Ox and the last
# in a -ffoo -fno-foo pair, there is no need to filter them out
$(CODECLIB) : CODECFLAGS += -O1
$(ALACLIB) : CODECFLAGS += -O1
$(ASFLIB) : CODECFLAGS += -O2
$(DEMACLIB) : CODECFLAGS += -O3
$(FAADLIB) : CODECFLAGS += -O2
$(FFMPEGFLACLIB) : CODECFLAGS += -O2
$(M4ALIB) : CODECFLAGS += -O3
$(MADLIB) : CODECFLAGS += -O2
$(OPUSLIB) : CODECFLAGS +=  -O2
$(PCMSLIB) : CODECFLAGS += -O1
$(TREMORLIB) : CODECFLAGS += -O2
$(WAVPACKLIB) : CODECFLAGS += -O1
$(WMALIB) : CODECFLAGS += -O2
$(WMAPROLIB) : CODECFLAGS += -O1

# fine-tuning of CODECFLAGS per cpu arch
ifeq ($(ARCH),arch_mips)
  $(OPUSLIB) : CODECFLAGS += -fno-strict-aliasing
endif

endif #ifndef DEBUG

ifndef APP_TYPE
  CONFIGFILE := $(FIRMDIR)/export/config/$(MODELNAME).h
  CODEC_LDS := $(RBCODECLIB_DIR)/codecs/codec.lds
  CODECLINK_LDS := $(CODECDIR)/codec.link
endif

CODEC_CRT0 := $(CODECDIR)/codec_crt0.o

$(CODECS): $(CODEC_CRT0) $(CODECLINK_LDS)

$(CODECLINK_LDS): $(CODEC_LDS) $(CONFIGFILE)
	$(call PRINTS,PP $(@F))
	$(shell mkdir -p $(dir $@))
	$(call preprocess2file, $<, $@, -DCODEC)

# codec/library dependencies
$(CODECDIR)/mpa.codec : $(CODECDIR)/libmad.a $(CODECDIR)/libasf.a
$(CODECDIR)/flac.codec : $(CODECDIR)/libffmpegFLAC.a
$(CODECDIR)/vorbis.codec : $(CODECDIR)/libtremor.a $(TLSFLIB) $(SETJMPLIB)
$(CODECDIR)/wavpack.codec : $(CODECDIR)/libwavpack.a
$(CODECDIR)/alac.codec : $(CODECDIR)/libalac.a $(CODECDIR)/libm4a.a
$(CODECDIR)/aac.codec : $(CODECDIR)/libfaad.a $(CODECDIR)/libm4a.a
$(CODECDIR)/shorten.codec : $(CODECDIR)/libffmpegFLAC.a
$(CODECDIR)/ape-pre.map : $(CODECDIR)/libdemac-pre.a
$(CODECDIR)/ape.codec : $(CODECDIR)/libdemac.a
$(CODECDIR)/wma.codec : $(CODECDIR)/libwma.a $(CODECDIR)/libasf.a
$(CODECDIR)/wmapro.codec : $(CODECDIR)/libwmapro.a $(CODECDIR)/libasf.a
$(CODECDIR)/wavpack_enc.codec: $(CODECDIR)/libwavpack.a
$(CODECDIR)/aiff.codec : $(CODECDIR)/libpcm.a
$(CODECDIR)/wav.codec : $(CODECDIR)/libpcm.a
$(CODECDIR)/smaf.codec : $(CODECDIR)/libpcm.a
$(CODECDIR)/au.codec : $(CODECDIR)/libpcm.a
$(CODECDIR)/vox.codec : $(CODECDIR)/libpcm.a
$(CODECDIR)/wav64.codec : $(CODECDIR)/libpcm.a
$(CODECDIR)/opus.codec : $(CODECDIR)/libopus.a $(TLSFLIB)
$(CODECDIR)/aac_bsf.codec : $(CODECDIR)/libfaad.a

$(CODECS): $(CODEC_LIBS) # this must be last in codec dependency list

# pattern rule for compiling codecs
$(CODECDIR)/%.o: $(RBCODECLIB_DIR)/codecs/%.c
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) \
		-I$(dir $<) $(CODECFLAGS) -c $< -o $@

# pattern rule for compiling codecs
$(CODECDIR)/%.o: $(RBCODECLIB_DIR)/codecs/%.S
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) \
		-I$(dir $<) $(CODECFLAGS) -c $< -o $@

$(CODECDIR)/%-pre.map: $(CODEC_CRT0) $(CODECLINK_LDS) $(CODECDIR)/%.o $(CODEC_LIBS)
	$(call PRINTS,LD $(@F))$(CC) $(CODECFLAGS) -o $(CODECDIR)/$*-pre.elf \
		$(filter %.o, $^) \
		$(filter-out $(CODECLIB),$(filter %.a, $+)) $(CODECLIB) \
		-lgcc $(subst .map,-pre.map,$(CODECLDFLAGS))

$(CODECDIR)/%.codec: $(CODECDIR)/%.o
	$(call PRINTS,LD $(@F))$(CC) $(CODECFLAGS) -o $(CODECDIR)/$*.elf \
		$(filter %.o, $^) \
		$(filter %.a, $+) \
		-lgcc $(CODECLDFLAGS)
	$(SILENT)$(call objcopy_plugin,$(CODECDIR)/$*.elf,$@)
