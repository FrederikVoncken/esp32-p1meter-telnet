/*
 * Tool.h
 */

#ifndef _TOOL_H
#define _TOOL_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define Tool_StringCompare(String1, String2) (strcmp((const char*)(String1), (const char*)(String2)) == 0)

#ifdef __cplusplus
}
#endif
#endif /* _TOOL_H */
