/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
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

/* Driver for Broadcom/Cypress FullMAC Wi-Fi chips on SDIO (written for the
 * CYW43438/BCM43430 A1). The bring-up follows what Linux's brcmfmac does:
 * the AI backplane through function 1, the firmware and NVRAM downloaded
 * to the chip's RAM, then the SDPCM protocol on function 2 with BCDC
 * control messages, firmware events and Ethernet frames.
 *
 * Everything is polled: a driver thread reads the chip's interrupt status
 * every tick while it runs. One mutex serialises the bus. */

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "thread.h"
#include "file.h"
#include "wifi.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "string-extra.h"

#define FW_FILE     WIFI_FW_DIR "cyfmac43430-sdio.bin"
#define NVRAM_FILE  WIFI_FW_DIR "nvram.txt"
#define CLM_FILE    WIFI_FW_DIR "cyfmac43430-sdio.clm_blob"

/***************************************************************************
 * Log: a small ring of text lines, shown by the Wi-Fi screen
 ***************************************************************************/

#define WLOG_SIZE 4096
static char wlog[WLOG_SIZE];
static size_t wlog_pos;
static bool wlog_wrapped;

static void wlog_add(const char *fmt, ...) ATTRIBUTE_PRINTF(1, 2);
static void wlog_add(const char *fmt, ...)
{
    char line[128];
    int n = snprintf(line, sizeof(line), "%4ld.%02ld ",
                     current_tick / HZ, current_tick % HZ);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    size_t len = strlen(line);
    line[len++] = '\n';
    for (size_t i = 0; i < len; ++i) {
        wlog[wlog_pos++] = line[i];
        if (wlog_pos >= WLOG_SIZE) {
            wlog_pos = 0;
            wlog_wrapped = true;
        }
    }
}

size_t wifi_hw_log(char *out, size_t size)
{
    size_t n = 0;
    if (!size)
        return 0;
    if (wlog_wrapped) {
        /* from the first full line after the write position */
        size_t s = wlog_pos;
        while (s < WLOG_SIZE && wlog[s] != '\n')
            s++;
        for (size_t i = s + 1; i < WLOG_SIZE && n + 1 < size; ++i)
            out[n++] = wlog[i];
    }
    for (size_t i = 0; i < wlog_pos && n + 1 < size; ++i)
        out[n++] = wlog[i];
    out[n] = '\0';
    return n;
}

/***************************************************************************
 * Chip constants
 ***************************************************************************/

/* Function 1 registers */
#define SBSDIO_WATERMARK            0x10008
#define SBSDIO_FUNC1_SBADDRLOW      0x1000A
#define SBSDIO_FUNC1_SBADDRMID      0x1000B
#define SBSDIO_FUNC1_SBADDRHIGH     0x1000C
#define SBSDIO_FUNC1_FRAMECTRL      0x1000D
#define SBSDIO_FUNC1_CHIPCLKCSR     0x1000E
#define SFC_RF_TERM                 0x01
#define SBSDIO_FORCE_ALP            0x01
#define SBSDIO_FORCE_HT             0x02
#define SBSDIO_ALP_AVAIL_REQ        0x08
#define SBSDIO_HT_AVAIL_REQ         0x10
#define SBSDIO_FORCE_HW_CLKREQ_OFF  0x20
#define SBSDIO_ALP_AVAIL            0x40
#define SBSDIO_HT_AVAIL             0x80

#define SB_OFT_ADDR_MASK            0x07FFF
#define SB_ACCESS_2_4B_FLAG         0x08000
#define SB_WINDOW_SIZE              0x08000

#define F1_BLOCK_SIZE               64
#define F2_BLOCK_SIZE               512

/* AI (AXI) core wrapper registers */
#define BCMA_IOCTL                  0x0408
#define BCMA_RESET_CTL              0x0800
#define BCMA_IOCTL_CLK              0x0001
#define BCMA_IOCTL_FGC              0x0002
#define BCMA_RESET_CTL_RESET        0x0001
#define D11_IOCTL_PHYCLOCKEN        0x0004
#define D11_IOCTL_PHYRESET          0x0008

#define CORE_INTERNAL_MEM           0x80E
#define CORE_80211                  0x812
#define CORE_SDIO_DEV               0x829
#define CORE_ARM_CM3                0x82A
#define SI_ENUM_BASE                0x18000000

#define SOCRAM_BANKIDX              0x10
#define SOCRAM_BANKPDA              0x44

/* SDIO device core registers */
#define SD_INTSTATUS                0x20
#define SD_HOSTINTMASK              0x24
#define SD_TOSBMAILBOX              0x40
#define SD_TOSBMAILBOXDATA          0x48
#define SD_TOHOSTMAILBOXDATA        0x4C
#define I_HMB_FRAME_IND             (1 << 6)
#define I_HMB_HOST_INT              (1 << 7)
#define I_HMB_SW_MASK               0x000000f0
#define I_CHIPACTIVE                (1 << 29)
#define HOSTINTMASK                 (I_HMB_SW_MASK | I_CHIPACTIVE)
#define SMB_NAK                     (1 << 0)
#define SMB_INT_ACK                 (1 << 1)
#define HMB_DATA_FWHALT             0x10
#define SDPCM_PROT_VERSION          4

#define CHIP_RAM_SIZE               0x80000     /* 43430: 512 KiB at 0 */

/* SDPCM / BCDC */
#define SDPCM_HDR_LEN               12
#define SDPCM_CHAN_CONTROL          0
#define SDPCM_CHAN_EVENT            1
#define SDPCM_CHAN_DATA             2
#define SDPCM_CHAN_GLOM             3
#define BCDC_HDR_LEN                16
#define BCDC_DCMD_ERROR             0x01
#define BCDC_DCMD_SET               0x02
#define BDC_HDR_LEN                 4
#define BDC_PROTO_VER               2
#define F2_ADDR                     (SB_ACCESS_2_4B_FLAG)   /* FIFO */
#define FIRSTREAD                   64

/* ioctls */
#define WLC_UP                      2
#define WLC_DOWN                    3
#define WLC_SET_INFRA               20
#define WLC_SET_AUTH                22
#define WLC_SET_SSID                26
#define WLC_DISASSOC                52
#define WLC_SET_PM                  86
#define WLC_GET_RSSI                127
#define WLC_SET_WSEC                134
#define WLC_SET_WPA_AUTH            165
#define WLC_GET_VAR                 262
#define WLC_SET_VAR                 263
#define WLC_SET_WSEC_PMK            268

#define WSEC_TKIP                   0x2
#define WSEC_AES                    0x4
#define WPA_AUTH_DISABLED           0x0000
#define WPA_AUTH_PSK                0x0004
#define WPA2_AUTH_PSK               0x0080
#define WSEC_PASSPHRASE             0x0001
#define PM_FAST                     2

/* events */
#define E_SET_SSID                  0
#define E_JOIN                      1
#define E_AUTH                      3
#define E_DEAUTH                    5
#define E_DEAUTH_IND                6
#define E_ASSOC                     7
#define E_DISASSOC                  11
#define E_DISASSOC_IND              12
#define E_LINK                      16
#define E_PSK_SUP                   46
#define E_ESCAN_RESULT              69
#define E_STATUS_SUCCESS            0
#define E_STATUS_NO_NETWORKS        3
#define E_STATUS_PARTIAL            8
#define EVENT_FLAG_LINK             0x01
#define SUP_KEYED                   6

/***************************************************************************
 * State
 ***************************************************************************/

static const struct wifi_callbacks *cbs;
static struct mutex wl_mtx;
static volatile bool running;
static bool fw_halted;

static uint32_t sb_window;
static bool sb_window_valid;

struct core {
    uint16_t id;
    uint32_t base;
    uint32_t wrap;
};
static struct core cores[16];
static int nr_cores;
static struct core *sdio_core;

static uint8_t tx_seq, tx_max;
static uint16_t req_id;
static bool rx_pending;

/* Pending control response */
static bool ctl_waiting, ctl_done;
static uint16_t ctl_id;
static uint8_t *ctl_out;
static size_t ctl_outmax, ctl_outlen;
static int32_t ctl_status;

static uint8_t rxbuf[4096 + 64] CACHEALIGN_ATTR;
static uint8_t txbuf[2048] CACHEALIGN_ATTR;
static uint8_t ioc_buf[1600];

static uint8_t mac_addr[6];
static char fw_version[80];

/* joining */
static bool join_active, join_link, join_keyed, join_need_keys, link_reported;

/* scanning */
#define MAX_BSS 40
static struct wifi_bss *scan_list;
static int scan_max, scan_count;
static bool scan_running, scan_done;

/* entropy: timings and received bytes stirred into a small pool */
static uint8_t ent_pool[64];
static unsigned ent_pos;

static void ent_stir(const uint8_t *p, size_t n)
{
    uint32_t t = wifi_target_usec();
    ent_pool[ent_pos++ % sizeof(ent_pool)] ^= t ^ (t >> 8);
    for (size_t i = 0; i < n; ++i) {
        uint8_t *e = &ent_pool[(ent_pos + i) % sizeof(ent_pool)];
        *e = (uint8_t)((*e << 1) | (*e >> 7)) ^ p[i];
    }
    ent_pos += n;
}

size_t wifi_hw_entropy(uint8_t *out, size_t len)
{
    size_t n = MIN(len, sizeof(ent_pool));
    uint32_t t = wifi_target_usec();
    for (size_t i = 0; i < n; ++i)
        out[i] = ent_pool[i] ^ (uint8_t)(t >> ((i & 3) * 8));
    ent_stir((uint8_t *)&t, sizeof(t));
    return n;
}

/***************************************************************************
 * Little helpers
 ***************************************************************************/

static uint16_t get16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t get32(const uint8_t *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t get16be(const uint8_t *p) { return (p[0] << 8) | p[1]; }
static uint32_t get32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

#define rd8  wifi_sdio_rd8
#define wr8  wifi_sdio_wr8

/***************************************************************************
 * Backplane
 ***************************************************************************/

static int set_window(uint32_t addr)
{
    uint32_t w = addr & ~SB_OFT_ADDR_MASK;
    if (sb_window_valid && w == sb_window)
        return 0;
    sb_window_valid = false;
    if (wr8(1, SBSDIO_FUNC1_SBADDRLOW, (w >> 8) & 0x80) ||
        wr8(1, SBSDIO_FUNC1_SBADDRMID, (w >> 16) & 0xff) ||
        wr8(1, SBSDIO_FUNC1_SBADDRHIGH, (w >> 24) & 0xff))
        return -1;
    sb_window = w;
    sb_window_valid = true;
    return 0;
}

static int bp_read32(uint32_t addr, uint32_t *val)
{
    uint8_t b[4];
    if (set_window(addr))
        return -1;
    if (wifi_sdio_rw(false, 1, (addr & SB_OFT_ADDR_MASK) | SB_ACCESS_2_4B_FLAG,
                     true, b, 4, 0))
        return -1;
    *val = get32(b);
    return 0;
}

static int bp_write32(uint32_t addr, uint32_t val)
{
    uint8_t b[4];
    put32(b, val);
    if (set_window(addr))
        return -1;
    return wifi_sdio_rw(true, 1, (addr & SB_OFT_ADDR_MASK) | SB_ACCESS_2_4B_FLAG,
                        true, b, 4, 0);
}

static uint32_t rd32(uint32_t addr)
{
    uint32_t v = 0xffffffff;
    bp_read32(addr, &v);
    return v;
}

/* Memory transfers through the 32 KiB window; len multiple of 4 */
static int bp_mem(bool wr, uint32_t addr, uint8_t *data, size_t len)
{
    while (len) {
        size_t n = MIN(len, (size_t)2048);
        n = MIN(n, (size_t)(SB_WINDOW_SIZE - (addr & SB_OFT_ADDR_MASK)));
        if (set_window(addr))
            return -1;
        if (wifi_sdio_rw(wr, 1, (addr & SB_OFT_ADDR_MASK) | SB_ACCESS_2_4B_FLAG,
                         true, data, n, F1_BLOCK_SIZE))
            return -1;
        addr += n;
        data += n;
        len -= n;
    }
    return 0;
}

static struct core *find_core(uint16_t id)
{
    for (int i = 0; i < nr_cores; ++i)
        if (cores[i].id == id)
            return &cores[i];
    return NULL;
}

/* Enumeration ROM scan, as brcmf_chip_dmp_erom_scan() */
#define DMP_DESC_TYPE_MSK       0x0000000F
#define DMP_DESC_EMPTY          0x00000000
#define DMP_DESC_VALID          0x00000001
#define DMP_DESC_COMPONENT      0x00000001
#define DMP_DESC_MASTER_PORT    0x00000003
#define DMP_DESC_ADDRESS        0x00000005
#define DMP_DESC_ADDRSIZE_GT32  0x00000008
#define DMP_DESC_EOT            0x0000000F
#define DMP_COMP_PARTNUM        0x000FFF00
#define DMP_COMP_PARTNUM_S      8
#define DMP_COMP_NUM_SWRAP      0x00F80000
#define DMP_COMP_NUM_SWRAP_S    19
#define DMP_COMP_NUM_MWRAP      0x0007C000
#define DMP_COMP_NUM_MWRAP_S    14
#define DMP_SLAVE_ADDR_BASE     0xFFFFF000
#define DMP_SLAVE_TYPE          0x000000C0
#define DMP_SLAVE_TYPE_S        6
#define DMP_SLAVE_TYPE_SLAVE    0
#define DMP_SLAVE_TYPE_SWRAP    2
#define DMP_SLAVE_TYPE_MWRAP    3
#define DMP_SLAVE_SIZE_TYPE     0x00000030
#define DMP_SLAVE_SIZE_TYPE_S   4
#define DMP_SLAVE_SIZE_4K       0
#define DMP_SLAVE_SIZE_8K       1
#define DMP_SLAVE_SIZE_DESC     3

static uint32_t erom_get(uint32_t *ea, uint8_t *type)
{
    uint32_t val = rd32(*ea);
    *ea += 4;
    if (type) {
        *type = val & DMP_DESC_TYPE_MSK;
        if ((*type & ~DMP_DESC_ADDRSIZE_GT32) == DMP_DESC_ADDRESS)
            *type = DMP_DESC_ADDRESS;
    }
    return val;
}

static int erom_regaddr(uint32_t *ea, uint32_t *regbase, uint32_t *wrapbase)
{
    uint8_t desc;
    uint8_t wraptype;
    uint32_t val = erom_get(ea, &desc);

    if (desc == DMP_DESC_MASTER_PORT) {
        wraptype = DMP_SLAVE_TYPE_MWRAP;
    } else if (desc == DMP_DESC_ADDRESS) {
        *ea -= 4;
        wraptype = DMP_SLAVE_TYPE_SWRAP;
    } else {
        *ea -= 4;
        return -1;
    }

    int guard = 0;
    do {
        do {
            val = erom_get(ea, &desc);
            if (desc == DMP_DESC_EOT || ++guard > 64) {
                *ea -= 4;
                return -1;
            }
        } while (desc != DMP_DESC_ADDRESS && desc != DMP_DESC_COMPONENT);

        if (desc == DMP_DESC_COMPONENT) {
            *ea -= 4;
            return 0;
        }
        if (val & DMP_DESC_ADDRSIZE_GT32)
            erom_get(ea, NULL);
        uint8_t sztype = (val & DMP_SLAVE_SIZE_TYPE) >> DMP_SLAVE_SIZE_TYPE_S;
        if (sztype == DMP_SLAVE_SIZE_DESC) {
            uint32_t szdesc = erom_get(ea, NULL);
            if (szdesc & DMP_DESC_ADDRSIZE_GT32)
                erom_get(ea, NULL);
        }
        if (sztype != DMP_SLAVE_SIZE_4K && sztype != DMP_SLAVE_SIZE_8K)
            continue;
        uint8_t stype = (val & DMP_SLAVE_TYPE) >> DMP_SLAVE_TYPE_S;
        if (*regbase == 0 && stype == DMP_SLAVE_TYPE_SLAVE)
            *regbase = val & DMP_SLAVE_ADDR_BASE;
        if (*wrapbase == 0 && stype == wraptype)
            *wrapbase = val & DMP_SLAVE_ADDR_BASE;
    } while (*regbase == 0 || *wrapbase == 0);
    return 0;
}

static void erom_scan(void)
{
    uint32_t ea = rd32(SI_ENUM_BASE + 0xFC);
    uint8_t type = 0;
    int guard = 0;

    nr_cores = 0;
    while (type != DMP_DESC_EOT && ++guard < 256 &&
           nr_cores < (int)ARRAYLEN(cores)) {
        uint32_t val = erom_get(&ea, &type);
        if (!(val & DMP_DESC_VALID) || type == DMP_DESC_EMPTY ||
            type != DMP_DESC_COMPONENT)
            continue;
        uint16_t id = (val & DMP_COMP_PARTNUM) >> DMP_COMP_PARTNUM_S;

        val = erom_get(&ea, &type);
        if ((val & DMP_DESC_TYPE_MSK) != DMP_DESC_COMPONENT)
            break;
        unsigned nmw = (val & DMP_COMP_NUM_MWRAP) >> DMP_COMP_NUM_MWRAP_S;
        unsigned nsw = (val & DMP_COMP_NUM_SWRAP) >> DMP_COMP_NUM_SWRAP_S;
        if (nmw + nsw == 0)
            continue;

        uint32_t base = 0, wrap = 0;
        if (erom_regaddr(&ea, &base, &wrap))
            continue;
        cores[nr_cores].id = id;
        cores[nr_cores].base = base;
        cores[nr_cores].wrap = wrap;
        nr_cores++;
    }
}

static bool core_is_up(struct core *c)
{
    uint32_t ioctl = rd32(c->wrap + BCMA_IOCTL);
    uint32_t rst = rd32(c->wrap + BCMA_RESET_CTL);
    return (ioctl & (BCMA_IOCTL_FGC | BCMA_IOCTL_CLK)) == BCMA_IOCTL_CLK &&
           !(rst & BCMA_RESET_CTL_RESET);
}

static void core_disable(struct core *c, uint32_t prereset, uint32_t reset)
{
    uint32_t v = rd32(c->wrap + BCMA_RESET_CTL);
    if (!(v & BCMA_RESET_CTL_RESET)) {
        bp_write32(c->wrap + BCMA_IOCTL, prereset | BCMA_IOCTL_FGC | BCMA_IOCTL_CLK);
        rd32(c->wrap + BCMA_IOCTL);
        bp_write32(c->wrap + BCMA_RESET_CTL, BCMA_RESET_CTL_RESET);
        udelay(20);
        for (int i = 0; i < 30 &&
             rd32(c->wrap + BCMA_RESET_CTL) != BCMA_RESET_CTL_RESET; ++i)
            udelay(10);
    }
    bp_write32(c->wrap + BCMA_IOCTL, reset | BCMA_IOCTL_FGC | BCMA_IOCTL_CLK);
    rd32(c->wrap + BCMA_IOCTL);
}

static void core_reset(struct core *c, uint32_t prereset, uint32_t reset,
                       uint32_t postreset)
{
    core_disable(c, prereset, reset);
    for (int i = 0; i < 50 &&
         (rd32(c->wrap + BCMA_RESET_CTL) & BCMA_RESET_CTL_RESET); ++i) {
        bp_write32(c->wrap + BCMA_RESET_CTL, 0);
        udelay(50);
    }
    bp_write32(c->wrap + BCMA_IOCTL, postreset | BCMA_IOCTL_CLK);
    rd32(c->wrap + BCMA_IOCTL);
}

/***************************************************************************
 * NVRAM: "key=value" lines to the chip's format (brcmf_fw_nvram_strip)
 ***************************************************************************/

static size_t nvram_build(const char *txt, size_t txtlen, uint8_t *out,
                          size_t outsize)
{
    size_t o = 0;
    size_t i = 0;
    while (i < txtlen) {
        size_t s = i;
        while (i < txtlen && txt[i] != '\n' && txt[i] != '\r')
            i++;
        size_t e = i;
        while (i < txtlen && (txt[i] == '\n' || txt[i] == '\r'))
            i++;
        while (s < e && (txt[s] == ' ' || txt[s] == '\t'))
            s++;
        while (e > s && (txt[e - 1] == ' ' || txt[e - 1] == '\t'))
            e--;
        if (s == e || txt[s] == '#')
            continue;
        if (!memchr(txt + s, '=', e - s))
            continue;
        if (o + (e - s) + 1 + 8 > outsize)
            return 0;
        memcpy(out + o, txt + s, e - s);
        o += e - s;
        out[o++] = '\0';
    }
    size_t padded = (o + 1 + 3) & ~(size_t)3;
    if (padded + 4 > outsize)
        return 0;
    while (o < padded)
        out[o++] = '\0';
    uint32_t token = padded / 4;
    token = (~token << 16) | (token & 0xffff);
    put32(out + o, token);
    return o + 4;
}

/***************************************************************************
 * SDPCM
 ***************************************************************************/

static void rx_fail(void)
{
    rx_pending = false;
    wr8(1, SBSDIO_FUNC1_FRAMECTRL, SFC_RF_TERM);
    bp_write32(sdio_core->base + SD_TOSBMAILBOX, SMB_NAK);
}

/* Security of a network from its capability field and its RSN/WPA IEs */
static int bss_security(uint16_t cap, const uint8_t *ie, size_t ielen)
{
    if (!(cap & 0x10))
        return WIFI_SEC_OPEN;
    bool wpa = false, rsn = false, psk = false, sae = false, eap = false;
    size_t i = 0;
    while (i + 2 <= ielen) {
        uint8_t id = ie[i], l = ie[i + 1];
        const uint8_t *d = ie + i + 2;
        if (i + 2 + l > ielen)
            break;
        if (id == 48 && l >= 8) {
            /* RSN: version, group cipher, pairwise suites, AKM suites */
            rsn = true;
            size_t p = 2 + 4;
            if (p + 2 <= l) {
                unsigned np = get16(d + p);
                p += 2 + 4 * np;
                if (p + 2 <= l) {
                    unsigned na = get16(d + p);
                    p += 2;
                    for (unsigned k = 0; k < na && p + 4 <= l; ++k, p += 4) {
                        if (d[p] != 0x00 || d[p + 1] != 0x0f || d[p + 2] != 0xac)
                            continue;
                        switch (d[p + 3]) {
                        case 2: case 6: psk = true; break;
                        case 8: sae = true; break;
                        case 1: case 5: eap = true; break;
                        }
                    }
                }
            }
        } else if (id == 221 && l >= 4 && d[0] == 0x00 && d[1] == 0x50 &&
                   d[2] == 0xf2 && d[3] == 1) {
            wpa = true;
        }
        i += 2 + l;
    }
    if (rsn) {
        if (psk)
            return WIFI_SEC_WPA2;
        if (sae)
            return WIFI_SEC_WPA3;
        if (eap)
            return WIFI_SEC_ENTERPRISE;
        return WIFI_SEC_WPA2;
    }
    if (wpa)
        return WIFI_SEC_WPA;
    return WIFI_SEC_WEP;
}

static void bss_add(const uint8_t *bi, size_t avail)
{
    if (avail < 128 || !scan_list)
        return;
    uint32_t len = get32(bi + 4);
    if (len < 128 || len > avail)
        return;

    struct wifi_bss b;
    memset(&b, 0, sizeof(b));
    memcpy(b.bssid, bi + 8, 6);
    uint16_t cap = get16(bi + 16);
    unsigned sl = MIN(bi[18], WIFI_SSID_LEN);
    memcpy(b.ssid, bi + 19, sl);
    b.ssid[sl] = '\0';
    b.channel = get16(bi + 72) & 0xff;
    b.rssi = (int16_t)get16(bi + 78);
    uint16_t ie_off = get16(bi + 116);
    uint32_t ie_len = get32(bi + 120);
    if (ie_off >= 128 && ie_off + ie_len <= len)
        b.security = bss_security(cap, bi + ie_off, ie_len);
    else
        b.security = (cap & 0x10) ? WIFI_SEC_WPA2 : WIFI_SEC_OPEN;

    /* hidden networks: an empty or all-zero name */
    bool empty = true;
    for (unsigned k = 0; k < sl; ++k)
        if (b.ssid[k])
            empty = false;
    if (empty)
        return;

    /* one entry per name, the strongest */
    for (int i = 0; i < scan_count; ++i) {
        if (!strcmp(scan_list[i].ssid, b.ssid)) {
            if (b.rssi > scan_list[i].rssi)
                scan_list[i] = b;
            return;
        }
    }
    if (scan_count < scan_max)
        scan_list[scan_count++] = b;
}

static void report(int ev)
{
    if (cbs && cbs->event)
        cbs->event(ev);
}

static void check_link(void)
{
    if (!link_reported && join_link && (join_keyed || !join_need_keys)) {
        link_reported = true;
        join_active = false;
        wlog_add("link up");
        report(WIFI_EV_LINK_UP);
    }
}

static void link_lost(const char *why)
{
    join_link = false;
    join_keyed = false;
    if (link_reported) {
        link_reported = false;
        wlog_add("link down (%s)", why);
        report(WIFI_EV_LINK_DOWN);
    }
}

static void handle_event(const uint8_t *eth, size_t n)
{
    if (n < 14 + 10 + 48)
        return;
    const uint8_t *ev = eth + 14 + 10;
    uint16_t flags = get16be(ev + 2);
    uint32_t type = get32be(ev + 4);
    uint32_t status = get32be(ev + 8);
    uint32_t reason = get32be(ev + 12);
    uint32_t datalen = get32be(ev + 20);
    const uint8_t *data = ev + 48;
    size_t avail = n - (14 + 10 + 48);
    if (datalen > avail)
        datalen = avail;

    switch (type) {
    case E_ESCAN_RESULT:
        if (!scan_running)
            break;
        if (status == E_STATUS_PARTIAL) {
            if (datalen >= 12 + 128 && get16(data + 10) >= 1)
                bss_add(data + 12, datalen - 12);
        } else {
            scan_done = true;
        }
        break;

    case E_SET_SSID:
        if (!join_active)
            break;
        if (status == E_STATUS_SUCCESS) {
            wlog_add("associated");
        } else {
            wlog_add("join failed: status %lu", status);
            join_active = false;
            report(status == E_STATUS_NO_NETWORKS ? WIFI_EV_NO_NETWORK
                                                  : WIFI_EV_JOIN_FAILED);
        }
        break;

    case E_LINK:
        if (flags & EVENT_FLAG_LINK) {
            join_link = true;
            check_link();
        } else {
            link_lost("link event");
        }
        break;

    case E_PSK_SUP:
        if (status == SUP_KEYED) {
            join_keyed = true;
            check_link();
        } else if (join_active) {
            wlog_add("key exchange failed: status %lu reason %lu",
                     status, reason);
            join_active = false;
            report(WIFI_EV_AUTH_FAILED);
        }
        break;

    case E_DEAUTH:
    case E_DEAUTH_IND:
    case E_DISASSOC:
    case E_DISASSOC_IND:
        if (join_active && !link_reported) {
            wlog_add("join failed: event %lu reason %lu", type, reason);
            join_active = false;
            report(WIFI_EV_JOIN_FAILED);
        }
        link_lost(type == E_DEAUTH_IND ? "deauthenticated" :
                  type == E_DISASSOC_IND ? "disassociated" : "left");
        break;
    }
}

static void handle_frame(uint8_t *f, size_t len)
{
    /* Flow control window: the highest sequence number we may send */
    uint8_t window = f[9];
    if ((uint8_t)(window - tx_seq) > 0x40)
        window = tx_seq + 2;
    tx_max = window;

    uint8_t chan = f[5] & 0x0f;
    uint8_t doff = f[7];
    if (doff < SDPCM_HDR_LEN || doff > len)
        return;
    uint8_t *p = f + doff;
    size_t n = len - doff;

    switch (chan) {
    case SDPCM_CHAN_CONTROL:
        if (n >= BCDC_HDR_LEN && ctl_waiting) {
            uint32_t fl = get32(p + 8);
            if ((fl >> 16) == ctl_id) {
                size_t dl = MIN(n - BCDC_HDR_LEN, (size_t)get32(p + 4));
                ctl_status = 0;
                if (fl & BCDC_DCMD_ERROR) {
                    ctl_status = (int32_t)get32(p + 12);
                    if (!ctl_status)
                        ctl_status = -1;
                }
                if (ctl_out) {
                    ctl_outlen = MIN(dl, ctl_outmax);
                    memcpy(ctl_out, p + BCDC_HDR_LEN, ctl_outlen);
                }
                ctl_done = true;
            }
        }
        break;
    case SDPCM_CHAN_EVENT:
    case SDPCM_CHAN_DATA: {
        if (n < BDC_HDR_LEN)
            break;
        size_t off = BDC_HDR_LEN + p[3] * 4;
        if (n < off + 14)
            break;
        uint8_t *eth = p + off;
        size_t elen = n - off;
        ent_stir(eth + 12, MIN(elen - 12, (size_t)16));
        if (get16be(eth + 12) == 0x886c)
            handle_event(eth, elen);
        else if (chan == SDPCM_CHAN_DATA && cbs && cbs->rx)
            cbs->rx(eth, elen);
        break;
    }
    default:
        break;
    }
}

static int f2_read(uint8_t *buf, size_t len)
{
    if (set_window(SI_ENUM_BASE))
        return -1;
    return wifi_sdio_rw(false, 2, F2_ADDR, false, buf, len, F2_BLOCK_SIZE);
}

/* Reads queued frames until the chip has none. Returns the number read. */
static int read_frames(void)
{
    int count = 0;
    for (int i = 0; i < 32; ++i) {
        if (f2_read(rxbuf, FIRSTREAD)) {
            rx_fail();
            break;
        }
        uint16_t len = get16(rxbuf), chk = get16(rxbuf + 2);
        if (!(len | chk)) {
            rx_pending = false;
            break;
        }
        if ((uint16_t)(len ^ chk) != 0xffff || len < SDPCM_HDR_LEN ||
            len > sizeof(rxbuf) - 64) {
            rx_fail();
            break;
        }
        if (len > FIRSTREAD) {
            size_t rest = (len - FIRSTREAD + 3) & ~3u;
            if (f2_read(rxbuf + FIRSTREAD, rest)) {
                rx_fail();
                break;
            }
        }
        handle_frame(rxbuf, len);
        count++;
    }
    return count;
}

static int poll_chip(void)
{
    uint32_t st = 0;
    if (bp_read32(sdio_core->base + SD_INTSTATUS, &st))
        return 0;
    st &= HOSTINTMASK;
    if (st)
        bp_write32(sdio_core->base + SD_INTSTATUS, st);

    if (st & I_HMB_HOST_INT) {
        uint32_t hmb = rd32(sdio_core->base + SD_TOHOSTMAILBOXDATA);
        bp_write32(sdio_core->base + SD_TOSBMAILBOX, SMB_INT_ACK);
        if ((hmb & HMB_DATA_FWHALT) && !fw_halted) {
            fw_halted = true;
            wlog_add("firmware halted (mailbox 0x%08lx)", hmb);
            link_lost("firmware halted");
        }
    }
    if (st & I_HMB_FRAME_IND)
        rx_pending = true;
    return rx_pending ? read_frames() : 0;
}

static bool may_send(void)
{
    uint8_t d = tx_max - tx_seq;
    return d != 0 && !(d & 0x80);
}

static int f2_send(uint8_t chan, const uint8_t *hdr, size_t hdrlen,
                   const uint8_t *payload, size_t len)
{
    size_t total = SDPCM_HDR_LEN + hdrlen + len;
    if (total > sizeof(txbuf))
        return -1;

    long end = current_tick + HZ / 2;
    while (!may_send()) {
        if (TIME_AFTER(current_tick, end)) {
            wlog_add("no send credit (seq %d, max %d)", tx_seq, tx_max);
            return -1;
        }
        if (!poll_chip())
            sleep(1);
    }

    memset(txbuf, 0, SDPCM_HDR_LEN);
    put16(txbuf, total);
    put16(txbuf + 2, ~total);
    txbuf[4] = tx_seq;
    txbuf[5] = chan;
    txbuf[7] = SDPCM_HDR_LEN;
    if (hdrlen)
        memcpy(txbuf + SDPCM_HDR_LEN, hdr, hdrlen);
    memcpy(txbuf + SDPCM_HDR_LEN + hdrlen, payload, len);

    size_t padded = (total + 3) & ~3u;
    if (padded > F2_BLOCK_SIZE)
        padded = (padded + F2_BLOCK_SIZE - 1) & ~(size_t)(F2_BLOCK_SIZE - 1);
    if (padded > sizeof(txbuf))
        return -1;
    memset(txbuf + total, 0, padded - total);

    if (set_window(SI_ENUM_BASE))
        return -1;
    if (wifi_sdio_rw(true, 2, F2_ADDR, false, txbuf, padded, F2_BLOCK_SIZE))
        return -1;
    tx_seq++;
    return 0;
}

/* BCDC ioctl. For a get, buf holds the request and receives the answer. */
static int wl_ioctl(uint32_t cmd, bool set, uint8_t *buf, size_t len)
{
    uint8_t hdr[BCDC_HDR_LEN];

    req_id++;
    put32(hdr, cmd);
    put32(hdr + 4, len);
    put32(hdr + 8, ((uint32_t)req_id << 16) | (set ? BCDC_DCMD_SET : 0));
    put32(hdr + 12, 0);

    ctl_waiting = true;
    ctl_done = false;
    ctl_id = req_id;
    ctl_out = set ? NULL : buf;
    ctl_outmax = len;
    ctl_outlen = 0;

    if (f2_send(SDPCM_CHAN_CONTROL, hdr, BCDC_HDR_LEN, buf, len)) {
        ctl_waiting = false;
        return -1;
    }

    long end = current_tick + 2 * HZ;
    while (!ctl_done && !TIME_AFTER(current_tick, end)) {
        if (!poll_chip() && !ctl_done)
            sleep(1);
    }
    ctl_waiting = false;
    if (!ctl_done) {
        wlog_add("ioctl %lu: no answer", cmd);
        return -2;
    }
    return ctl_status ? -3 : 0;
}

static int set_var(const char *name, const void *data, size_t len)
{
    size_t nl = strlen(name) + 1;
    if (nl + len > sizeof(ioc_buf))
        return -1;
    memcpy(ioc_buf, name, nl);
    memcpy(ioc_buf + nl, data, len);
    return wl_ioctl(WLC_SET_VAR, true, ioc_buf, nl + len);
}

/* The request is the name followed by room for the answer; the answer
 * comes back at the start of the buffer */
static int get_var(const char *name, uint8_t *out, size_t outsize)
{
    size_t nl = strlen(name) + 1;
    if (nl + outsize > sizeof(ioc_buf))
        return -1;
    memset(ioc_buf, 0, nl + outsize);
    memcpy(ioc_buf, name, nl);
    int rc = wl_ioctl(WLC_GET_VAR, false, ioc_buf, nl + outsize);
    memcpy(out, ioc_buf, outsize);
    return rc;
}

static int set_var_u32(const char *name, uint32_t v)
{
    uint8_t b[4];
    put32(b, v);
    return set_var(name, b, 4);
}

static int set_bsscfg_u32(const char *name, uint32_t v)
{
    uint8_t b[8];
    put32(b, 0);    /* bsscfg index */
    put32(b + 4, v);
    return set_var(name, b, 8);
}

static int set_int(uint32_t cmd, uint32_t val)
{
    uint8_t b[4];
    put32(b, val);
    return wl_ioctl(cmd, true, b, 4);
}

/***************************************************************************
 * Bring-up
 ***************************************************************************/

static int wait_clk(uint8_t mask, int ms)
{
    uint8_t v = 0;
    for (int i = 0; i < ms; ++i) {
        if (rd8(1, SBSDIO_FUNC1_CHIPCLKCSR, &v) == 0 && (v & mask))
            return 0;
        if (i < 20)
            udelay(1000);
        else {
            sleep(1);
            i += 1000 / HZ - 1;
        }
    }
    return -1;
}

static int wait_ioready(uint8_t mask, int ticks)
{
    uint8_t v = 0;
    for (int i = 0; i < ticks; ++i) {
        if (rd8(0, 0x03, &v) == 0 && (v & mask) == mask)
            return 0;
        sleep(1);
    }
    return -1;
}

static uint8_t chunk[2048] CACHEALIGN_ATTR;

/* Firmware from the card straight into the chip's RAM */
static int download_firmware(char *err, size_t errlen)
{
    int fd = open(FW_FILE, O_RDONLY);
    if (fd < 0) {
        snprintf(err, errlen, "Missing %s", FW_FILE);
        return -1;
    }
    uint32_t addr = 0;
    uint8_t first[64];
    bool have_first = false;
    while (1) {
        ssize_t n = read(fd, chunk, sizeof(chunk));
        if (n < 0) {
            close(fd);
            snprintf(err, errlen, "Could not read the Wi-Fi firmware");
            return -1;
        }
        if (n == 0)
            break;
        if (addr + n > CHIP_RAM_SIZE - 4096) {
            close(fd);
            snprintf(err, errlen, "Wi-Fi firmware too large");
            return -1;
        }
        size_t padded = (n + 3) & ~3u;
        memset(chunk + n, 0, padded - n);
        if (!have_first) {
            memcpy(first, chunk, sizeof(first));
            have_first = true;
        }
        if (bp_mem(true, addr, chunk, padded)) {
            close(fd);
            snprintf(err, errlen, "Firmware download failed");
            return -1;
        }
        addr += n;
    }
    close(fd);

    uint8_t chk[64];
    if (!have_first || bp_mem(false, 0, chk, sizeof(chk)) ||
        memcmp(chk, first, sizeof(chk))) {
        snprintf(err, errlen, "Firmware check failed");
        return -1;
    }
    wlog_add("firmware: %lu bytes", addr);
    return 0;
}

static int download_nvram(char *err, size_t errlen)
{
    static char txt[4096];
    static uint8_t nv[2048] CACHEALIGN_ATTR;
    int fd = open(NVRAM_FILE, O_RDONLY);
    if (fd < 0) {
        snprintf(err, errlen, "Missing %s", NVRAM_FILE);
        return -1;
    }
    ssize_t n = read(fd, txt, sizeof(txt));
    close(fd);
    if (n <= 0) {
        snprintf(err, errlen, "Could not read the Wi-Fi NVRAM");
        return -1;
    }
    size_t nvlen = nvram_build(txt, n, nv, sizeof(nv));
    if (!nvlen) {
        snprintf(err, errlen, "Bad Wi-Fi NVRAM file");
        return -1;
    }
    uint32_t nvaddr = CHIP_RAM_SIZE - nvlen;
    if (bp_mem(true, nvaddr, nv, nvlen)) {
        snprintf(err, errlen, "NVRAM download failed");
        return -1;
    }
    return 0;
}

/* CLM blob, in chunks through "clmload" (brcmf_c_process_clm_blob) */
#define DL_BEGIN        0x0002
#define DL_END          0x0004
#define DL_TYPE_CLM     2
#define DLOAD_FLAG_VER  (1 << 12)
#define CLM_CHUNK       1400

static int load_clm(void)
{
    static uint8_t clm[CLM_CHUNK + 12];
    int fd = open(CLM_FILE, O_RDONLY);
    if (fd < 0) {
        wlog_add("no CLM blob");
        return 0;   /* the firmware's built-in data may do */
    }
    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    off_t off = 0;
    uint16_t flag = DL_BEGIN;
    int rc = 0;
    while (off < size) {
        size_t n = MIN((size_t)(size - off), (size_t)CLM_CHUNK);
        if (read(fd, clm + 12, n) != (ssize_t)n) {
            rc = -1;
            break;
        }
        if (off + (off_t)n >= size)
            flag |= DL_END;
        put16(clm, DLOAD_FLAG_VER | flag);
        put16(clm + 2, DL_TYPE_CLM);
        put32(clm + 4, n);
        put32(clm + 8, 0);
        rc = set_var("clmload", clm, 12 + n);
        if (rc)
            break;
        off += n;
        flag = 0;
    }
    close(fd);
    uint8_t st[4] = { 0, 0, 0, 0 };
    get_var("clmload_status", st, 4);
    wlog_add("CLM: %s, status %lu", rc ? "failed" : "loaded", get32(st));
    return rc;
}

static int bring_up(char *err, size_t errlen)
{
    uint8_t v;
    uint32_t id;

    sb_window_valid = false;
    tx_seq = 0;
    tx_max = 1;
    req_id = 0;
    rx_pending = false;
    fw_halted = false;
    join_active = join_link = join_keyed = link_reported = false;

    int rc = wifi_target_power_on();
    if (rc) {
        snprintf(err, errlen, "Wi-Fi chip not found (%d)", rc);
        return -1;
    }

    if (wr8(0, 0x110, F1_BLOCK_SIZE & 0xff) || wr8(0, 0x111, F1_BLOCK_SIZE >> 8) ||
        wr8(0, 0x210, F2_BLOCK_SIZE & 0xff) || wr8(0, 0x211, F2_BLOCK_SIZE >> 8) ||
        wr8(0, 0x02, 0x02) || wait_ioready(0x02, HZ / 2)) {
        snprintf(err, errlen, "Wi-Fi bus setup failed");
        return -1;
    }

    /* Backplane clock: ALP, with the PLL forced off for now */
    wr8(1, SBSDIO_FUNC1_CHIPCLKCSR, SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ);
    if (wait_clk(SBSDIO_ALP_AVAIL, 100)) {
        snprintf(err, errlen, "Wi-Fi chip clock failed");
        return -1;
    }
    wr8(1, SBSDIO_FUNC1_CHIPCLKCSR, SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_FORCE_ALP);
    udelay(65);

    if (bp_read32(SI_ENUM_BASE, &id)) {
        snprintf(err, errlen, "Wi-Fi backplane failed");
        return -1;
    }
    wlog_add("chip %lu rev %lu", id & 0xffff, (id >> 16) & 0xf);

    erom_scan();
    struct core *arm = find_core(CORE_ARM_CM3);
    struct core *ram = find_core(CORE_INTERNAL_MEM);
    struct core *d11 = find_core(CORE_80211);
    sdio_core = find_core(CORE_SDIO_DEV);
    if (!arm || !ram || !d11 || !sdio_core) {
        snprintf(err, errlen, "Unknown Wi-Fi chip (%lu)", id & 0xffff);
        return -1;
    }

    /* Passive: ARM held, radio and RAM cores reset, RAM bank 3 not
     * remapped (43430) */
    core_disable(arm, 0, 0);
    core_reset(d11, D11_IOCTL_PHYRESET | D11_IOCTL_PHYCLOCKEN,
               D11_IOCTL_PHYCLOCKEN, D11_IOCTL_PHYCLOCKEN);
    core_reset(ram, 0, 0, 0);
    bp_write32(ram->base + SOCRAM_BANKIDX, 3);
    bp_write32(ram->base + SOCRAM_BANKPDA, 0);

    if (download_firmware(err, errlen) || download_nvram(err, errlen))
        return -1;

    /* Active: clear the SDIO interrupts, then let the ARM run */
    if (!core_is_up(ram)) {
        snprintf(err, errlen, "Wi-Fi RAM core is not up");
        return -1;
    }
    bp_write32(sdio_core->base + SD_INTSTATUS, 0xffffffff);
    core_reset(arm, 0, 0, 0);

    /* HT clock (the firmware brings up the PLL) */
    wr8(1, SBSDIO_FUNC1_CHIPCLKCSR, SBSDIO_HT_AVAIL_REQ);
    if (wait_clk(SBSDIO_HT_AVAIL, 1000)) {
        rd8(1, SBSDIO_FUNC1_CHIPCLKCSR, &v);
        snprintf(err, errlen, "Wi-Fi firmware did not start (clock 0x%02x)", v);
        return -1;
    }
    uint8_t saveclk = 0;
    rd8(1, SBSDIO_FUNC1_CHIPCLKCSR, &saveclk);
    wr8(1, SBSDIO_FUNC1_CHIPCLKCSR, saveclk | SBSDIO_FORCE_HT);

    /* Function 2 (frames): comes up once the firmware has booted */
    bp_write32(sdio_core->base + SD_TOSBMAILBOXDATA, SDPCM_PROT_VERSION << 16);
    if (wr8(0, 0x02, 0x06) || wait_ioready(0x04, 3 * HZ)) {
        snprintf(err, errlen, "Wi-Fi firmware did not start");
        return -1;
    }
    bp_write32(sdio_core->base + SD_HOSTINTMASK, HOSTINTMASK);
    wr8(1, SBSDIO_WATERMARK, 8);
    wr8(1, SBSDIO_FUNC1_CHIPCLKCSR, saveclk);

    /* Let the firmware settle and send its first frames */
    for (int i = 0; i < 10; ++i) {
        poll_chip();
        sleep(1);
    }

    /* No superframes from the chip: this driver reads single frames */
    set_var_u32("bus:txglom", 0);

    uint8_t b[96];
    if (get_var("ver", b, sizeof(b)) == 0) {
        b[sizeof(b) - 1] = '\0';
        char *nl = strchr((char *)b, '\n');
        if (nl)
            *nl = '\0';
        /* "wl0: Mar 30 2021 01:12:21 version 7.45.98.118 (...)" */
        char *ver = strstr((char *)b, "version ");
        strmemccpy(fw_version, ver ? ver + 8 : (char *)b, sizeof(fw_version));
    } else {
        snprintf(err, errlen, "Wi-Fi firmware does not answer");
        return -1;
    }
    wlog_add("firmware %s", fw_version);

    if (get_var("cur_etheraddr", mac_addr, 6) == 0)
        wlog_add("MAC %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0],
                 mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    ent_stir(mac_addr, 6);

    load_clm();

    /* Events we use */
    uint8_t mask[16];
    memset(mask, 0, sizeof(mask));
    static const uint8_t evs[] = { E_SET_SSID, E_JOIN, E_AUTH, E_DEAUTH,
        E_DEAUTH_IND, E_ASSOC, E_DISASSOC, E_DISASSOC_IND, E_LINK, E_PSK_SUP,
        E_ESCAN_RESULT };
    for (unsigned i = 0; i < sizeof(evs); ++i)
        mask[evs[i] / 8] |= 1 << (evs[i] % 8);
    set_var("event_msgs", mask, sizeof(mask));

    set_int(WLC_SET_INFRA, 1);
    if (set_int(WLC_UP, 1)) {
        snprintf(err, errlen, "Wi-Fi radio did not come up");
        return -1;
    }
    return 0;
}

/***************************************************************************
 * Driver thread and API
 ***************************************************************************/

static long wifi_stack[(DEFAULT_STACK_SIZE + 4096) / sizeof(long)];

static void wifi_thread(void)
{
    while (1) {
        if (!running) {
            sleep(HZ / 5);
            continue;
        }
        mutex_lock(&wl_mtx);
        int n = running ? poll_chip() : 0;
        mutex_unlock(&wl_mtx);
        /* Busy: come back as soon as others had a turn */
        if (n)
            yield();
        else
            sleep(1);
    }
}

void wifi_hw_init(const struct wifi_callbacks *cb)
{
    cbs = cb;
    mutex_init(&wl_mtx);
    create_thread(wifi_thread, wifi_stack, sizeof(wifi_stack), 0, "wifi"
                  IF_PRIO(, PRIORITY_SYSTEM) IF_COP(, CPU));
}

int wifi_hw_start(char *err, size_t errlen)
{
    mutex_lock(&wl_mtx);
    if (running) {
        mutex_unlock(&wl_mtx);
        return 0;
    }
    wlog_add("starting");
    cpu_boost(true);
    int rc = bring_up(err, errlen);
    cpu_boost(false);
    if (rc) {
        wlog_add("start failed: %s", err);
        wifi_target_power_off();
    } else {
        running = true;
        wlog_add("ready");
    }
    mutex_unlock(&wl_mtx);
    return rc;
}

void wifi_hw_stop(void)
{
    mutex_lock(&wl_mtx);
    if (running) {
        if (link_reported || join_active)
            set_int(WLC_DISASSOC, 0);
        set_int(WLC_DOWN, 1);
        running = false;
        link_lost("off");
        join_active = false;
        wifi_target_power_off();
        wlog_add("off");
    }
    mutex_unlock(&wl_mtx);
}

bool wifi_hw_running(void)
{
    return running;
}

static int cmp_rssi(const struct wifi_bss *a, const struct wifi_bss *b)
{
    return b->rssi - a->rssi;
}

int wifi_hw_scan(struct wifi_bss *out, int max)
{
    mutex_lock(&wl_mtx);
    if (!running) {
        mutex_unlock(&wl_mtx);
        return -1;
    }

    /* Active escan on all channels */
    uint8_t prm[76];
    memset(prm, 0, sizeof(prm));
    put32(prm + 0, 1);          /* version */
    put16(prm + 4, 1);          /* action: start */
    put16(prm + 6, 0x1235);     /* sync id */
    memset(prm + 8 + 36, 0xff, 6);      /* any BSSID */
    prm[8 + 42] = 2;            /* bss type: any */
    prm[8 + 43] = 0;            /* active */
    put32(prm + 8 + 44, 0xffffffff);    /* defaults for the times */
    put32(prm + 8 + 48, 0xffffffff);
    put32(prm + 8 + 52, 0xffffffff);
    put32(prm + 8 + 56, 0xffffffff);
    put32(prm + 8 + 60, 0);             /* all channels */

    scan_list = out;
    scan_max = max;
    scan_count = 0;
    scan_done = false;
    scan_running = true;
    int rc = set_var("escan", prm, sizeof(prm));
    mutex_unlock(&wl_mtx);
    if (rc) {
        scan_running = false;
        scan_list = NULL;
        wlog_add("scan failed (%d)", rc);
        return -1;
    }

    /* The driver thread collects the results */
    long end = current_tick + 8 * HZ;
    while (!scan_done && running && !TIME_AFTER(current_tick, end))
        sleep(HZ / 20);

    mutex_lock(&wl_mtx);
    scan_running = false;
    scan_list = NULL;
    int n = scan_count;
    mutex_unlock(&wl_mtx);

    /* strongest first */
    for (int i = 1; i < n; ++i) {
        struct wifi_bss t = out[i];
        int j = i - 1;
        while (j >= 0 && cmp_rssi(&out[j], &t) > 0) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = t;
    }
    wlog_add("scan: %d networks", n);
    return n;
}

int wifi_hw_join(const char *ssid, const char *psk, int security)
{
    size_t sl = strlen(ssid);
    size_t pl = psk ? strlen(psk) : 0;
    if (sl == 0 || sl > WIFI_SSID_LEN)
        return -1;
    if (security != WIFI_SEC_OPEN && (pl < 8 || pl > 63))
        return -2;
    if (security == WIFI_SEC_WEP || security == WIFI_SEC_WPA3 ||
        security == WIFI_SEC_ENTERPRISE)
        return -3;

    mutex_lock(&wl_mtx);
    if (!running) {
        mutex_unlock(&wl_mtx);
        return -1;
    }
    if (link_reported || join_active) {
        set_int(WLC_DISASSOC, 0);
        link_lost("rejoin");
    }

    join_active = true;
    join_link = join_keyed = link_reported = false;
    join_need_keys = security != WIFI_SEC_OPEN;

    int rc = 0;
    uint32_t wsec = security == WIFI_SEC_OPEN ? 0 :
                    security == WIFI_SEC_WPA ? (WSEC_TKIP | WSEC_AES) : WSEC_AES;
    uint32_t wpa_auth = security == WIFI_SEC_OPEN ? WPA_AUTH_DISABLED :
                        security == WIFI_SEC_WPA ? WPA_AUTH_PSK : WPA2_AUTH_PSK;
    rc |= set_int(WLC_SET_WSEC, wsec);
    if (security != WIFI_SEC_OPEN) {
        /* the firmware's own supplicant does the key exchange */
        rc |= set_bsscfg_u32("bsscfg:sup_wpa", 1);
        set_bsscfg_u32("bsscfg:sup_wpa2_eapver", 0xffffffff);
        set_bsscfg_u32("bsscfg:sup_wpa_tmo", 2500);
        uint8_t pmk[68];
        memset(pmk, 0, sizeof(pmk));
        put16(pmk, pl);
        put16(pmk + 2, WSEC_PASSPHRASE);
        memcpy(pmk + 4, psk, pl);
        /* the firmware hashes the passphrase: give it time */
        sleep(HZ / 50);
        rc |= wl_ioctl(WLC_SET_WSEC_PMK, true, pmk, sizeof(pmk));
    }
    rc |= set_int(WLC_SET_INFRA, 1);
    rc |= set_int(WLC_SET_AUTH, 0);
    rc |= set_int(WLC_SET_WPA_AUTH, wpa_auth);

    uint8_t s[36];
    memset(s, 0, sizeof(s));
    put32(s, sl);
    memcpy(s + 4, ssid, sl);
    rc |= wl_ioctl(WLC_SET_SSID, true, s, sizeof(s));
    if (rc) {
        join_active = false;
        wlog_add("join setup failed");
    } else {
        wlog_add("joining \"%s\" (%s)", ssid,
                 security == WIFI_SEC_OPEN ? "open" :
                 security == WIFI_SEC_WPA ? "WPA" : "WPA2");
    }
    mutex_unlock(&wl_mtx);
    return rc ? -4 : 0;
}

void wifi_hw_leave(void)
{
    mutex_lock(&wl_mtx);
    if (running) {
        join_active = false;
        set_int(WLC_DISASSOC, 0);
        link_lost("left");
    }
    mutex_unlock(&wl_mtx);
}

/* Fast power save once joined (called by the network layer) */
void wifi_hw_power_save(bool on)
{
    mutex_lock(&wl_mtx);
    if (running)
        set_int(WLC_SET_PM, on ? PM_FAST : 0);
    mutex_unlock(&wl_mtx);
}

int wifi_hw_send(const void *frame, size_t len)
{
    uint8_t bdc[BDC_HDR_LEN] = { BDC_PROTO_VER << 4, 0, 0, 0 };
    int rc = -1;
    mutex_lock(&wl_mtx);
    if (running && link_reported)
        rc = f2_send(SDPCM_CHAN_DATA, bdc, BDC_HDR_LEN, frame, len);
    mutex_unlock(&wl_mtx);
    return rc;
}

void wifi_hw_mac(uint8_t mac[6])
{
    memcpy(mac, mac_addr, 6);
}

int wifi_hw_rssi(void)
{
    int rssi = 0;
    mutex_lock(&wl_mtx);
    if (running && link_reported) {
        uint8_t b[4] = { 0, 0, 0, 0 };
        if (wl_ioctl(WLC_GET_RSSI, false, b, 4) == 0)
            rssi = (int32_t)get32(b);
    }
    mutex_unlock(&wl_mtx);
    return rssi;
}

const char *wifi_hw_fw_version(void)
{
    return fw_version;
}
