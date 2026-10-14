#ifndef _CONFIG_TREMOR_H
#define _CONFIG_TREMOR_H

#include "codeclib.h" 

#ifndef BYTE_ORDER
#ifdef ROCKBOX_BIG_ENDIAN
#define BIG_ENDIAN 1
#define LITTLE_ENDIAN 0
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#define LITTLE_ENDIAN 1
#define BIG_ENDIAN 0
#endif
#endif

#ifndef ICODE_ATTR_TREMOR_NOT_MDCT
#define ICODE_ATTR_TREMOR_NOT_MDCT ICODE_ATTR
#endif

/* Enable special handling of buffers in faster ram, not usefull when no
   such different ram is available. There are 3 different memory configurations
   * No special iram, uses double buffers to avoid copying data
   * Small special iram, copies buffers to run hottest processing in iram
   * Large iram, uses double buffers in iram */

/* max 2 channels  */
#define CHANNELS 2

// #define _LOW_ACCURACY_

#endif /*  _CONFIG_TREMOR_H */
