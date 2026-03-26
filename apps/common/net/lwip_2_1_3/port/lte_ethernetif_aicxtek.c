#include "lwip/err.h"
#include "etharp.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip.h"
#include "lte_module.h"
#include <string.h>
#include "printf.h"
#include "os/os_api.h"
#include "init.h"
#include "app_config.h"
#include "dns.h"
#include "aic_srv_net.h"
#include "aic_net.h"
#include "aic_ctrl.h"

#if TCFG_APP_CAT1_EN && TCFG_CAT1_FUNC_NET_ENABLE

#if TCFG_CAT1_AICXTEK_ENABLE
/* Define those to better describe your network interface. */
#define IFNAME0 'L'
#define IFNAME1 'T'

extern void lte_ethernetif_input(struct netif *netif);
static void lte_ethernetif_exit(struct netif *netif);

static struct netif *lte_netif = NULL;

struct aic_network_stats net_stats;

u8 *lte_module_get_mac_addr(void)
{
    static u8 lte_mac_addr[6];

    return lte_mac_addr;
}

/* void lte_ethernetif_input(void *param, void *data, int len); */
//6字节的目的MAC＋6字节的源MAC＋2字节的帧类型＋最大数据包(1500)
//unsigned char Rx_Data_Buf[1514];
//unsigned char Tx_Data_Buf[1514];
void *__attribute__((weak)) lte_module_get_txaddr(void)
{
    static u8 send_buff[1500];

    return send_buff;
}

int __attribute__((weak)) lte_module_get_rxpkt_addr_len(struct lte_module_pkg *_oeth_data)
{
    size_t size;
    static u8 recv_buff[1500];

    size = aic_srv_net_read(recv_buff, 1500, 0xFFFFFFFF);
    if (size <= 0) {
        LWIP_DEBUGF(NETIF_DEBUG, ("LTE: recv data fail 0x%x\n", size));
        return -1;
    }

    _oeth_data->data = recv_buff;
    _oeth_data->data_len = size;

    return 0;
}

void __attribute__((weak)) lte_module_tx_packet(u16 length)
{
    size_t size;
    unsigned char *send_buf;

    send_buf = lte_module_get_txaddr();

    size = aic_srv_net_write(send_buf, length, 1000);
    if (size != length) {
        LWIP_DEBUGF(NETIF_DEBUG, ("LTE: send data fail size %d len %d\n", size, length));
    }
}

void __attribute__((weak)) lte_module_release_current_rxpkt(void)
{
    ;
}

static void lte_rx_task(void *p_arg)
{
    while (1) {
        /* Read a received packet from the Ethernet buffers and send it to the lwIP for handling */
        lte_ethernetif_input((struct netif *)p_arg);
    }
}

static int create_lte_rx_task(void *p_arg, int prio)
{
    return os_task_create(lte_rx_task, (void *)p_arg, prio, 0x100, 0, "lte_rx_task");
}

static int delete_lte_rx_task(void)
{
    return os_task_del("lte_rx_task");
}

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
//struct ethernetif {
//  struct eth_addr *ethaddr;
//  /* Add whatever per-interface state that is needed here. */
//};
/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */

static void
wired_low_level_init(struct netif *netif)
{
    /* maximum transfer unit */
    netif->mtu = 1500;

    /* device capabilities */
    /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
    netif->flags = NETIF_FLAG_UP | NETIF_FLAG_LINK_UP;

    /* Do whatever else is needed to initialize interface. */

    /* Initialise the EMAC.  This routine contains code that polls status bits.
       If the Ethernet cable is not plugged in then this can take a considerable
       time.  To prevent this starving lower priority tasks of processing time we
       lower our priority prior to the call, then raise it back again once the
       initialisation is complete. */

    create_lte_rx_task(netif, 8);
    /* netdev_rx_register((void (*)(void *, void *, unsigned int))lte_ethernetif_input, netif); */
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */
static err_t
wired_low_level_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    unsigned int i;
    unsigned char *ptxpkt;

    /* Parameter not used. */

    //获取txbd指向的内存地址
    ptxpkt = lte_module_get_txaddr();

    for (i = 0, q = p; q != NULL; q = q->next) {
        memcpy(&ptxpkt[i], (u8_t *)q->payload, q->len);
        i = i + q->len;
    }
    /* printf("low_level_output len = %d \r\n",i); */
    /* printf("+++++++++++++++++++++send_AAA+++++++++++++++++++\n"); */
    /* printf("%s, len = %d\n", __FUNCTION__, i); */
    /* put_buf(ptxpkt,i); */
    /* printf("+++++++++++++++++++++send_BBB+++++++++++++++++++\n"); */
    lte_module_tx_packet(i);

    net_stats.ul_bytes += i;
    net_stats.ul_packets++;

    LINK_STATS_INC(link.xmit);

    return ERR_OK;
}

err_t lte_nic_ip_output_test(u8 *data, u32 len)
{
    unsigned int i;
    unsigned char *ptxpkt;

    /* Parameter not used. */

    //获取txbd指向的内存地址
    ptxpkt = lte_module_get_txaddr();

    memcpy(ptxpkt, data, len);

    lte_module_tx_packet(len);

    return ERR_OK;
}

err_t lte_nic_ip_output(struct netif *netif, struct pbuf *p,
                        const struct ip4_addr *ipaddr)
{
    return wired_low_level_output(netif, p);
}

err_t lte_nic_ip6_output(struct netif *netif, struct pbuf *p,
                         const struct ip6_addr *ipaddr)
{
    return wired_low_level_output(netif, p);
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory err|= */
static struct pbuf *
wired_low_level_input(struct netif *netif)
{
    struct pbuf *q, *p = NULL;
    int i;
    int ret = 0;
    struct lte_module_pkg lte_pkg = {0};
    /* Obtain the size of the packet and put it into the "len" variable. 并且调整下一个接收oeth_rxbd */
    ret = lte_module_get_rxpkt_addr_len(&lte_pkg);
    /* printf("+++++++++++++++++++++recv_AAA+++++++++++++++++++\n"); */
    /* printf("%s, len = %d\n", __FUNCTION__, lte_pkg.data_len); */
    /* put_buf(lte_pkg.data, lte_pkg.data_len); */
    /* printf("+++++++++++++++++++++recv_BBB+++++++++++++++++++\n"); */

    if (ret < 0) {
        return NULL;
    }
    /* printf("low_level_input len = %d \r\n",len); */
    /* put_buf(prxpkt,len); */

    if (!(netif->flags & NETIF_FLAG_LINK_UP)) {
        alog_info("[LWIP] LTE netif is down 0x%x!\n", netif->flags);
        return NULL;
    }

    net_stats.dl_bytes += lte_pkg.data_len;
    net_stats.dl_packets++;

    /* We allocate a pbuf chain of pbufs from the pool. */
    p = pbuf_alloc(PBUF_RAW, lte_pkg.data_len, PBUF_POOL);

    if (p != NULL) {
        /* We iterate over the pbuf chain until we have read the entire packet into the pbuf. */
        for (i = 0, q = p; q != NULL; q = q->next) {
            /* Read enough bytes to fill this pbuf in the chain. The
             * available data in the pbuf is given by the q->len
             * variable.
             * This does not necessarily have to be a memcpy, you can also preallocate
             * pbufs for a DMA-enabled MAC and after receiving truncate it to the
             * actually received size. In this case, ensure the tot_len member of the
             * pbuf is the sum of the chained pbuf len members.
             */

            //             dma0_cpy((void *)q->payload, (void *)&prxpkt[i], q->len);
            memcpy((void *)q->payload, (void *)&lte_pkg.data[i], q->len);
            i += q->len;
        }

        LINK_STATS_INC(link.recv);
    } else {
        puts("pbuf_alloc fail!\n");
        LINK_STATS_INC(link.memerr);
        LINK_STATS_INC(link.drop);
    }

    lte_module_release_current_rxpkt();

    return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void lte_ethernetif_input(struct netif *netif)
/* void lte_ethernetif_input(void *param, void *data, int len) */
{
    int err;
    struct pbuf *p;
    /* struct netif *netif = (struct netif *)param; */
    /* move received packet into a new pbuf */

    p = wired_low_level_input(netif);

    /* no packet could be read, silently ignore this */
    if (p == NULL) {
        return;
    }

    /* full packet send to tcpip_thread to process */
    err = netif->input(p, netif);

    if (err != ERR_OK) {
        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
        pbuf_free(p);
        p = NULL;
    }
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on err|= */
err_t
lte_ethernetif_init(struct netif *netif)
{
    int32_t ret = 0;
    //  struct ethernetif *ethernetif;

    LWIP_ASSERT("netif != NULL", (netif != NULL));

    //  ethernetif = malloc(sizeof(struct ethernetif));
    //  if (ethernetif == NULL) {
    //    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
    //    return ERR_MEM;
    //  }

    ret = aic_srv_tcpip_net_init();
    if (ret) {
        LWIP_DEBUGF(NETIF_DEBUG, ("[lte_ethernetif_init]: aic_srv_net_init fail\n"));
    }

#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "LWIP_LTE";
#endif /* LWIP_NETIF_HOSTNAME */

    /*
     * Initialize the snmp variables and counters inside the struct netif.
     * The last argument should be replaced with your link speed, in units
     * of bits per second.
     */
#define LINK_SPEED_OF_YOUR_NETIF_IN_BPS (100*1000000)
    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

    //  netif->state = ethernetif;
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    /* We directly use etharp_output() here to save a function call.
     * You can instead declare your own function an call etharp_output()
     * from it if you have to do some checks before sending (e.g. if link
     * is available...) */
    netif->output = lte_nic_ip_output;
    //netif->output_ip6 = lte_nic_ip6_output;

    //  ethernetif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

    /* initialize the hardware */
    wired_low_level_init(netif);

#if LWIP_NETIF_REMOVE_CALLBACK
    netif->remove_callback = lte_ethernetif_exit;
#endif /* LWIP_NETIF_REMOVE_CALLBACK */

    lte_netif = netif;

    memset(&net_stats, 0, sizeof(net_stats));

    return ERR_OK;
}

static void lte_ethernetif_exit(struct netif *netif)
{
    int32_t ret = 0;

    printf("[LWIP] lte netif exit\n");
    delete_lte_rx_task();
    netif->input = NULL;
    lte_netif = NULL;

    ret = aic_srv_net_deinit();
    if (ret) {
        LWIP_DEBUGF(NETIF_DEBUG, ("lte_ethernetif_exit: aic_srv_net_deinit fail\n"));
    }
}

static void lwip_net_info_change_cb(WAN_NET *wan_net_ptr)
{
    int err;
    struct ip4_addr gw;
    struct ip4_addr ipaddr;
    struct ip4_addr netmask;
    ip_addr_t dns_server;
    char host_name[32] = {0};
    struct lan_setting *lan_setting_info = net_get_lan_info(LTE_NETIF);
    struct netif *netif = lte_netif;

    sprintf(host_name, "%s", LOCAL_LTE_HOST_NAME);

    if (wan_net_ptr->pdp_activated) {
        IP4_ADDR(&ipaddr, lan_setting_info->WIRELESS_IP_ADDR0,
                 lan_setting_info->WIRELESS_IP_ADDR1,
                 lan_setting_info->WIRELESS_IP_ADDR2,
                 lan_setting_info->WIRELESS_IP_ADDR3);
        if (ipaddr.addr == wan_net_ptr->ipv4_addr || !wan_net_ptr->ipv4_addr) {
            printf("[LWIP] addr 0x%x new addr 0x%x is equal\n",
                   ipaddr.addr, wan_net_ptr->ipv4_addr);
            return;
        }

        memset(lan_setting_info, 0, sizeof(struct lan_setting));
        lan_setting_info->WIRELESS_IP_ADDR0 = (u8)wan_net_ptr->ipv4_addr;
        lan_setting_info->WIRELESS_IP_ADDR1 = (u8)(wan_net_ptr->ipv4_addr >> 8);
        lan_setting_info->WIRELESS_IP_ADDR2 = (u8)(wan_net_ptr->ipv4_addr >> 16);
        lan_setting_info->WIRELESS_IP_ADDR3 = (u8)(wan_net_ptr->ipv4_addr >> 24);
        lan_setting_info->WIRELESS_NETMASK0 = 255;
        lan_setting_info->WIRELESS_NETMASK1 = 255;
        lan_setting_info->WIRELESS_NETMASK2 = 255;
        lan_setting_info->WIRELESS_NETMASK3 = 0;
        lan_setting_info->WIRELESS_GATEWAY0 = (u8)wan_net_ptr->ipv4_addr;
        lan_setting_info->WIRELESS_GATEWAY1 = (u8)(wan_net_ptr->ipv4_addr >> 8);
        lan_setting_info->WIRELESS_GATEWAY2 = (u8)(wan_net_ptr->ipv4_addr >> 16);
        lan_setting_info->WIRELESS_GATEWAY3 = 1;

        if (netif) {
            dns_local_removehost(host_name, &netif->ip_addr);
            netif_remove(netif);
#if (!LWIP_NETIF_REMOVE_CALLBACK)
            lte_ethernetif_exit(netif);
#endif
        }

        Init_LwIP(LTE_NETIF);
        lwip_renew(LTE_NETIF, 0);

        ip_addr_set_ip4_u32_val(dns_server, wan_net_ptr->ipv4_dns1_addr);
        dns_setserver(0, &dns_server);
        ip_addr_set_ip4_u32_val(dns_server, wan_net_ptr->ipv4_dns2_addr);
        dns_setserver(1, &dns_server);
        netif_set_default(lte_netif);
    } else {
        if (netif) {
            dns_local_removehost(host_name, &netif->ip_addr);
            netif_remove(netif);
#if (!LWIP_NETIF_REMOVE_CALLBACK)
            lte_ethernetif_exit(netif);
#endif
        }

        lte_netif = NULL;
    }
}

void aic_get_network_stats(struct aic_network_stats *stats)
{
    if (!stats) {
        return;
    }

    stats->dl_bytes = net_stats.dl_bytes;
    stats->dl_packets = net_stats.dl_packets;
    stats->ul_bytes = net_stats.ul_bytes;
    stats->ul_packets = net_stats.ul_packets;
}

void aic_reset_network_stats()
{
    memset(&net_stats, 0, sizeof(net_stats));
}

int lwip_pdp_init(void)
{
    aic_register_net_info_cb(lwip_net_info_change_cb);
    return 0;
}

#endif
#endif

