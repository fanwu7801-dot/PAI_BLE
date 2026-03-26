#include "app_config.h"

//================================================//
//          lwip btnet 支持
//================================================//
#if USER_SUPPORT_PROFILE_PAN
const int CONFIG_LWIP_NET_ENABLE = 1;
#else
const int CONFIG_LWIP_NET_ENABLE = 0;
#endif


