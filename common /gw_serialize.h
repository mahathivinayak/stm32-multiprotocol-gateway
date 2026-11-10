#ifndef GW_SERIALIZE_H
#define GW_SERIALIZE_H
#include <stddef.h>
#include "gw_msg.h"

/* Writes one JSON line: {"topic":"gw/...","ts":123,"payload":"..."}\n
   Returns bytes written, or 0 if the message can't be encoded. */
int gw_serialize(const gw_msg_t *m, char *out, size_t cap);

#endif
