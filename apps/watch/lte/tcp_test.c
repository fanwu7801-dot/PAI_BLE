#include "sock_api/sock_api.h"
#include "os/os_api.h"
#include "app_config.h"
#include "lwip.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "aic_net.h"

#define SERVER_PORT 3300
#define SERVER_ADDR "120.78.8.157"
#define BUF_SIZE 2*1024
#define LOG_TAG "[TCP Client]"
static char send_data[BUF_SIZE];
static unsigned int upload_rate = 0;
static char recv_buf[BUF_SIZE];

static void lte_status(void *p)
{
    printf("network rate U= %d KB/s\r\n", upload_rate / 1024);
    upload_rate = 0;
}

void tcp_client_1(void)
{
    void *sockfd = NULL;
    int bytes;
    struct sockaddr_in server_addr;

    if ((sockfd = sock_reg(AF_INET, SOCK_STREAM, 0, NULL, NULL)) == NULL) {
        printf(LOG_TAG"malloc err\n");
        return;
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    server_addr.sin_port = htons(SERVER_PORT);

    if (0 != sock_connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in))) {
        printf(LOG_TAG"sock_connect err\n");
        return;
    }

    int count = 0;
    memset(send_data, 0xee, BUF_SIZE);

    char *send_str = "S";
    bytes = sock_send(sockfd, send_str, 1, 0);
    if (bytes <= 0) {
        printf("sock_send err, %d\n", bytes);
        sock_unreg(sockfd);
        printf("unreg ok!\n");
    }

    while (1) {
        bytes = sock_recv(sockfd, recv_buf, BUF_SIZE, 0);
        if (bytes < 0) {
            printf("sock_recv err, %d\n", bytes);
            sock_unreg(sockfd);
            printf("unreg ok!\n");
        }
        printf("bytes:%d\n", bytes);
        upload_rate += bytes;
    }
    for (;;) {
        os_time_dly(1);
    }
}

void tcp_client_2(void)
{
    void *sockfd = NULL;
    int bytes;
    struct sockaddr_in server_addr;

    if ((sockfd = sock_reg(AF_INET, SOCK_STREAM, 0, NULL, NULL)) == NULL) {
        printf(LOG_TAG"malloc err\n");
        return;
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    server_addr.sin_port = htons(SERVER_PORT);

    if (0 != sock_connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in))) {
        printf(LOG_TAG"sock_connect err\n");
        return;
    }

    memset(send_data, 0xee, BUF_SIZE);
    while (1) {
        bytes = sock_send(sockfd, send_data, BUF_SIZE, 0);
        printf("bytes:%d\n", bytes);
        if (bytes <= 0) {
            printf("sock_send err, %d\n", bytes);
            sock_unreg(sockfd);
            printf("unreg ok!\n");
            break;
        }
        upload_rate += bytes;
    }
    for (;;) {
        os_time_dly(1);
    }
}

void tcp_client_dl(void *priv)
{
    os_task_create(tcp_client_1, NULL, 30, 1024, 0, "tcp_client_1");
    extern int sys_timer_add(void *priv, void (*func)(void *priv), int msec);
    sys_timer_add(NULL, lte_status, 1000);
}


void tcp_client_ul(void *priv)
{
    os_task_create(tcp_client_2, NULL, 30, 1024, 0, "tcp_client_2");
    extern int sys_timer_add(void *priv, void (*func)(void *priv), int msec);
    sys_timer_add(NULL, lte_status, 1000);
}


