#include <stdio.h>
#include "gw_serialize.h"

static uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int gw_serialize(const gw_msg_t *m, char *out, size_t cap)
{
    static const char hex[] = "0123456789ABCDEF";
    char payload[2 * GW_MSG_MAX_DATA + 1];
    char topic[32];
    int n;

    switch (m->src) {
    case GW_SRC_UART: {                       /* Raspberry Pi command and gateway reply */
        size_t j = 0;
        for (uint8_t i = 0; i < m->len && j < sizeof payload - 1; i++) {
            char c = (char)m->data[i];
            if (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') payload[j++] = c;  /* keep JSON valid */
        }
        payload[j] = '\0';
        snprintf(topic, sizeof topic, "gw/uart/pi");
        break;
    }
    case GW_SRC_I2C: {                        /* left-justified 16-bit, 1/256 degC per LSB */
        if (m->len < 2) return 0;
        int16_t raw   = (int16_t)((m->data[0] << 8) | m->data[1]);
        int32_t centi = ((int32_t)raw * 100) / 256;
        int32_t a     = centi < 0 ? -centi : centi;
        snprintf(payload, sizeof payload, "%s%ld.%02ld",
                 centi < 0 ? "-" : "", (long)(a / 100), (long)(a % 100));
        snprintf(topic, sizeof topic, "gw/i2c/temp");
        break;
    }
    case GW_SRC_SPI:                          /* flash log status */
        if (m->len < 11) return 0;
        snprintf(payload, sizeof payload, "jedec=%02X%02X%02X,head=%lu,logged=%lu",
                 m->data[0], m->data[1], m->data[2],
                 (unsigned long)rd_le32(&m->data[3]), (unsigned long)rd_le32(&m->data[7]));
        snprintf(topic, sizeof topic, "gw/spi/flash");
        break;
    case GW_SRC_CAN: {                        /* raw frame as hex */
        uint8_t i;
        for (i = 0; i < m->len && i < 8; i++) {
            payload[2 * i]     = hex[m->data[i] >> 4];
            payload[2 * i + 1] = hex[m->data[i] & 0x0F];
        }
        payload[2 * i] = '\0';
        snprintf(topic, sizeof topic, "gw/can/0x%03lX", (unsigned long)m->id);
        break;
    }
    default:
        return 0;
    }

    n = snprintf(out, cap, "{\"topic\":\"%s\",\"ts\":%lu,\"payload\":\"%s\"}\n",
                 topic, (unsigned long)m->tick_ms, payload);
    return (n > 0 && (size_t)n < cap) ? n : 0;
}
