//
//  dlog.h
//  lock_detection
//
//  Created by Yano, Takayuki on 2024/10/30.
//

#ifndef __DLOG_H__
#define __DLOG_H__

#include <string.h>

#define ERR_RETn(c)            \
    {                          \
        if (c)                 \
            goto error_return; \
    }

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define ERR_RET(c, s, ...)                                                                \
    {                                                                                     \
        if (c)                                                                            \
        {                                                                                 \
            printf("%s(%d) %s " s "\n", __FILENAME__, __LINE__, __func__, ##__VA_ARGS__); \
            goto error_return;                                                            \
        }                                                                                 \
    }

#define dlog(s, ...) \
    printf("%s(%d) %s " s "\n", __FILENAME__, __LINE__, __func__, ##__VA_ARGS__)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

#endif /* dlog_h */

