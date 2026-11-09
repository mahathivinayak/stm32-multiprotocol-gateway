#include <string.h>
#include "main.h"
#include "gw_port.h"
#include "gw_queue.h"
#include "drv_spi_flash.h"

/* Handles defined in CubeMX-generated main.c (STM32F407G-DISC1) */
extern UART_HandleTypeDef huart2;   /* PA2/PA3 -> USB-UART adapter -> PC -> MQTT    */
extern UART_HandleTypeDef huart3;   /* PD8/PD9 -> Raspberry Pi scheduler, 3.3 V      */
extern I2C_HandleTypeDef  hi2c1;    /* PB6/PB9 -> temp sensor (shared with codec)    */
extern CAN_HandleTypeDef  hcan1;    /* PD0/PD1, loopback mode                        */
/* hspi1 (SPI flash, shared bus with on-board LIS3DSH) is used in drv_spi_flash.c */

/* ---------------- critical section / time / output ---------------- */
static uint32_t s_primask;

void gw_port_enter_critical(void)
{
    uint32_t p = __get_PRIMASK();
    __disable_irq();
    s_primask = p;
}

void gw_port_exit_critical(void) { __set_PRIMASK(s_primask); }
uint32_t gw_port_millis(void)     { return HAL_GetTick(); }

void gw_port_uplink_write(const char *buf, uint32_t len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)len, 100);
}

void gw_port_peer_write(const char *buf, uint32_t len)
{
    HAL_UART_Transmit(&huart3, (uint8_t *)buf, (uint16_t)len, 100);
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* ---------------- UART: Raspberry Pi command lines (interrupt-driven) ---------------- */
static uint8_t  pi_rx_byte;
static gw_msg_t pi_line;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3) return;

    uint8_t b = pi_rx_byte;
    if (b == '\n' || pi_line.len == GW_MSG_MAX_DATA) {
        if (pi_line.len) {
            pi_line.src     = GW_SRC_UART;
            pi_line.id      = 3;
            pi_line.tick_ms = HAL_GetTick();
            gw_queue_push(&pi_line);
        }
        pi_line.len = 0;
    } else if (b != '\r') {
        pi_line.data[pi_line.len++] = b;
    }
    HAL_UART_Receive_IT(&huart3, &pi_rx_byte, 1);         /* re-arm */
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
        HAL_UART_Receive_IT(&huart3, &pi_rx_byte, 1);
}

/* ---------------- CAN1: loopback ---------------- */
static void can_start(void)
{
    CAN_FilterTypeDef f = {0};
    f.FilterBank           = 0;
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation     = ENABLE;
    f.SlaveStartFilterBank = 14;     /* F407 has CAN1 + CAN2 sharing 28 filter banks */
    HAL_CAN_ConfigFilter(&hcan1, &f);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef h;
    gw_msg_t m;
    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &h, m.data) != HAL_OK) break;
        m.src     = GW_SRC_CAN;
        m.id      = (h.IDE == CAN_ID_STD) ? h.StdId : h.ExtId;
        m.len     = (uint8_t)(h.DLC > 8 ? 8 : h.DLC);
        m.tick_ms = HAL_GetTick();
        gw_queue_push(&m);
    }
}

void gw_port_task_can_heartbeat(void)
{
    static uint8_t counter;
    CAN_TxHeaderTypeDef h = {0};
    uint8_t d[2] = { 0xA5, counter++ };
    uint32_t mailbox;
    h.StdId = 0x123;
    h.IDE   = CAN_ID_STD;
    h.RTR   = CAN_RTR_DATA;
    h.DLC   = 2;
    h.TransmitGlobalTime = DISABLE;
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
        HAL_CAN_AddTxMessage(&hcan1, &h, d, &mailbox);
}

/* ---------------- I2C1: temperature sensor (LM75 / TMP102 compatible) ---------------- */
#define TEMP_ADDR  (0x48 << 1)    /* on-board CS43L22 codec is at 0x4A, no conflict */
#define TEMP_REG   0x00

void gw_port_task_temp(void)
{
    gw_msg_t m;
    if (HAL_I2C_Mem_Read(&hi2c1, TEMP_ADDR, TEMP_REG, I2C_MEMADD_SIZE_8BIT, m.data, 2, 50) != HAL_OK)
        return;
    m.src     = GW_SRC_I2C;
    m.id      = 0x48;
    m.len     = 2;
    m.tick_ms = HAL_GetTick();
    gw_queue_push(&m);
}

/* ---------------- SPI1: flash log ---------------- */
static uint8_t jedec[3];

void gw_port_log_append(const gw_msg_t *m) { flashlog_append(m); }

void gw_port_task_flash_status(void)
{
    gw_msg_t m;
    memcpy(m.data, jedec, 3);
    put_le32(&m.data[3], flashlog_head());
    put_le32(&m.data[7], flashlog_logged());
    m.src     = GW_SRC_SPI;
    m.id      = 0;
    m.len     = 11;
    m.tick_ms = HAL_GetTick();
    gw_queue_push(&m);
}

/* ---------------- init ---------------- */
void gw_port_init(void)
{
    /* On-board LIS3DSH shares SPI1: keep its CS (PE3) high so it stays off the bus */
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);

    HAL_UART_Receive_IT(&huart3, &pi_rx_byte, 1);
    can_start();
    flashlog_init(jedec);
}
