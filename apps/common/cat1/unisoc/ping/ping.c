/**
 * This is an example of a "ping" sender (with raw API).
 * It can be used as a start point to maintain opened a network connection, or
 * like a network "watchdog" for your device.
 *
 */

#include "opt.h"

//#ifdef LWIP_RAW /* don't build if not configured for use in lwipopts.h */

#include "ping.h"

#include "lwip/mem.h"
#include "lwip/raw.h"
#include "icmp.h"
#include "icmp6.h"
#include "prot/ip6.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "api.h"
#include "ip_addr.h"
#include "uos_deval.h"
#include "inet_chksum.h"
#include "ip.h"


#if TCFG_CAT1_UNISOC_ENABLE
/**
 * PING_DEBUG: Enable debugging for PING.
 */
#ifndef PING_DEBUG
#define PING_DEBUG     LWIP_DBG_ON
#endif

/** ping receive timeout - in milliseconds */
#ifndef PING_RCV_TIMEO
#define PING_RCV_TIMEO 1000
#endif

/** ping delay - in milliseconds */
#ifndef PING_DELAY
#define PING_DELAY     1000 * 10
#endif

/** ping identifier - must fit on a u16_t */
#ifndef PING_ID
#define PING_ID        0xAFAF
#endif

/** ping additional data size to include in the packet */
#ifndef PING_DATA_SIZE
#define PING_DATA_SIZE 32
#endif

/** ping result action - no default action */
#ifndef PING_RESULT
//#define PING_RESULT(ping_ok) if(ping_ok) LWIP_TRACE("ping [%u] OK: time=[%u]ms\n",ping_seq_num, sys_now()-ping_time)
#endif

#define IP_HEAD_LEN 20
#define IP6_HEAD_LEN 40
#define LWIP_TRACE(fmt, param_num, ...) UOS_LOG_INFO("[LWIP Debug]" fmt, param_num, ##__VA_ARGS__)


/* ping variables */
static u16_t ping_seq_num;
static u32_t ping_time;
static u32_t p_counts;
static struct raw_pcb *ping_pcb = NULL;
static ip_addr_t ping_dst;

typedef u8_t (*raw_recv_fn)(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr);

/*
=====if input is ipv4 address=====
input: in_ip
return NULL: is ipv4 address
*/
char *lwip_parse_ipad(char *in_ip)
{
    char *cp;
    int dots = 0;
    cp = in_ip;
    if (NULL == in_ip) {
        return ("input invalid");
    }

    while (*cp) {
        if (*cp > '9' || *cp < '.' || *cp == '/') {
            return ("all chars must be digits (0-9) or dots (.)");
        }
        if (*cp == '.') {
            dots++;
        }
        cp++;
    }

    if (dots < 1 || dots > 3) {
        return ("string must contain 1 - 3 dots (.)");
    }

    return NULL;
}

/*
=====if input is ipv6 address=====
input: in_ip
return NULL: is ipv6 address
*/
char *lwip_parse_ipad6(char *in_ip)
{
    char *cp = in_ip;
    int is_numericip = 0;
    while (*cp) {
        if (*cp == ':') {
            is_numericip = 1;
            break;
        }
        cp++;
    }

    if (is_numericip) {
        return NULL;
    }
    return " the url  is not a numberic ip";
}

/*
        return 0: success
        return 1: failed
*/
int lwip_getipbyname(char *in_ip, ip_addr_t *out_ip, u8_t type)
{
    int err;
    char *cp;
    //in_ip is ipv4 addr
    cp = lwip_parse_ipad(in_ip);
    if (!cp) {
        ipaddr_aton(in_ip, out_ip);
        LWIP_TRACE("in_ip is ipv4 addr", 0);
        return 0;
    } else {
        //in_ip is ipv6 addr
        cp = lwip_parse_ipad6(in_ip);
        if (!cp) {
            ipaddr_aton(in_ip, out_ip);
            LWIP_TRACE("in_ip is ipv6 addr", 0);
            return 0;
        }
    }
    LWIP_TRACE("lwip_getipbyname ip=%s type=%d", 2, ipaddr_ntoa(out_ip), type);
    err = netconn_gethostbyname_addrtype(in_ip, out_ip, type);
    return err;
}

/** Prepare a echo ICMP request */
static void
ping_prepare_echo(struct icmp_echo_hdr *iecho, u16_t len, int type)
{
    size_t i;
    size_t data_len = len - sizeof(struct icmp_echo_hdr);

    if (type == 0) {
        ICMPH_TYPE_SET(iecho, ICMP6_TYPE_EREQ);
    } else {
        ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    }
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id     = PING_ID;
    iecho->seqno  = htons(++ping_seq_num);

    /* fill the additional data buffer with some data */
    for (i = 0; i < data_len; i++) {
        ((char *)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
    }
    iecho->chksum = (type ? inet_chksum(iecho, len) : 0);

}

/* Ping using the raw ip */
static u8_t
ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, ip_addr_t *addr)
{
    u16_t p_ip_len = 0;
    u8_t resp_data_ptr[80] = {0};
    u8_t icmp_type = 0;
    struct icmp_echo_hdr *iecho;
//    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(pcb);
//    LWIP_UNUSED_ARG(addr);
    LWIP_ASSERT("p != NULL", p != NULL);

    LWIP_TRACE("ping_recv start arg=%s", 1, (u8_t)arg);

    if (IP_HDR_GET_VERSION(p->payload) == 6) {
        p_ip_len = IP6_HEAD_LEN;
        icmp_type = ICMP6_TYPE_EREP;
    } else {
        p_ip_len = IP_HEAD_LEN;
        icmp_type = ICMP_ER;
    }
    //we can also check src ip here, but just egnore it
    LWIP_TRACE("p_ip_len%d+ %d", 2, p_ip_len, sizeof(struct icmp_echo_hdr));
    if ((p->tot_len >= (p_ip_len + sizeof(struct icmp_echo_hdr)))) {
        LWIP_TRACE("tot_len%d >= PBUF_IP_HLEN+ %d", 2, p->tot_len, sizeof(struct icmp_echo_hdr));
        iecho = (struct icmp_echo_hdr *)((u8_t *)p->payload + p_ip_len);
        LWIP_TRACE("ICMP_ER=%d PING_ID=%d ping_seq_num=%d", 2, ICMP_ER, PING_ID, ping_seq_num);
        LWIP_TRACE("iecho->type=%d iecho->code=%d iecho->id=%d iecho->seqno=%d", 4, iecho->type, iecho->code, iecho->id, iecho->seqno);
        if ((iecho->type == icmp_type) && (iecho->id == PING_ID) && (iecho->seqno == htons(ping_seq_num))) {
            LWIP_DEBUGF(PING_DEBUG, ("ping: recv "));
            ip_addr_debug_print(PING_DEBUG, addr);
            LWIP_DEBUGF(PING_DEBUG, (" time=%"U32_F" ms\n", (sys_now() - ping_time)));

            /* do some ping result processing */
//            PING_RESULT(1);
            sprintf(resp_data_ptr, "recv %s reply: time=%"U32_F" ms\r\n", ipaddr_ntoa(addr), (sys_now() - ping_time));
#ifdef SIO_MULTI_AT_COM
            SIO_ATC_WriteCmdRes(resp_data_ptr, strlen(resp_data_ptr), (u8_t)arg);
#else
            //SIO_ATC_WriteCmdRes(resp_data_ptr, strlen(resp_data_ptr));
#endif
            pbuf_free(p);
            return 1; /* eat the packet */
        }
    }
    return 0; /* don't eat the packet */
}

static void
ping_send(void *arg)
{
    struct pbuf *p;
    struct icmp_echo_hdr *iecho;
    struct raw_pcb *pcb = (struct raw_pcb *)arg;
    size_t ping_size = sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE;

    LWIP_TRACE("ping_send start p_counts = %d", 1, p_counts);

    LWIP_ASSERT("ping_timeout: no pcb given!", pcb != NULL);
    LWIP_ASSERT("ping_size <= 0xffff", ping_size <= 0xffff);

    p = pbuf_alloc(PBUF_IP, (u16_t)ping_size, PBUF_RAM);
    if (!p) {
        return;
    }
    if ((p->len == p->tot_len) && (p->next == NULL)) {
        iecho = (struct icmp_echo_hdr *)p->payload;
#if 0
        if (IP_IS_V6(&ping_dst)) {
            ping_prepare_echo(iecho, (u16_t)ping_size, 0);
            send_dest = &ping_dst.u_addr.ip6;
            netif = ip_route(IP6_ADDR_ANY, &ping_dst);
            send_src = ip_2_ip6(ip6_select_source_address(netif, send_dest));

            iecho->chksum = ip6_chksum_pseudo(p, IP6_NEXTH_ICMP6, p->tot_len,
                                              send_src, send_dest);
            LWIP_TRACE("ping_prepare_echo v6 end", 0);
        } else
#endif
        {
            ping_prepare_echo(iecho, (u16_t)ping_size, 1);
            LWIP_TRACE("ping_prepare_echo end", 0);
        }
        extern err_t raw_sendto(struct raw_pcb * pcb, struct pbuf * p, const ip_addr_t *ipaddr);
        raw_sendto(pcb, p, &ping_dst);
        LWIP_TRACE("raw_sendto end", 0);
        ping_time = sys_now();

        LWIP_DEBUGF(PING_DEBUG, ("ping:[%"U32_F"] send ", ping_seq_num));
        ip_addr_debug_print(PING_DEBUG, &ping_dst);
        LWIP_DEBUGF(PING_DEBUG, ("\n"));
    }
    pbuf_free(p);
    p_counts--;

    if (p_counts > 0) {
        LWIP_TRACE("sys_timeout next ping", 0);
        sys_timeout(PING_DELAY, ping_send, pcb);
    } else {
        LWIP_TRACE("sys_timeout ping end", 0);
        p_counts = 0;
    }
}

void
ping_start(u8_t type, u8_t *dst_ip, u32_t count)
{
    int err = 1;
    ip_addr_t addr;
    struct netif *netif_first = NULL;

    memset(&ping_dst, 0, sizeof(ip_addr_t));

    LWIP_TRACE("netconn_gethostbyname_addrtype enter name=%s type=%d ping_dst=%d", 3, dst_ip, type, ping_dst);
    //NETCONN_DNS_IPV4:0  NETCONN_DNS_IPV6:1
    if (!type) {
        err = lwip_getipbyname(dst_ip, &addr, NETCONN_DNS_IPV4);
    } else {
        err = lwip_getipbyname(dst_ip, &addr, NETCONN_DNS_IPV6);
    }

    if (err == ERR_OK) {
        p_counts = count;
        memcpy(&ping_dst, &addr, sizeof(ip_addr_t));
        LWIP_TRACE("PING dst_ip = %s, p_counts = %d err=%d", 3, ipaddr_ntoa(&ping_dst), p_counts, err);
        extern struct raw_pcb *raw_new(u8_t proto);
        ping_pcb = raw_new(IP_IS_V6(&ping_dst) ? IP6_NEXTH_ICMP6 : IP_PROTO_ICMP);
        LWIP_ASSERT("ping_pcb != NULL", ping_pcb != NULL);
        extern void raw_recv(struct raw_pcb * pcb, raw_recv_fn recv, void *recv_arg);
        raw_recv(ping_pcb, ping_recv, NULL);
        LWIP_TRACE("raw_recv set OK", 0);
        //raw_bind(ping_pcb, IP_IS_V6(&ping_dst)?IP6_ADDR_ANY:IP4_ADDR_ANY);
        extern err_t raw_bind(struct raw_pcb * pcb, const ip_addr_t *ipaddr);
        raw_bind(ping_pcb, IP4_ADDR_ANY);
        LWIP_TRACE("raw_bind set OK", 0);
        sys_timeout(PING_DELAY, ping_send, ping_pcb);
        LWIP_TRACE("sys_timeout set OK", 0);
    } else {
        LWIP_TRACE("netconn_gethostbyname failed err=%d", 1, err);
    }
    /*
        else
        {
            ipaddr_aton(dst_ip, &ping_dst);
        }
    */
}

#endif /*TCFG_CAT1_UNISOC_ENABLE*/
