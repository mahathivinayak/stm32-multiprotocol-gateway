/* W25Qxx SPI NOR flash driver + circular telemetry log ("black box").
   Log region: 64 sectors x 4 KB = 256 KB, 32-byte records (8 per page, never cross a page).
   Uses hspi1 and the FLASH_CS pin label from each board's CubeMX project (main.h). */
#include <string.h>
#include "main.h"
#include "drv_spi_flash.h"

extern SPI_HandleTypeDef hspi1;

#define CMD_WREN   0x06
#define CMD_RDSR   0x05
#define CMD_READ   0x03
#define CMD_PP     0x02
#define CMD_SE     0x20
#define CMD_JEDEC  0x9F

#define SECTOR_SIZE  4096u
#define LOG_SECTORS  64u
#define LOG_BASE     0x000000u
#define LOG_SIZE     (SECTOR_SIZE * LOG_SECTORS)
#define REC_SIZE     32u
#define REC_MAGIC    0xA5

typedef struct __attribute__((packed)) {
    uint8_t  magic;      /* 0xA5 = written, 0xFF = erased */
    uint8_t  src;
    uint8_t  len;
    uint8_t  rsvd;
    uint32_t tick_ms;
    uint32_t id;
    uint8_t  data[20];   /* longer payloads are truncated in the log */
} log_rec_t;
_Static_assert(sizeof(log_rec_t) == REC_SIZE, "log record must be 32 bytes");

#define CS_LOW()   HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET)

static uint32_t head;      /* byte offset of next free record */
static uint32_t logged;
static uint8_t  present;

static void send_cmd_addr(uint8_t cmd, uint32_t addr)
{
    uint8_t b[4] = { cmd, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };
    HAL_SPI_Transmit(&hspi1, b, 4, 10);
}

static int wait_ready(uint32_t timeout_ms)
{
    uint8_t cmd = CMD_RDSR, status;
    uint32_t t0 = HAL_GetTick();
    do {
        CS_LOW();
        HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
        HAL_SPI_Receive(&hspi1, &status, 1, 10);
        CS_HIGH();
        if (!(status & 0x01)) return 1;          /* BUSY bit cleared */
    } while (HAL_GetTick() - t0 < timeout_ms);
    return 0;
}

static void write_enable(void)
{
    uint8_t cmd = CMD_WREN;
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
    CS_HIGH();
}

static void flash_read(uint32_t addr, uint8_t *buf, uint16_t len)
{
    CS_LOW();
    send_cmd_addr(CMD_READ, addr);
    HAL_SPI_Receive(&hspi1, buf, len, 50);
    CS_HIGH();
}

static int flash_program(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    write_enable();
    CS_LOW();
    send_cmd_addr(CMD_PP, addr);
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, 50);
    CS_HIGH();
    return wait_ready(10);                        /* page program: ~0.7 ms typ */
}

static int flash_erase_sector(uint32_t addr)
{
    write_enable();
    CS_LOW();
    send_cmd_addr(CMD_SE, addr);
    CS_HIGH();
    return wait_ready(500);                       /* sector erase: up to ~400 ms */
}

int flashlog_init(uint8_t jedec[3])
{
    uint8_t cmd = CMD_JEDEC;
    CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
    HAL_SPI_Receive(&hspi1, jedec, 3, 10);
    CS_HIGH();

    present = !(jedec[0] == 0x00 || jedec[0] == 0xFF);
    if (!present) return 0;

    /* Resume after reset: the first erased slot is the log head
       (each sector is erased when the log enters it). */
    for (head = 0; head < LOG_SIZE; head += REC_SIZE) {
        uint8_t magic;
        flash_read(LOG_BASE + head, &magic, 1);
        if (magic == 0xFF) break;
    }
    if (head >= LOG_SIZE) head = 0;
    return 1;
}

int flashlog_append(const gw_msg_t *m)
{
    log_rec_t r;

    if (!present) return 0;
    if ((head % SECTOR_SIZE) == 0 && !flash_erase_sector(LOG_BASE + head)) return 0;

    memset(&r, 0xFF, sizeof r);
    r.magic   = REC_MAGIC;
    r.src     = m->src;
    r.len     = (m->len > sizeof r.data) ? (uint8_t)sizeof r.data : m->len;
    r.rsvd    = 0;
    r.tick_ms = m->tick_ms;
    r.id      = m->id;
    memcpy(r.data, m->data, r.len);

    if (!flash_program(LOG_BASE + head, (const uint8_t *)&r, REC_SIZE)) return 0;
    head = (head + REC_SIZE) % LOG_SIZE;
    logged++;
    return 1;
}

uint32_t flashlog_head(void)   { return head / REC_SIZE; }
uint32_t flashlog_logged(void) { return logged; }
