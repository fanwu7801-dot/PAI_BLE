#ifndef FIND_MY_H
#define FIND_MY_H

#include "os/os_type.h"
#include "os/os_api.h"
#define   CFG_FINDMY_OPEN_INFO  30
#define   CFG_FINDMY_BIND_INFO  31
#define   CFG_FINDMY_STR_INFO   32
u8 is_findmy_open();
u8 is_findmy_bind();
u8 is_findmy_connecting_flag();

#endif
