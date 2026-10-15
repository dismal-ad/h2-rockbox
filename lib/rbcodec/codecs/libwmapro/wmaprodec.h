#include "codeclib.h"
#include "wma.h"
#include "../libasf/asf.h"

/* Not enough IRAM available. */
#define IBSS_ATTR_WMAPRO_LARGE_IRAM
#define ICODE_ATTR_WMAPRO_LARGE_IRAM
#define ICONST_ATTR_WMAPRO_LARGE_IRAM
#define IBSS_ATTR_WMAPRO_VLC_TABLES
/* Models with large IRAM put tmp to IRAM rather than window coefficients as
 * this is the fastest option. On models with smaller IRAM the 2nd-best option
 * is to move the window coefficients to IRAM. */
#define ICONST_ATTR_WMAPRO_WIN_VS_TMP ICONST_ATTR

int decode_init(asf_waveformatex_t *wfx);
int decode_packet(asf_waveformatex_t *wfx,
                  int32_t *dec[2], int *data_size, void* pktdata, int size);
