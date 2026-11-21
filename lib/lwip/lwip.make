#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#
# lwIP 2.2.1 (BSD licence, see COPYING): the TCP/IP stack for Wi-Fi

LWIP_DIR := $(ROOTDIR)/lib/lwip
LWIP_SRC := $(call preprocess, $(LWIP_DIR)/SOURCES)
LWIP_OBJ := $(call c2obj, $(LWIP_SRC))

OTHER_SRC += $(LWIP_SRC)

ifneq (,$(strip $(LWIP_SRC)))
LIBLWIP := $(BUILDDIR)/lib/liblwip.a
CORE_LIBS := $(LIBLWIP) $(CORE_LIBS)
INCLUDES += -I$(LWIP_DIR)/port/include -I$(LWIP_DIR)/src/include
endif

LWIP_FLAGS := $(CFLAGS) -Wno-undef -Wno-unused-parameter -Wno-format \
              -Wno-address -Wno-cast-align -Wno-sign-compare -Wno-unused-function

$(BUILDDIR)/lib/lwip/%.o: $(LWIP_DIR)/%.c
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) \
		$(LWIP_FLAGS) -c $< -o $@

$(LIBLWIP): $(LWIP_OBJ)
	$(SILENT)$(shell rm -f $@)
	$(call PRINTS,AR $(@F))$(AR) rcs $@ $^ >/dev/null
