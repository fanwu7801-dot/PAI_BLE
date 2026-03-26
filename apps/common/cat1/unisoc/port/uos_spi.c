#include "asm/spi.h"
#include "system/spinlock.h"
#include "system/timer.h"
#include "clock_cfg.h"
#include "uos_deval.h"
#include "os/os_api.h"
#include "asm/clock.h"

#if TCFG_CAT1_UNISOC_ENABLE

/*************************************************
* 参数：
* spi_module SPI设备或编号
* status SPI状态
* data_buff 非阻塞读写时传入的数据指针
* data_size 实际的读写数据字节数
************************************************/
typedef void (*spi_callback)(uos_uint16_t spi_module, SpiStatusEnum status,
                             uos_uint8_t *data_buff, uos_uint16_t data_size);





#define spi_slave_hdl  2

static spi_callback spi_slave_tx_callback;
static spi_callback spi_slave_rx_callback;

typedef struct slave_async_spi_struct {
    spi_callback callback;
    u8  *data_buff;
    u16  data_size;
} slave_dev_spi;

static slave_dev_spi spi_slave_rx;
static slave_dev_spi spi_slave_tx;



#define SPI_SLAVE_IDLE  0
#define SPI_SLAVE_WRITE 1
#define SPI_SLAVE_READ  2
#define SPI_SLAVE_SYNC_WR 3

static volatile u8 slave_dir = SPI_SLAVE_IDLE;
static u8 is_clock_add_set = 0;






/*************************************************
* 功能： 打开和modem通信的SPI并配置完成，内部逻辑处理使用哪一个SPI
* 输入参数：
* 无
* 返回：
* 错误码
************************************************/


#define SPI2_CS_IN() \
    do { \
        JL_PORTB->DIR |= BIT(11); \
        JL_PORTB->DIE |= BIT(11); \
        JL_PORTB->PU &= ~BIT(11); \
        JL_PORTB->PD &= ~BIT(11); \
    } while (0)

#define SPI2_READ_CS()     (JL_PORTB->IN & BIT(11))


static void spi2_isr();


static spinlock_t lock;

static void unisoc_clock_remove(u32 dat)
{
    clock_remove_set(UNISOC_SPI_CLK);
    is_clock_add_set = 0;
}

uos_err_t uos_spi_open()
{
    spi_open(spi_slave_hdl);
    //spi_set_ie(spi_slave_hdl, 1);
    //配置中断优先级，中断函数
    //request_irq(IRQ_SPI2_IDX, 3, spi2_isr, 1);
    request_irq(IRQ_SPI2_IDX, 0, spi2_isr, 0);
    SPI2_CS_IN();
    return 0;
}




/*************************************************
* 功能： 阻塞式写SPI, 可选实现
* 输入参数：
* write_buff 输入。写数据指针
* write_size 输入。写数据大小
* 返回：
* 实际写入数据大小
************************************************/
uos_uint32_t uos_spi_write(uos_uint8_t *write_buff, uos_uint32_t write_size)
{
    return write_size;
}

/*************************************************
* 功能： 阻塞式读SPI，可选实现
* 输入参数：
* read_buff 输入。读数据指针
* read_size 输入。读数据大小
* 返回：
* 实际读出数据大小
************************************************/
uos_uint32_t uos_spi_read(uos_uint8_t *read_buff, uos_uint32_t read_size)
{
    return read_size;
}

/*************************************************
* 功能： 阻塞式读写SPI，可选实现
* 输入参数：
* write_buff 输入。写数据指针
* read_buff 输入。读数据指针
* size 输入。读写数据大小
* 返回：
* 实际读写数据大小
************************************************/
uos_uint32_t uos_spi_write_read(uos_uint8_t *write_buff, uos_uint8_t *read_buff,
                                uos_uint32_t size)
{
    return size;
}


/*************************************************
* 功能： 非阻塞式写SPI，必须实现
* 输入参数：
* write_buff 输入。写数据指针
* write_size 输入。写数据大小
* tx_callback 输入。写完成回调
* 返回：
* 错误码
************************************************/
uos_err_t uos_spi_write_async(uos_uint8_t *write_buff, uos_uint32_t write_size,
                              spi_callback tx_callback)
{

    ASSERT((u32)write_buff % 4 == 0, "spi dma addr need 4-aligned");

#if 0
    if (SPI2_READ_CS()) {
        printf("%s %d\n", __FUNCTION__, __LINE__);
        return -1;
    }
#endif


#if 0
__again:
    spin_lock(&lock);
    if (slave_dir) {
        spin_unlock(&lock);
        asm("csync");
        asm("csync");
        asm("csync");
        goto __again;
    }
#endif

    slave_dir = SPI_SLAVE_WRITE;
    int clk = clk_get("spi");
    if (clk < 96000000) {
        clock_add_set(UNISOC_SPI_CLK);
        is_clock_add_set = 1;
    }
    //spin_unlock(&lock);

    spi_slave_tx.callback = tx_callback;
    spi_slave_tx.data_buff = write_buff;
    spi_slave_tx.data_size = write_size;

    spi_dma_set_addr_for_isr(spi_slave_hdl, write_buff, write_size, 0);
    return 0;
}

/*************************************************
* 功能： 非阻塞式读SPI，必须实现
* 输入参数：
* read_buff 输入。读数据指针
* read_size 输入。读数据大小
* rx_callback 输入。读完成回调
* 返回：
* 错误码
************************************************/
uos_err_t uos_spi_read_async(uos_uint8_t *read_buff, uos_uint32_t
                             read_size, spi_callback rx_callback)
{
    ASSERT((u32)read_buff % 4 == 0, "spi dma addr need 4-aligned");

#if 0
__again:
    spin_lock(&lock);
    if (slave_dir) {
        spin_unlock(&lock);
        asm("csync");
        asm("csync");
        asm("csync");
        goto __again;
    }
#endif
    slave_dir = SPI_SLAVE_READ;
    int clk = clk_get("spi");
    if (clk < 96000000) {
        clock_add_set(UNISOC_SPI_CLK);
        is_clock_add_set = 1;
    }
    //spin_unlock(&lock);
    spi_slave_rx.callback = rx_callback;
    spi_slave_rx.data_buff = read_buff;
    spi_slave_rx.data_size = read_size;
    spi_dma_set_addr_for_isr(spi_slave_hdl, read_buff, read_size, 1);
    return 0;
}

/*************************************************
* 功能： abort SPI
* 输入参数：
* 无
* 返回：
* 错误码
************************************************/
uos_err_t uos_spi_abort()
{

    return 0;
}
/*************************************************
* 功能： 去初始化SPI
* 输入参数：
* 无
* 返回：
* 错误码
************************************************/
uos_err_t uos_spi_deinit()
{
    spi_close(spi_slave_hdl);
    slave_dir = SPI_SLAVE_IDLE;
    return 0;
}

/*************************************************
* 功能： 作为slave SPI时，读取CS状态
* 输入参数：
* 无
* 返回：
* CS硬件电平状态
************************************************/
uos_int8_t uos_spi_get_cs_state(void)
{
    if (SPI2_READ_CS()) {
        return 1;
    }
    return 0;
}
/*************************************************
* 功能： 关闭SPI
* 输入参数：
* 无
* 返回：
* 错误码
************************************************/
uos_err_t uos_spi_close(void)
{
    spi_close(spi_slave_hdl);
    slave_dir = SPI_SLAVE_IDLE;
    return 0;
}

#define spi_ie_dis(reg)                     ((reg)->CON &= ~BIT(13))
#define spi_disable(reg)                    ((reg)->CON &= ~BIT(0))

//中断函数，需以下特殊声明
__attribute__((interrupt("")))
static void spi2_isr()
{
    int argv[3];
    argv[0] = (int)unisoc_clock_remove;
    argv[1] = 1;

    if (spi_get_pending(spi_slave_hdl)) {
        spi_clear_pending(spi_slave_hdl);
        spi_ie_dis(JL_SPI2);  //tao.shi
        spi_disable(JL_SPI2);
        if (slave_dir == SPI_SLAVE_WRITE) {
            slave_dir = SPI_SLAVE_IDLE;
            if (spi_slave_tx.callback) {
                spi_slave_tx.callback(UOS_MODULE_SPI2, SPI_STATUS_OK, spi_slave_tx.data_buff, spi_slave_tx.data_size);
            }
            if (is_clock_add_set == 1) {
                os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
            }
        } else if (slave_dir == SPI_SLAVE_READ) {
            slave_dir = SPI_SLAVE_IDLE;
            if (spi_slave_rx.callback) {
                spi_slave_rx.callback(UOS_MODULE_SPI2, SPI_STATUS_OK, spi_slave_rx.data_buff, spi_slave_rx.data_size);
            }
            if (is_clock_add_set == 1) {
                os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
            }
        } else if (slave_dir == SPI_SLAVE_SYNC_WR) {

        } else {
            printf("%s %d\n", __FUNCTION__, __LINE__);
        }
    }
}


//通信过程不能进低功耗
static u8 spi_idle_query(void)
{
    return !slave_dir;
}

REGISTER_LP_TARGET(spi_demo_lp_target) = {
    .name = "spi_slave",
    .is_idle = spi_idle_query,
};


/////////////////test///////////////////////////////////////////////////////////////

#if 0//板卡配置

//通用硬件spi 配置
//非推屏使用，改参数对推屏没有影响，推屏有专门硬件模块
#if TCFG_HW_SPI1_ENABLE

const struct spi_platform_data spi1_p_data = {
    .port = {
        IO_PORTB_08,    //CLK
        IO_PORTB_09,    //DO
        IO_PORTB_10,    //DI
    },
    .mode = SPI_MODE_BIDIR_1BIT,
    .clk = 1000000,
    .role = SPI_ROLE_MASTER,
};

#endif


//通用硬件spi 配置
#if TCFG_HW_SPI2_ENABLE
const struct spi_platform_data spi2_p_data = {
    .port = {
        IO_PORTB_02,    //CLK
        IO_PORTB_03,    //DO
        IO_PORTB_04,    //DI
    },
    .mode = SPI_MODE_BIDIR_1BIT,
    .clk = 1000000,
    .role = SPI_ROLE_SLAVE,
};
#endif



#endif



#if 0//


static u8 spi1_send_buf[100] __attribute__((aligned(4)));
static u8 spi1_recv_buf[100] __attribute__((aligned(4)));

#define SPI1_CS_OUT() \
    do { \
        JL_PORTB->DIR &= ~BIT(5); \
        JL_PORTB->DIE |= BIT(5); \
        JL_PORTB->PU &= ~BIT(5); \
        JL_PORTB->PD &= ~BIT(5); \
    } while(0)

#define SPI1_CS_L()     (JL_PORTB->OUT &= ~BIT(5))
#define SPI1_CS_H()     (JL_PORTB->OUT |= BIT(5))



static void spi_test_run()
{
    SPI1_CS_L();

    static u8 i = 0;
    for (i = 0; i < 100; i++) {
        spi1_send_buf[i] = i;
    }
    int err = spi_dma_send(1, spi1_send_buf, 100);
    if (err < 0) {
        puts("spi1 dma send timeout\n");
        goto __out_dma;
    }

    //延时时间需要协商
    //
    err = spi_dma_recv(1, spi1_recv_buf, 100);
    if (err < 0) {
        puts("spi1 dma recv timeout\n");
        goto __out_dma;
    }

    put_buf(spi1_recv_buf, 100);

__out_dma:
    SPI1_CS_H();
}


static u8 spi2_send_buf[100] __attribute__((aligned(4)));
static u8 spi2_recv_buf[100] __attribute__((aligned(4)));

static void spi2_read_callback(uos_uint16_t spi_module, SpiStatusEnum status,
                               uos_uint8_t *data_buff, uos_uint16_t data_size);



static void spi2_write_callback(uos_uint16_t spi_module, SpiStatusEnum status,
                                uos_uint8_t *data_buff, uos_uint16_t data_size)
{

    uos_spi_read_async(spi2_recv_buf, 100, spi2_read_callback);
    //写完数据再次读数
}




static void spi2_read_callback(uos_uint16_t spi_module, SpiStatusEnum status,
                               uos_uint8_t *data_buff, uos_uint16_t data_size)
{

    static u8 i = 6;
    memset(spi2_send_buf, i++, 100);
    uos_spi_write_async(spi2_send_buf, 100, spi2_write_callback); //回复数据..
}





//ces
void jl_spi_test_main()
{
    spi_open(1);
    uos_spi_open();
    SPI1_CS_OUT();
    SPI1_CS_H();
    uos_spi_read_async(spi2_recv_buf, 100, spi2_read_callback); //发起读数请求
    sys_timer_add(NULL, spi_test_run, 3000);
}


#endif
#endif

