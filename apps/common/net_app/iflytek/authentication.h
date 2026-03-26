#ifndef __AUTHENTICATION_H
#define __AUTHENTICATION_H

#include "generic/includes.h"

// 鉴权接口。内存需要释放
char *ifly_authentication(char *host_name, char *host, char *path);

#endif
