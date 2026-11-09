#ifndef GW_MSG_H
#define GW_MSG_H
#include <stdint.h>

/* Every field interface converts its data into this common message.
   This is the protocol-abstraction boundary of the gateway. */
typedef enum { GW_SRC_UART = 0, GW_SRC_SPI, GW_SRC_I2C, GW_SRC_CAN } gw_src_t;

#define GW_MSG_MAX_DATA 64

typedef struct {
    uint8_t  src;                     /* gw_src_t */
    uint8_t  len;
    uint32_t id;                      /* CAN ID / UART port / device address */
    uint32_t tick_ms;
    uint8_t  data[GW_MSG_MAX_DATA];
} gw_msg_t;

#endif
