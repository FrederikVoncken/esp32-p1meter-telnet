/*
 * Telnet_Config.h
 */

#ifndef _TELNET_CONFIG_H
#define _TELNET_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define TELNET_PORT                 23 // "port"
#define TELNET_KEEPALIVE_IDLE       5  // "TCP keep-alive idle time(s)"
#define TELNET_KEEPALIVE_INTERVAL   5  // "TCP keep-alive interval time(s)"
#define TELNET_KEEPALIVE_COUNT      3  // "TCP keep-alive interval time(s)"

#define TELNET_MAX_SOCKET           5  // Max number of connections

#ifdef __cplusplus
}
#endif

#endif /* _TELNET_CONFIG_H */
