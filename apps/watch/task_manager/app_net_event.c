#include "typedef.h"
#include "lwip.h"
#include "event.h"



extern const int CONFIG_LWIP_NET_ENABLE;
static volatile bt_dhcp_flag = 0;

int bt_lwip_event_cb(void *lwip_ctx, enum LWIP_EVENT event)
{
    struct sys_event e = {0};
    e.type = SYS_NET_EVENT;
    e.u.net.event = event;
    sys_event_notify(&e);
    return 0;
}



int net_get_dhcp_flag()
{
    return bt_dhcp_flag;
}


int net_event_cb(struct sys_event *event)
{
    if (!CONFIG_LWIP_NET_ENABLE) {
        return 0;
    }


    switch (event->u.net.event) {
    case LWIP_BT_DHCP_BOUND_TIMEOUT:
        puts("LWIP_EXT_WIRELESS_DHCP_BOUND_SUCC\n");
        break;
    case LWIP_BT_DHCP_BOUND_RELEASE:
        puts("LWIP_BT_DHCP_BOUND_RELEASE\n");
        bt_dhcp_flag = 0;
        break;
    case LWIP_BT_DHCP_BOUND_SUCC:
        puts("LWIP_BT_DHCP_BOUND_SUCC\n");
        bt_dhcp_flag = 1;
        /* extern int jl_net_time_sync(); */
        /* jl_net_time_sync(); */
        break;
    };
    return 0;
}


