#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#
# Mbed TLS 3.6 (Apache-2.0 OR GPL-2.0-or-later, used under the GPL; see
# LICENSE): TLS for HTTPS streams

MBEDTLS_DIR := $(ROOTDIR)/lib/mbedtls
MBEDTLS_SRC := $(call preprocess, $(MBEDTLS_DIR)/SOURCES)
MBEDTLS_OBJ := $(call c2obj, $(MBEDTLS_SRC))

OTHER_SRC += $(MBEDTLS_SRC)

ifneq (,$(strip $(MBEDTLS_SRC)))
LIBMBEDTLS := $(BUILDDIR)/lib/libmbedtls.a
CORE_LIBS := $(LIBMBEDTLS) $(CORE_LIBS)
INCLUDES += -I$(MBEDTLS_DIR)/port -I$(MBEDTLS_DIR)/include
endif

MBEDTLS_FLAGS := $(CFLAGS) -I$(MBEDTLS_DIR)/library \
                 -DMBEDTLS_CONFIG_FILE='"mbedtls_config_rb.h"' \
                 -Wno-undef -Wno-unused-parameter -Wno-cast-align \
                 -Wno-sign-compare -Wno-shadow -Wno-redundant-decls

$(BUILDDIR)/lib/mbedtls/%.o: $(MBEDTLS_DIR)/%.c
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) \
		$(MBEDTLS_FLAGS) -c $< -o $@

$(LIBMBEDTLS): $(MBEDTLS_OBJ)
	$(SILENT)$(shell rm -f $@)
	$(call PRINTS,AR $(@F))$(AR) rcs $@ $^ >/dev/null
