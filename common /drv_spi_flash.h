#ifndef DRV_SPI_FLASH_H
#define DRV_SPI_FLASH_H
#include <stdint.h>
#include "gw_msg.h"

int      flashlog_init(uint8_t jedec[3]);     /* 1 if a flash chip answered */
int      flashlog_append(const gw_msg_t *m);  /* 1 on success */
uint32_t flashlog_head(void);                 /* next record slot */
uint32_t flashlog_logged(void);               /* records written since boot */

#endif
