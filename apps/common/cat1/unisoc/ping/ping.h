#ifndef __PING_H__
#define __PING_H__
#include "lwip/ip_addr.h"

void ping_start(u8_t type, u8_t *dst_ip, u32_t count);

int lwip_getipbyname(char *in_ip, ip_addr_t *out_ip, u8_t type);

#endif /*__PING_H__*/
