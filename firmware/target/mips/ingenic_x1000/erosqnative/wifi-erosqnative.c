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

/* Wi-Fi SDIO bus of the H2 (Eros Q native).
 *
 * The radio module (AzureWave AW-NM372SM, Cypress CYW43438) has Wi-Fi on
 * SDIO, wired like Ingenic's Halley2 reference board: MSC1 on PC0-PC5
 * (function 0), WL_REG_ON on PC17, WL_HOST_WAKE on PC16. Its 32.768 kHz
 * sleep clock is shared with Bluetooth. The chip driver is in
 * firmware/drivers/wifi/. */

#include "system.h"
#include "kernel.h"
#include "gpio-ingenic.h"
#include "irq-x1000.h"
#include "clk-x1000.h"
#include "msc-x1000.h"
#include "sdmmc_host.h"
#include "bt_hci.h"
#include "wifi.h"
#include "x1000/cpm.h"
#include "x1000/msc.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define WIFI_SDIO_PORT      GPIO_C
#define WIFI_SDIO_PINS      (0x3f << 0)     /* PC0-PC5 */
#define GPIO_WL_HOST_WAKE   GPIO_PC(16)
#define GPIO_WL_REG_ON      GPIO_PC(17)
#define GPIO_CLK32K         GPIO_PB(26)

static struct x1000_msc_controller msc1_ctl;
static struct x1000_msc_dma_desc msc1_dma_desc CACHEALIGN_ATTR;
static bool msc1_active = false;
static bool wifi_clk32k_ours = false;
static bool wifi_powered = false;

#define DMA_BUF_SIZE 4096
static uint8_t dma_buf[DMA_BUF_SIZE] CACHEALIGN_ATTR;

void MSC1(void)
{
    if (msc1_active)
        x1000_msc_irq_handler(&msc1_ctl);
    else
        REG_MSC_IMASK(1) = 0xFFFFFFFFu;
}

uint32_t wifi_target_usec(void)
{
    return __ost_read32() / OST_TICKS_PER_US;
}

/* Gives up a command whose interrupt never comes, so a dead bus can't hang
 * the player */
static struct timeout cmd_tmo;
static int cmd_tmo_cb(struct timeout *tmo)
{
    (void)tmo;
    x1000_msc_abort_command(&msc1_ctl);
    return 0;
}

static int sdio_cmd_ex(uint16_t cmd, uint32_t arg, uint16_t flags,
                       uint16_t nr_blocks, uint16_t block_len, uint32_t *resp)
{
    struct sdmmc_host_command c = {
        .flags = flags, .command = cmd, .argument = arg,
        .nr_blocks = nr_blocks, .block_len = block_len,
        .buffer = SDMMC_DATA_PRESENT(flags) ? dma_buf : NULL,
    };
    struct sdmmc_host_response r;
    memset(&r, 0, sizeof(r));

    timeout_register(&cmd_tmo, cmd_tmo_cb, HZ / 2, 0);
    int rc = x1000_msc_submit_command(&msc1_ctl, &c, &r);
    timeout_cancel(&cmd_tmo);

    if (resp)
        *resp = r.data[0];
    return rc;
}

static int sdio_cmd(uint16_t cmd, uint32_t arg, uint16_t flags, uint32_t *resp)
{
    return sdio_cmd_ex(cmd, arg, flags, 0, 0, resp);
}

static const char *rc_str(int rc)
{
    switch (rc) {
    case SDMMC_STATUS_OK:          return "ok";
    case SDMMC_STATUS_TIMEOUT:     return "no answer (timeout)";
    case SDMMC_STATUS_INVALID_CRC: return "bad CRC";
    default:                       return "error";
    }
}

static void power_up(void)
{
    /* The chip needs its 32.768 kHz sleep clock; Bluetooth may have it on */
    wifi_clk32k_ours = !bt_hci_sleep_clock();
    if (wifi_clk32k_ours) {
        jz_writef(CPM_OPCR, ERCS(1));
        gpio_set_function(GPIO_CLK32K, GPIOF_DEVICE(0));
    }

    /* MSC1 shares MSC0's clock source (SCLK_A, set up for the SD card) */
    x1000_msc_init(&msc1_ctl, &msc1_dma_desc, 1, clk_get(X1000_CLK_SCLK_A));
    msc1_active = true;
    system_enable_irq(IRQ_MSC1);

    gpio_set_function(GPIO_WL_HOST_WAKE, GPIOF_INPUT);
    gpio_set_function(GPIO_WL_REG_ON, GPIOF_OUTPUT(0));
    sleep(HZ / 20);
}

static void power_down(void)
{
    gpio_set_level(GPIO_WL_REG_ON, 0);
    sleep(HZ / 50);
    gpio_set_function(GPIO_WL_REG_ON, GPIOF_INPUT);

    system_disable_irq(IRQ_MSC1);
    msc1_active = false;

    /* Bluetooth may have taken the clock over meanwhile */
    if (wifi_clk32k_ours && !bt_hci_sleep_clock())
        gpio_set_function(GPIO_CLK32K, GPIOF_INPUT);
    wifi_clk32k_ours = false;
}

static void bus_up(void)
{
    gpioz_configure(WIFI_SDIO_PORT, WIFI_SDIO_PINS, GPIOF_DEVICE(0));
    x1000_msc_set_bus_clock(&msc1_ctl, SDMMC_BUS_CLOCK_400KHZ);
    x1000_msc_set_power_enabled(&msc1_ctl, true);
    sleep(HZ / 50);
}

static void bus_down(void)
{
    x1000_msc_set_power_enabled(&msc1_ctl, false);
    gpioz_configure(WIFI_SDIO_PORT, WIFI_SDIO_PINS, GPIOF_INPUT);
}

/* CMD0 then CMD5 with argument 0 (inquiry) */
static int sdio_inquiry(uint32_t *r4)
{
    sdio_cmd(0, 0, SDMMC_RESP_NONE, NULL);
    sleep(HZ / 100);
    return sdio_cmd(5, 0, SDMMC_RESP_SHORT | SDMMC_RESP_NOCRC, r4);
}

/* R5 flags: COM_CRC_ERROR, ILLEGAL_COMMAND, ERROR, FUNCTION_NUMBER,
 * OUT_OF_RANGE */
#define R5_ERRORS 0xcb00

static int cmd52(bool wr, int fn, uint32_t addr, uint8_t data, uint8_t *out)
{
    uint32_t r5 = 0;
    uint32_t arg = (wr ? 0x80000000u : 0) | ((uint32_t)(fn & 7) << 28) |
                   ((addr & 0x1ffff) << 9) | (wr ? data : 0);
    int rc = sdio_cmd(52, arg, SDMMC_RESP_SHORT, &r5);
    if (rc == SDMMC_STATUS_OK && (r5 & R5_ERRORS))
        rc = SDMMC_STATUS_ERROR;
    if (out)
        *out = r5 & 0xff;
    return rc;
}

int wifi_sdio_rd8(int fn, uint32_t addr, uint8_t *val)
{
    return cmd52(false, fn, addr, 0, val);
}

int wifi_sdio_wr8(int fn, uint32_t addr, uint8_t val)
{
    return cmd52(true, fn, addr, val, NULL);
}

/* One CMD53 of at most DMA_BUF_SIZE bytes: block mode if blocks > 0,
 * else byte mode with count bytes (1..512) */
static int cmd53_once(bool wr, int fn, uint32_t addr, bool incr,
                      uint8_t *buf, unsigned blocks, unsigned blksz,
                      unsigned count)
{
    unsigned len = blocks ? blocks * blksz : count;
    uint32_t arg = (wr ? 0x80000000u : 0) | ((uint32_t)(fn & 7) << 28) |
                   (blocks ? (1u << 27) : 0) | (incr ? (1u << 26) : 0) |
                   ((addr & 0x1ffff) << 9) |
                   ((blocks ? blocks : count) & 0x1ff);
    uint32_t r5 = 0;
    int rc;

    if (wr)
        memcpy(dma_buf, buf, len);
    rc = sdio_cmd_ex(53, arg,
                     SDMMC_RESP_SHORT | (wr ? SDMMC_DATA_WRITE : SDMMC_DATA_READ),
                     blocks ? blocks : 1, blocks ? blksz : count, &r5);
    if (rc == SDMMC_STATUS_OK && (r5 & R5_ERRORS))
        rc = SDMMC_STATUS_ERROR;
    if (!wr && rc == SDMMC_STATUS_OK)
        memcpy(buf, dma_buf, len);
    return rc;
}

int wifi_sdio_rw(bool wr, int fn, uint32_t addr, bool incr,
                 uint8_t *buf, size_t len, unsigned blksz)
{
    while (len) {
        int rc;
        unsigned n;
        if (blksz && len >= blksz) {
            unsigned blocks = MIN(len / blksz, DMA_BUF_SIZE / blksz);
            blocks = MIN(blocks, 511u);
            n = blocks * blksz;
            rc = cmd53_once(wr, fn, addr, incr, buf, blocks, blksz, 0);
        } else {
            n = MIN(len, (size_t)(blksz ? blksz : 512));
            n = MIN(n, 512u);
            rc = cmd53_once(wr, fn, addr, incr, buf, 0, 0, n);
        }
        if (rc)
            return rc;
        buf += n;
        len -= n;
        if (incr)
            addr += n;
    }
    return 0;
}

/* Bring a powered card from idle to the transfer state */
static int sdio_enumerate(void)
{
    uint32_t r = 0;
    int rc = sdio_inquiry(&r);
    if (rc)
        return -1;
    uint32_t ocr = r & 0x00ffffff;

    /* Ask for 3.2-3.4 V (the module's VDDIO is 3.3 V) until ready */
    uint32_t want = ocr & 0x00300000;
    if (!want)
        want = ocr;
    for (int i = 0; i < 100; ++i) {
        rc = sdio_cmd(5, want, SDMMC_RESP_SHORT | SDMMC_RESP_NOCRC, &r);
        if (rc == SDMMC_STATUS_OK && (r & 0x80000000))
            break;
        sleep(HZ / 100);
    }
    if (!(rc == SDMMC_STATUS_OK && (r & 0x80000000)))
        return -2;
    if (sdio_cmd(3, 0, SDMMC_RESP_SHORT, &r))
        return -3;
    uint32_t rca = r >> 16;
    if (sdio_cmd(7, rca << 16, SDMMC_RESP_SHORT | SDMMC_RESP_BUSY, &r))
        return -4;
    return 0;
}

int wifi_target_power_on(void)
{
    if (wifi_powered)
        wifi_target_power_off();
    power_up();
    gpio_set_level(GPIO_WL_REG_ON, 1);
    sleep(HZ / 4);
    bus_up();
    wifi_powered = true;

    int rc = sdio_enumerate();
    if (rc) {
        wifi_target_power_off();
        return rc;
    }
    x1000_msc_set_bus_clock(&msc1_ctl, SDMMC_BUS_CLOCK_25MHZ);
    return 0;
}

void wifi_target_power_off(void)
{
    if (!wifi_powered)
        return;
    bus_down();
    power_down();
    wifi_powered = false;
}

/***************************************************************************
 * Debug: is the Wi-Fi core there?
 ***************************************************************************/

static char *log_buf;
static size_t log_size, log_len;

static void plog(const char *fmt, ...) ATTRIBUTE_PRINTF(1, 2);
static void plog(const char *fmt, ...)
{
    if (log_size < 2 || log_len + 2 >= log_size)
        return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(log_buf + log_len, log_size - log_len - 1, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    log_len += n;
    if (log_len > log_size - 2)
        log_len = log_size - 2;
    log_buf[log_len++] = '\n';
    log_buf[log_len] = '\0';
}

static void identify_cis(void)
{
    uint8_t cccr_rev = 0;
    wifi_sdio_rd8(0, 0x00, &cccr_rev);
    plog("CCCR rev %d, SDIO spec rev %d", cccr_rev & 0xf, (cccr_rev >> 4) & 0xf);

    /* Common CIS pointer, then look for CISTPL_MANFID */
    uint8_t p0 = 0, p1 = 0, p2 = 0;
    if (wifi_sdio_rd8(0, 0x09, &p0) || wifi_sdio_rd8(0, 0x0a, &p1) ||
        wifi_sdio_rd8(0, 0x0b, &p2)) {
        plog("Could not read the CIS pointer");
        return;
    }
    uint32_t cis = p0 | (p1 << 8) | ((uint32_t)p2 << 16);
    plog("CIS at 0x%05lx", cis);

    uint32_t p = cis;
    for (int t = 0; t < 32 && p < cis + 512; ++t) {
        uint8_t code = 0, link = 0;
        if (wifi_sdio_rd8(0, p, &code) || code == 0xff)
            break;
        if (wifi_sdio_rd8(0, p + 1, &link))
            break;
        if (code == 0x20 && link >= 4) {
            uint8_t b[4];
            for (int i = 0; i < 4; ++i)
                wifi_sdio_rd8(0, p + 2 + i, &b[i]);
            unsigned manf = b[0] | (b[1] << 8);
            unsigned card = b[2] | (b[3] << 8);
            plog("Vendor 0x%04x, device 0x%04x", manf, card);
            if (manf == 0x02d0)
                plog("=> Broadcom/Cypress Wi-Fi%s",
                     (card == 0xa9a6 || card == 0x4343) ?
                     " (BCM43430/CYW43438)" : "");
            return;
        }
        if (link == 0xff)
            break;
        p += 2 + link;
    }
    plog("No manufacturer ID found in the CIS");
}

int wifi_debug_probe(char *buf, size_t size)
{
    uint32_t r4 = 0;
    int rc;

    log_buf = buf;
    log_size = size;
    log_len = 0;
    if (size)
        buf[0] = '\0';

    if (wifi_powered) {
        plog("Wi-Fi is in use; turn it off first");
        return (int)log_len;
    }

    plog("SDIO: MSC1 on PC0-PC5");
    plog("WL_REG_ON PC17, WL_HOST_WAKE PC16");

    power_up();
    plog("32k clock: %s", wifi_clk32k_ours ? "turned on" : "already on (Bluetooth)");
    plog("WL_HOST_WAKE at start: %d", gpio_get_level(GPIO_WL_HOST_WAKE));

    /* Control: Wi-Fi core held off */
    bus_up();
    rc = sdio_inquiry(&r4);
    plog("[REG_ON low]  CMD5: %s", rc_str(rc));
    if (rc == SDMMC_STATUS_OK)
        plog("  answered anyway: R4 0x%08lx", r4);
    bus_down();

    /* Wi-Fi core on */
    gpio_set_level(GPIO_WL_REG_ON, 1);
    sleep(HZ / 4);
    plog("WL_HOST_WAKE after REG_ON: %d", gpio_get_level(GPIO_WL_HOST_WAKE));

    bus_up();
    rc = sdio_inquiry(&r4);
    plog("[REG_ON high] CMD5: %s", rc_str(rc));
    if (rc == SDMMC_STATUS_OK) {
        plog("  R4 0x%08lx, functions %lu, OCR 0x%06lx", r4, (r4 >> 28) & 7,
             r4 & 0xffffff);
        if (sdio_enumerate() == 0)
            identify_cis();
        else
            plog("Enumeration failed");
    }
    bus_down();

    power_down();
    plog("Done; Wi-Fi core powered off");
    return (int)log_len;
}
