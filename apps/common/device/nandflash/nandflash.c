#include "nandflash.h"
#include "timer.h"
#include "app_config.h"
#include "asm/clock.h"

#if (defined TCFG_NANDFLASH_DEV_ENABLE && TCFG_NANDFLASH_DEV_ENABLE)

#undef LOG_TAG_CONST
#define LOG_TAG     "[nandFLASH]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"


#define NANDFLASH_SFC1_READ_EN  1

#define NAND_FLASH_TIMEOUT			1000000
#define NAND_BLOCK_SIZE    			0x20000
#define NANDFLASH_ERROR_RETRY		3
//nandflash
#define XT26G01C                    0x0b11
#define XT26G02C                    0x2c24
#define XCSP4AAWH                   0x9cb1
#define F35SQA002G                  0xcd72
#define F35SQA001G                  0xcd71
#define F35SQA512M                  0xcd70
#define F35UQA001G                  0xcd61
#define ZB35Q01C		    		0x5e41
#define GD5F1GM7UEYIG		    	0xc891
#define GD5F2GM7UE		    		0xc892
#define DS35Q1GA_IB		    		0xe571


struct nandflash_data {
    u32 nand_id;
    u32 plane_number;  //设备plane数量,没有填0

    u32 capacity;
    u32 max_erase_cnt; //block的最大擦除次数

    u16 block_number;  //flash中块数量
    u16 page_num;      //块中page数量
    u16 page_size;
    u16 oob_user_offset[2];         //oob(obb区域偏移地址)
    u16 oob_user_size[2];           //oob(obb写入bytes)
    u16 oob_size;      //oob区域大小(byte)

    u8 quad_mode_dummy_num: 4; //0XEB指令dummy数量
    u8 quad_mode_qe: 1; //featrue(0xb0):bit0:QE

    u8 ecc_mask;
    u8 ecc_err;
    u8 plane_select;
    u8 write_enable_position;//写使能的位置，1表示在写入缓存区指令前进行写使能，0表示在写入缓存区指令后写使能
};
struct nandflash_data nand_flash = {0, 0, 0, 0, 0, 0, 0, 0};
static u8 spi_data_width = SPI_MODE_BIDIR_1BIT;


#define MAX_NANDFLASH_PART_NUM      4
struct nandflash_partition {
    const char *name;
    u32 start_addr;
    u32 size;
    struct device device;
};

static struct nandflash_partition nor_part[MAX_NANDFLASH_PART_NUM];

struct nandflash_info {
    u32 flash_id;
    int spi_num;
    int spi_err;
    u8 spi_cs_io;
    u8 spi_r_width;
    u8 part_num;
    u8 open_cnt;
    struct nandflash_partition *const part_list;
    OS_MUTEX mutex;
    u32 max_end_addr;
};

static struct nandflash_info _nandflash = {
    .spi_num = (int) - 1,
    .part_list = nor_part,
};
void nand_flash_reset(void);
int nand_flash_read(u32 addr, u8 *buf, u32 len);
int nand_flash_erase(u32 addr);
extern void nandflash_spi_close(spi_dev spi);
static void nandflash_power_check();
static void nandflash_power_set(int enable);
#define spi_cs_init() \
    do { \
        gpio_set_die(_nandflash.spi_cs_io, 1); \
        gpio_set_direction(_nandflash.spi_cs_io, 0); \
        gpio_set_hd0(_nandflash.spi_cs_io, 1); \
        gpio_set_hd(_nandflash.spi_cs_io, 1); \
        gpio_set_pull_up(_nandflash.spi_cs_io, 1); \
        gpio_set_pull_down(_nandflash.spi_cs_io, 0); \
        gpio_write(_nandflash.spi_cs_io, 1); \
    } while (0)

#define spi_cs_uninit() \
    do { \
        gpio_set_die(_nandflash.spi_cs_io, 0); \
        gpio_set_direction(_nandflash.spi_cs_io, 1); \
        gpio_set_pull_up(_nandflash.spi_cs_io, 0); \
        gpio_set_pull_down(_nandflash.spi_cs_io, 0); \
    } while (0)
#define spi_cs_h()                  gpio_write(_nandflash.spi_cs_io, 1)
#define spi_cs_l()                  gpio_write(_nandflash.spi_cs_io, 0)
#define spi_read_byte()             spi_recv_byte(_nandflash.spi_num, &_nandflash.spi_err)
#define spi_write_byte(x)           spi_send_byte(_nandflash.spi_num, x)
#define spi_dma_read(x, y)          spi_dma_recv(_nandflash.spi_num, x, y)
#define spi_dma_write(x, y)         spi_dma_send(_nandflash.spi_num, x, y)
#define spi_set_width(x)            spi_set_bit_mode(_nandflash.spi_num, x)

static void nandflash_gpio_disable(spi_dev spi)
{
    u8 *port = NULL;
    if (spi == SPI1) {
        port = (u8 *)spi1_p_data.port;
    } else if (spi == SPI2) {
        port = (u8 *)spi2_p_data.port;
    }
    ASSERT(port, "spi_dev %d not find", spi);
    //clk d0 di d2(wp) d3(hold)
    for (int i = 0; i <= 4; i++) {
        if (port[i] != (u8) - 1) {
            gpio_set_direction(port[i], 1);
            gpio_set_die(port[i], 0);
            gpio_set_dieh(port[i], 0);
            gpio_set_pull_up(port[i], 0);
            gpio_set_pull_down(port[i], 0);
        }
    }
}

static struct nandflash_partition *nandflash_find_part(const char *name)
{
    struct nandflash_partition *part = NULL;
    u32 idx;
    for (idx = 0; idx < MAX_NANDFLASH_PART_NUM; idx++) {
        part = &_nandflash.part_list[idx];
        if (part->name == NULL) {
            continue;
        }
        if (!strcmp(part->name, name)) {
            return part;
        }
    }
    return NULL;
}

static struct nandflash_partition *nandflash_new_part(const char *name, u32 addr, u32 size)
{
    struct nandflash_partition *part;
    u32 idx;
    for (idx = 0; idx < MAX_NANDFLASH_PART_NUM; idx++) {
        part = &_nandflash.part_list[idx];
        if (part->name == NULL) {
            break;
        }
    }
    if (part->name != NULL) {
        log_error("create nandflash part fail\n");
        return NULL;
    }
    memset(part, 0, sizeof(*part));
    part->name = name;
    part->start_addr = addr;
    part->size = size;
    if (part->start_addr + part->size > _nandflash.max_end_addr) {
        _nandflash.max_end_addr = part->start_addr + part->size;
    }
    _nandflash.part_num++;
    return part;
}

static void nandflash_delete_part(const char *name)
{
    struct nandflash_partition *part;
    u32 idx;
    for (idx = 0; idx < MAX_NANDFLASH_PART_NUM; idx++) {
        part = &_nandflash.part_list[idx];
        if (part->name == NULL) {
            continue;
        }
        if (!strcmp(part->name, name)) {
            part->name = NULL;
            _nandflash.part_num--;
        }
    }
}

static int nandflash_verify_part(struct nandflash_partition *p)
{
    struct nandflash_partition *part = NULL;
    u32 idx;
    for (idx = 0; idx < MAX_NANDFLASH_PART_NUM; idx++) {
        part = &_nandflash.part_list[idx];
        if (part->name == NULL) {
            continue;
        }
        if ((p->start_addr >= part->start_addr) && (p->start_addr < part->start_addr + part->size)) {
            if (strcmp(p->name, part->name) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

void mdelay(unsigned int ms);
u16 nand_flash_read_id()
{
    u16 id;
    spi_cs_l();
    spi_write_byte(GD_READ_ID);
    spi_write_byte(0x0);
    id = spi_read_byte();
    id <<= 8;
    id |= spi_read_byte();
    spi_cs_h();
    return id;
}

static void nand_write_enable()
{
    spi_cs_l();
    spi_write_byte(GD_WRITE_ENABLE);
    spi_cs_h();
}

static void nand_write_disable()
{
    spi_cs_l();
    spi_write_byte(GD_WRITE_DISABLE);
    spi_cs_h();
}

static void nand_set_features(u8 addr, u8 dat)
{
    spi_cs_l();
    spi_write_byte(GD_SET_FEATURES); //命令头
    spi_write_byte(addr);             //发送protection寄存器地址
    spi_write_byte(dat);             //需要设置的数据
    spi_cs_h();
}

static u8 nand_get_features(u8 addr)
{
    u8 temp;
    spi_cs_l();
    spi_write_byte(GD_GET_FEATURES); //命令头
    spi_write_byte(addr);             //发送protection寄存器地址
    temp = spi_read_byte();
    spi_cs_h();
    return temp;
}

static u8 no_cs_nand_get_features(u8 addr)
{
    u8 temp;
    // spi_cs_l();
    spi_write_byte(GD_GET_FEATURES); //命令头
    spi_write_byte(addr);             //发送protection寄存器地址
    temp = spi_read_byte();
    // spi_cs_h();
    return temp;
}


void nand_flash_set_ecc(u8 en)
{
    int ret = 0;
    u8 cfg_reg = nand_get_features(GD_FEATURES);
    if ((!!(cfg_reg & BIT(4))) == en) {
        printf("nandflash_set_ecc succ\n");
        return;
    }
    if (en) {
        cfg_reg |= BIT(4);
    } else {
        cfg_reg &= ~ BIT(4);
    }
    /* printf("nand_flash_set_ecc cfg_reg:0x%x\n", cfg_reg); */
    nand_set_features(GD_FEATURES, cfg_reg);
}

void nand_flash_set_quad(u8 en)
{
    int ret = 0;
    u8 cfg_reg = nand_get_features(GD_FEATURES);
    if ((!!(cfg_reg & BIT(0))) == en) {
        log_debug("nandflash_set_quad succ\n");
        return;
    }
    if (en) {
        cfg_reg |= BIT(0);
    } else {
        cfg_reg &= ~ BIT(0);
    }
    nand_set_features(GD_FEATURES, cfg_reg);
}

enum {
    NANDFLASH_ECC_FAIL = 1,//ecc已纠正
    NANDFLASH_BAD_BLOCK,  //ecc 无法纠正
    NANDFLASH_OIP_FAIL,    //nandflash 忙
    NANDFLASH_E_FAIL,     //擦除错
    NANDFLASH_P_FAIL,     //写错误
};
void udelay(u32 us);
static u8 nand_flash_wait_ok(u32 timeout)
{
    u8 sta;
    do {
        sta = nand_get_features(GD_GET_STATUS);
        if ((sta & NAND_STATUS_OIP) == 0) {
            sta = 0;
            break;
        }
//It will also be set if the user attempts to program an invalid
//address or a locked or a protected region, including the OTP area.
        if (sta & NAND_STATUS_P_FAIL) { //ing
            log_error("nand_flash prom error![status:0x%x,timeout:%d]\r\n", sta, timeout);
            sta = NANDFLASH_P_FAIL;
            goto _exit;
        }
//It will also be set if the user attempts to erase a locked region.
        if (sta & NAND_STATUS_E_FAIL) { //erase fail
            log_error("nand_flash erase error![status:0x%x,timeout:%d]\r\n", sta, timeout);
            sta = NANDFLASH_E_FAIL;
            goto _exit;
        }
        if ((sta & nand_flash.ecc_mask) == nand_flash.ecc_err) {
            log_error("nand_flash block should be marked as a bad block![status:0x%x,timeout:%d]\r\n", sta, timeout);
            sta = NANDFLASH_BAD_BLOCK;//坏块
            break;
        } else if (sta & nand_flash.ecc_mask) {
            log_error("nand_flash block data should be refreshed![status:0x%x,timeout:%d]\r\n", sta, timeout);
            sta = NANDFLASH_ECC_FAIL;//ecc错误
            /* break; */
        }

        udelay(1);
    } while (--timeout);
    if (timeout == 0) {
        log_error("nand_flash_wait_ok timeout!\r\n");
        sta = NANDFLASH_OIP_FAIL;
    }
_exit:
    if (sta) {
#if (TCFG_EX_FLASH_POWER_IO != NO_CONFIG_PORT)
        nandflash_power_set(0);
        udelay(200);
        nandflash_power_set(1);
#else
        /*flash异常时软件复位不一定有效*/
        nand_flash_reset();
#endif
    }
    return sta;
}

static u8 no_cs_nand_flash_wait_ok(u32 timeout)
{
    udelay(1);
    /* delay(20); */
    u8 sta;
    do {
        sta = no_cs_nand_get_features(GD_GET_STATUS);
        // printf("STA:%d\n",sta);
        if ((sta & NAND_STATUS_OIP) == 0) {
            sta = 0;
            break;
        }
#if 1
//It will also be set if the user attempts to program an invalid
//address or a locked or a protected region, including the OTP area.
        if (sta & NAND_STATUS_P_FAIL) { //ing
            log_error("nand_flash prom error![status:0x%x,timeout:%d]\r\n", sta, timeout);
            sta = NANDFLASH_P_FAIL;
            goto _exit;
        }
//It will also be set if the user attempts to erase a locked region.
        if (sta & NAND_STATUS_E_FAIL) { //erase fail
            log_error("nand_flash erase error![status:0x%x,timeout:%d]\r\n", sta, timeout);
            sta = NANDFLASH_E_FAIL;
            goto _exit;
        }
        if ((sta & nand_flash.ecc_mask) == nand_flash.ecc_err) {
            log_error("nand_flash block should be marked as a bad block![status:0x%x,timeout:%d]\r\n", sta, timeout);
            sta = NANDFLASH_BAD_BLOCK;//坏块
            break;
        } else if (sta & nand_flash.ecc_mask) {
            log_error("nand_flash block data should be refreshed![status:0x%x,timeout:%d]\r\n", sta, timeout);
            sta = NANDFLASH_ECC_FAIL;//ecc错误
            /* break; */
        }
#endif
        udelay(1);
        /* delay(20); */
    } while (--timeout);
    if (timeout == 0) {
        log_error("nand_flash_wait_ok timeout!\r\n");
        sta = NANDFLASH_OIP_FAIL;
    }
_exit:
    if (sta) {
#if (TCFG_EX_FLASH_POWER_IO != NO_CONFIG_PORT)
        nandflash_power_set(0);
        udelay(200);
        nandflash_power_set(1);
#else
        /*flash异常时软件复位不一定有效*/
        nand_flash_reset();
#endif
    }

    return sta;
}


void nand_flash_reset(void)
{
    mdelay(2);//2ms
    // os_time_dly(2);
    spi_cs_l();
    spi_write_byte(0xff); //命令头
    spi_cs_h();
    mdelay(2);//2ms
// 4Gb:must wait Tpor,2Gb:no wait
    mdelay(2);//2ms
// wait ok
    if (nand_flash_wait_ok(NAND_FLASH_TIMEOUT)) {
        log_error("nand flash reset error");
    }
}

static u32 block_page_get(u32 addr, u32 *cache_addr)
{
    u32  page = 0;
    u32 block = 0, bp_mix = 0;

    //<地址超1页范围
    if (addr >= nand_flash.page_size) {
        if (addr >= NAND_BLOCK_SIZE) {
            while (addr >= NAND_BLOCK_SIZE) {
                block++;
                addr -= NAND_BLOCK_SIZE;
            }
            goto _page_count;
        } else {
            goto _page_count;
        }
    }
    //<地址不超1页范围
    else {
        goto _end_count;
    }

_page_count:
    while (addr >= nand_flash.page_size) {
        page++;
        addr -= nand_flash.page_size;
    }

_end_count:
    *cache_addr = addr;
    bp_mix = (block << 6) | page;
    /* log_info("addr change:block:%d,page:%d,cache:%d", block, page, addr); */
    return bp_mix;
}

#define block_change(x) (x)

int nand_flash_erase(u32 address)
{
    os_mutex_pend(&_nandflash.mutex, 0);
    nandflash_power_check();

    u32 bp_mix = 0;
    u32 cache_addr;
    bp_mix = block_page_get(address, &cache_addr);
    u8 sta = 0;
    int retry = NANDFLASH_ERROR_RETRY;
    do {
        nand_write_enable();
        // r_printf("erase 0x%x 0x%x 0x%x 0x%x\n",GD_BLOCK_ERASE,bp_mix >> 16,bp_mix >> 8,bp_mix);
        spi_cs_l();
        spi_write_byte(GD_BLOCK_ERASE);
        spi_write_byte(bp_mix >> 16); //擦除地址:block+page addr
        spi_write_byte(bp_mix >> 8);//
        spi_write_byte(bp_mix);
        spi_cs_h();
        /* nand_write_disable(); */
        //delay 20ns  erase time:4~10ms
        mdelay(1);
        sta = nand_flash_wait_ok(NAND_FLASH_TIMEOUT);
        if (sta) {
            log_error("nand flash erase error status reg %x", sta);
            retry --;
        } else {
            retry = 0;
        }
    } while (retry);
    os_mutex_post(&_nandflash.mutex);
    return sta;
}

void wdt_clear(void);
static void nand_flash_erase_all()
{
    u16 block_num = 1024; //XT26G01C(1Gb)
    nand_set_features(0xA0, 0x00);
    block_num = nand_flash.block_number;
    for (int i = 0; i < block_num; i++) {
        nand_flash_erase(NAND_BLOCK_SIZE * i);
        wdt_clear();
    }
    r_printf("nandflash erase all!!!");
}

//return:0:ok, >0:err, <0:bad block(-1表示0,-2:1,,,)
static int nand_page_read_to_cache(u32 block_page_addr) //PageRead
{
    u8 reg = 0;
    int err_b_addr = 0;
    int retry = NANDFLASH_ERROR_RETRY;
    do {
        spi_cs_l();
        spi_write_byte(GD_PAGE_READ_CACHE);         //PageRead to cache
        spi_write_byte(block_page_addr >> 16);  //send the Page/Block Add
        spi_write_byte((u8)((block_page_addr) >> 8));
        spi_write_byte((u8)block_page_addr);
        spi_cs_h();
        reg = nand_flash_wait_ok(NAND_FLASH_TIMEOUT);
        if (reg) {
            retry --;
        } else {
            retry = 0;
        }
    } while (retry);
    if (reg) {
        log_error("[addr:block:%d,page:%d]nand flash read to cache error", block_page_addr >> 6, block_page_addr & 0x3f);
        if (reg == NANDFLASH_BAD_BLOCK) {
            /* log_error("nand_flash block should be marked as a bad block!\r\n"); */
            err_b_addr = block_page_addr >> 6;
            err_b_addr = -err_b_addr - 1;
            return err_b_addr;
        }
    }
    return reg;
}

//03h/0bh/3bh/6bh/bbh/ebh:addr_bit12:sel plane
static void nand_read_from_cache(u8 *buf, u32 cache_addr, u32 len) //ReadCache
{
    /* cache_addr |= 0x4000; */
    spi_cs_l();//SPI_CS(0);
    spi_write_byte(GD_READ_FROM_CACHE);     //PageRead to cache
    spi_write_byte((u8)((cache_addr) >> 8));
    spi_write_byte((u8)cache_addr);
    spi_write_byte(0xFF);  //send 1byte dummy clk
    spi_dma_read(buf, len);
    spi_cs_h();//SPI_CS(1);
    /* printf("%s\n",__func__); */
}
//3bh/6bh/bbh/ebh:addr_bit12:sel plane
static void nand_read_from_cache_x2(u8 *buf, u32 cache_addr, u32 len) //ReadCache
{
    /* cache_addr |= 0x4000; */
    spi_cs_l();//SPI_CS(0);
    spi_write_byte(GD_READ_FROM_CACHE_X2);     //PageRead to cache
    spi_write_byte((u8)((cache_addr) >> 8));
    spi_write_byte((u8)cache_addr);
    spi_write_byte(0xff);
    spi_set_width(SPI_MODE_UNIDIR_2BIT);
    spi_dma_read(buf, len);
    spi_set_width(spi_data_width);
    spi_cs_h();//SPI_CS(1);
    /* printf("%s\n",__func__); */
}
//03h/0bh/3bh/6bh/bbh/ebh:addr_bit12:sel plane
static void nand_read_from_cache_dual_io(u8 *buf, u32 cache_addr, u32 len) //ReadCache
{
    /* cache_addr |= 0x4000; */
    spi_cs_l();//SPI_CS(0);
    spi_write_byte(GD_READ_FROM_CACHE_DUAL_IO);     //PageRead to cache

    spi_set_width(SPI_MODE_UNIDIR_2BIT);
    spi_write_byte((u8)((cache_addr) >> 8));
    spi_write_byte((u8)cache_addr);
    spi_write_byte(0xff);
    spi_dma_read(buf, len);
    spi_set_width(spi_data_width);

    spi_cs_h();//SPI_CS(1);
    /* printf("%s\n",__func__); */
}
//3bh/6bh/bbh/ebh:addr_bit12:sel plane
static void nand_read_from_cache_x4(u8 *buf, u32 cache_addr, u32 len) //ReadCache
{
    /* cache_addr |= 0x4000; */
    spi_cs_l();//SPI_CS(0);
    spi_write_byte(GD_READ_FROM_CACHE_X4);     //PageRead to cache
    spi_write_byte((u8)((cache_addr) >> 8));
    spi_write_byte((u8)cache_addr);
    spi_write_byte(0xff);
    spi_set_width(SPI_MODE_UNIDIR_4BIT);
    spi_dma_read(buf, len);
    spi_set_width(spi_data_width);

    spi_cs_h();//SPI_CS(1);
    /* printf("%s\n",__func__); */
}
//03h/0bh/3bh/6bh/bbh/ebh:addr_bit12:sel plane
static void nand_read_from_cache_quad_io(u8 *buf, u32 cache_addr, u32 len) //ReadCache
{
    u8 temp[4];
    temp[0] = (u8)((cache_addr) >> 8);
    temp[1] = (u8)cache_addr;
    temp[2] = 0xff;
    temp[3] = 0xff;
    /* cache_addr |= 0x4000; */
    spi_cs_l();//SPI_CS(0);
    spi_write_byte(GD_READ_FROM_CACHE_QUAD_IO);     //PageRead to cache

    spi_set_width(SPI_MODE_UNIDIR_4BIT);
    spi_dma_write(temp, 2 + nand_flash.quad_mode_dummy_num);
    spi_dma_read(buf, len);
    spi_set_width(spi_data_width);

    spi_cs_h();//SPI_CS(1);
    /* printf("%s\n",__func__); */
}
#if NANDFLASH_SFC1_READ_EN
void nand_sfc_read_cache_x4(u8 *buf, u32 offset, u32 len);
#endif
//return:0:ok, >0:err, <0:bad block(-1表示0,-2:1,,,)
static int nand_read(u32 addr, u32 len, u8 *buf)
{
    int reg = 0;
    u32 bp_mix = 0;
    u32 cache_addr;
    bp_mix = block_page_get(addr, &cache_addr);
    bp_mix = block_change(bp_mix);
    cache_addr &= 0x0fff;
    if (nand_flash.plane_select == 1) { //XT26G02C(2Gb)
        if (bp_mix & BIT(6)) {
            cache_addr |= BIT(12);
        }
    }
    reg = nand_page_read_to_cache(bp_mix);
    if (reg < 0) {
        return reg;
    }
    //时间间隔需小于trd
    if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_2BIT) {
        /* nand_set_features(0xa0,0x02);//bit1=1 or 0x82*/
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* nand_read_from_cache_dual_io(buf, cache_addr, len); */
        nand_read_from_cache_x2(buf, cache_addr, len);
        /* nand_set_features(0xa0,0x00);//bit1=1 */
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("X4featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
    } else if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_4BIT) {
        /* nand_set_features(0xa0,0x02);//bit1=1 or 0x82*/
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
        /* nand_read_from_cache_quad_io(buf, cache_addr, len); */
#if NANDFLASH_SFC1_READ_EN
        if ((cache_addr & 0xfff) % 4) {
            nand_read_from_cache_x4(buf, cache_addr, len);
        } else {
            nand_sfc_read_cache_x4(buf, cache_addr, len);
        }
#else
        nand_read_from_cache_x4(buf, cache_addr, len);
#endif
        /* nand_set_features(0xa0,0x00);//bit1=1 */
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("X4featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
    } else {
        nand_read_from_cache(buf, cache_addr, len);
    }
    return reg;
}

int nand_flash_read_page(u16 block, u8 page, u16 offset, u8 *buf, u16 len)
{
    os_mutex_pend(&_nandflash.mutex, 0);
    nandflash_power_check();
    int reg = 0;
    u32 bp_mix = (block << 6) | page;
    u32 cache_addr = offset & 0x0fff;

    if (nand_flash.plane_select == 1) { //XT26G02C(2Gb)
        if (bp_mix & BIT(6)) {
            cache_addr |= BIT(12);
        }
    }
    reg = nand_page_read_to_cache(bp_mix);
    if (reg < 0) {
        goto __read_exit;
    }
    //时间间隔需小于trd
    if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_2BIT) {
        /* nand_set_features(0xa0,0x02);//bit1=1 or 0x82*/
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* nand_read_from_cache_dual_io(buf, cache_addr, len); */
        nand_read_from_cache_x2(buf, cache_addr, len);
        /* nand_set_features(0xa0,0x00);//bit1=1 */
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("X4featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
    } else if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_4BIT) {
        /* nand_set_features(0xa0,0x02);//bit1=1 or 0x82*/
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
        /* nand_read_from_cache_quad_io(buf, cache_addr, len); */
#if NANDFLASH_SFC1_READ_EN
        if ((cache_addr & 0xfff) % 4) {
            nand_read_from_cache_x4(buf, cache_addr, len);
        } else {
            nand_sfc_read_cache_x4(buf, cache_addr, len);
        }
#else
        nand_read_from_cache_x4(buf, cache_addr, len);
#endif
        /* nand_set_features(0xa0,0x00);//bit1=1 */
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("X4featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
    } else {
        nand_read_from_cache(buf, cache_addr, len);
    }
__read_exit:
    os_mutex_post(&_nandflash.mutex);
    return reg;
}

//简化接口
//return:0:ok, >0:err, <0:bad block(-1表示0,-2:1,,,)
int nand_flash_read(u32 offset, u8 *buf,  u32 len)
{
    /* printf("%s() %x l: %x @:%x \n",__func__,(u32)buf,len,offset); */

    int reg = 0;
    int _len = len;
    u8 *_buf = (u8 *)buf;
    os_mutex_pend(&_nandflash.mutex, 0);
    nandflash_power_check();
    u32 first_page_len = nand_flash.page_size - (offset % nand_flash.page_size);
    /* printf("first_page_len %x %x \n", first_page_len, offset % nand_flash.page_size); */
    first_page_len = _len > first_page_len ? first_page_len : _len;
    reg = nand_read(offset, first_page_len, _buf);
    if (reg) {
        log_error("read nandflash addr:%d fail!", offset);
        goto __read_exit;
    }
    _len -= first_page_len;
    _buf += first_page_len;
    offset += first_page_len;
    while (_len) {
        u32 cnt = _len > nand_flash.page_size ? nand_flash.page_size : _len;
        reg = nand_read(offset, cnt, _buf);
        if (reg) {
            log_error("read nandflash addr:%d fail!", offset);
            goto __read_exit;
        }
        _len -= cnt;
        offset += cnt;
        _buf += cnt;
    }
__read_exit:
    os_mutex_post(&_nandflash.mutex);
    return reg;
}

//2Gb-nandflash random read(30h,3fh):

//write:
#if 1
//1Gb 2Gb program 时序不同
//Only four partial-page programs(02h) are allowed on a single page.*******************
//02h/32h/84h/34h:addr_bit12:sel plane
static void nand_program_load(u8 *buf, u32 cache_addr, u16 len)
{
    spi_cs_l();//SPI_CS(0);
    spi_write_byte(GD_PROGRAM_LOAD);         //PageLoad to cache ,change the data
    spi_write_byte((u8)((cache_addr) >> 8));
    spi_write_byte((u8)cache_addr);
    spi_dma_write(buf, len); //将数据放到总线上
    spi_cs_h();//SPI_CS(1);
    /* printf("%s\n",__func__); */
}
//02h/32h/84h/34h:addr_bit12:sel plane
static void nand_program_load_x4(u8 *buf, u32 cache_addr, u16 len)
{
    spi_cs_l();//SPI_CS(0);
    spi_write_byte(GD_PROGRAM_LOAD_X4);         //PageLoad to cache ,change the data
    spi_write_byte((u8)((cache_addr) >> 8));
    spi_write_byte((u8)cache_addr);

    spi_set_width(SPI_MODE_UNIDIR_4BIT);
    spi_dma_write(buf, len); //将数据放到总线上
    spi_set_width(spi_data_width);

    spi_cs_h();//SPI_CS(1);
    /* printf("%s\n",__func__); */
}

static int nand_program_excute(u32 block_page_addr)
{
    int reg;
    int retry = NANDFLASH_ERROR_RETRY;
    do {
        spi_cs_l();
        spi_write_byte(GD_PROGRAM_EXECUTE);
        spi_write_byte(block_page_addr >> 16);  //send the Page/Block Add
        spi_write_byte((u8)((block_page_addr) >> 8));
        spi_write_byte((u8)block_page_addr);
        spi_cs_h();
        reg = nand_flash_wait_ok(NAND_FLASH_TIMEOUT);
        if (reg) {
            log_error("nand flash program error");
            retry --;
        } else {
            retry = 0;
        }
    } while (retry);
    return reg;
}


static int nand_write(u32 addr, u16 len, u8 *buf)
{
    int reg;
    u32 bp_mix = 0;
    u32 cache_addr;

    bp_mix = block_page_get(addr, &cache_addr);
    bp_mix = block_change(bp_mix);
    //printf_u16(cache_addr);
    cache_addr &= 0x0fff;
    if (nand_flash.plane_select == 1) { //XT26G02C(2Gb)
        if (bp_mix & BIT(6)) {
            cache_addr |= BIT(12);
        }
    }
    if (nand_flash.write_enable_position == 1) {
        nand_write_enable();
    }

    if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_4BIT) {
        /* nand_set_features(0xa0,0x02);//bit1=1 */
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
        nand_program_load_x4(buf, cache_addr, len);
        /* nand_set_features(0xa0,0x00);//bit1=1 */
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("X4featrue(a0):p0x%x\n",nand_get_features(0xa0)); */
    } else {
        nand_program_load(buf, cache_addr, len);
    }
    if (nand_flash.write_enable_position == 0) { //XT26G01C(1Gb)
        nand_write_enable();
    }
    reg = nand_program_excute(bp_mix);
    return reg;
}/**/

int nand_flash_write_page(u16 block, u8 page, u8 *buf, u16 len)
{
    os_mutex_pend(&_nandflash.mutex, 0);
    nandflash_power_check();
    int reg;
    u32 bp_mix = 0;
    u32 cache_addr;

    /*bp_mix = block_page_get(addr, &cache_addr);
    bp_mix = block_change(bp_mix);*/
    bp_mix = (block << 6) | page;
    //printf_u16(cache_addr);
    cache_addr = 0;
    if (nand_flash.plane_select == 1) { //XT26G02C(2Gb)
        if (bp_mix & BIT(6)) {
            cache_addr |= BIT(12);
        }
    }
    if (nand_flash.write_enable_position == 1) {
        nand_write_enable();
    }

    if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_4BIT) {
        nand_program_load_x4(buf, cache_addr, len);
    } else {
        nand_program_load(buf, cache_addr, len);
    }
    if (nand_flash.write_enable_position == 0) { //XT26G01C(1Gb)
        nand_write_enable();
    }
    reg = nand_program_excute(bp_mix);
    os_mutex_post(&_nandflash.mutex);
    return reg;
}





//写前需确保该block(128k)已擦除
int nand_flash_write(u32 offset, u8 *buf,  u32 len)
{
    /* printf("%s() %x l: %x @:%x \n",__func__,(u32)buf,len,offset); */

    int reg;
    int _len = len;
    u8 *_buf = (u8 *)buf;
    os_mutex_pend(&_nandflash.mutex, 0);
    nandflash_power_check();
    u32 first_page_len = nand_flash.page_size - (offset % nand_flash.page_size);
    /* printf("first_page_len %x %x \n", first_page_len, offset % nand_flash.page_size); */
    first_page_len = _len > first_page_len ? first_page_len : _len;
    reg = nand_write(offset, first_page_len, _buf);
    if (reg) {
        goto __exit;
    }
    _len -= first_page_len;
    _buf += first_page_len;
    offset += first_page_len;
    while (_len) {
        u32 cnt = _len > nand_flash.page_size ? nand_flash.page_size : _len;
        reg = nand_write(offset, cnt, _buf);
        if (reg) {
            goto __exit;
        }
        _len -= cnt;
        offset += cnt;
        _buf += cnt;
    }
__exit:
    os_mutex_post(&_nandflash.mutex);
    return reg;
}

//1Gb 2Gb program 时序不同
// If the random data is not sequential, then another PROGRAM LOAD RANDOM DATA x1 (84h) command must be issued with a new column address.*******************
//02h/32h/84h/34h:addr_bit12:sel plane
static void nand_program_load_random_data(u8 *buf, u16 cache_addr, u16 len)
{
    spi_cs_l();//SPI_CS(0);
    spi_write_byte(GD_PROGRAM_LOAD_RANDOM_DATA);
    spi_write_byte((u8)((cache_addr) >> 8));
    spi_write_byte((u8)cache_addr);
    if (buf && (len > 0)) {
        /* printf("\n >>>[test]:func = %s,line= %d, addr = %d\n",__FUNCTION__, __LINE__,  cache_addr); */
        /* put_buf(buf, len); */
        spi_dma_write(buf, len); //将数据放到总线上
    }
    spi_cs_h();//SPI_CS(1);
    /* printf("%s\n",__func__); */
}
//02h/32h/84h/34h:addr_bit12:sel plane
static void nand_program_load_random_data_x4(u8 *buf, u16 cache_addr, u16 len)
{
    spi_cs_l();//SPI_CS(0);
    spi_write_byte(GD_PROGRAM_LOAD_RANDOM_DATA_X4);
    spi_write_byte((u8)((cache_addr) >> 8));
    spi_write_byte((u8)cache_addr);
    if (buf && (len > 0)) {
        spi_set_width(SPI_MODE_UNIDIR_4BIT);
        spi_dma_write(buf, len); //将数据放到总线上
        spi_set_width(spi_data_width);
    }
    spi_cs_h();//SPI_CS(1);
    /* printf("%s\n",__func__); */
}

/*
 * 块间或块内页数据移动, 可替换offset位置len长的数据
 * page_src_addr :要移动的页
 * page_dest_addr:移动目标页(需已擦除)
 * offset :需要替换的数据起始地址(<nand_flash.page_size)
 * len    :需替换数据长度. 无则=0
 * buf    :存储的替换的数据.无则NULL
 * return :nandflash status reg(0:ok, >0:err) ;or bad block(<0:bad block num(-1表示0块,-2:1,,,))
 * */
int nand_page_internal_data_move(u32 page_src_addr, u32 page_dest_addr, u16 offset, u16 len, u8 *buf)
{
    u32 bp_mix_src = 0, bp_mix_dst = 0;
    u32 cache_addr_src, cache_addr_dst;
    int bad_block = 0;

    bp_mix_src = block_page_get(page_src_addr, &cache_addr_src);
    bp_mix_src = block_change(bp_mix_src);
    bp_mix_dst = block_page_get(page_dest_addr, &cache_addr_dst);
    bp_mix_dst = block_change(bp_mix_dst);

    //<1、PAGE READ to cache
    bad_block = nand_page_read_to_cache(bp_mix_src);
    if (bad_block < 0) {
        return bad_block;
    }

    //<2、PROGRAM LOAD RANDOM DATA
    cache_addr_dst &= 0x0fff;
    if (nand_flash.plane_select == 1) { //XT26G02C(2Gb)
        if (bp_mix_dst & BIT(6)) {
            cache_addr_dst |= BIT(12);
        }
    }
    if (nand_flash.write_enable_position == 1) {
        nand_write_enable();
    }
    if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_4BIT) {
        /* nand_set_features(0xa0,0x02);//bit1=1 */
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
        nand_program_load_random_data_x4(buf, cache_addr_dst + offset, len);
        /* nand_set_features(0xa0,0x00);//bit1=1 */
        /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
        /* printf("X4featrue(a0):p0x%x\n",nand_get_features(0xa0)); */
    } else {
        nand_program_load_random_data(buf, cache_addr_dst + offset, len);
    }
// If the random data is not sequential, then another PROGRAM LOAD RANDOM DATA x1 (84h) command must be issued with a new column address.*******************
    //<3、WRITE ENABLE
    //<4、PROGRAM EXECUTE
    if (nand_flash.write_enable_position == 0) { //XT26G01C(1Gb)
        nand_write_enable();
    }
    nand_program_excute(bp_mix_dst);
    //<5、GET FEATURE
    return nand_get_features(GD_GET_STATUS);
}

/*
 * 同一plane中块与块数据移动, 可替换块中page_num页的offset位置len长的数据
 * block_src_addr :要移动的块
 * block_dest_addr:移动目标块(需已擦除)
 * page_num:需修改数据的页号(<=63)
 * offset :需要替换的数据起始地址(<nand_flash.page_size)
 * len    :需替换数据长度. 无则=0
 * buf    :存储的替换的数据.无则NULL
 * return :nandflash status reg(0:ok, >0:err) ;or bad block(<0:bad block num(-1表示0块,-2:1,,,))
 * */
int nand_block_internal_data_move(u32 block_src_addr, u32 block_dest_addr, u8 page_num, u16 offset, u16 len, u8 *buf)
{
    /* y_printf(">>>[test]:move, src_addr=0x%x(b:%d), dest_addr=0x%x(b:%d),page_num:%d write len=%d\n", block_src_addr,block_src_addr/NAND_BLOCK_SIZE,  block_dest_addr,block_dest_addr/NAND_BLOCK_SIZE, page_num, len ); */
    u32 bp_mix_src = 0, bp_mix_dst = 0;
    u32 cache_addr_src, cache_addr_dst;
    u8 page_cnt = 0;
    int bad_block = 0;

    bp_mix_src = block_page_get(block_src_addr, &cache_addr_src);
    bp_mix_src = block_change(bp_mix_src);
    bp_mix_src &= ~0x3f;
    bp_mix_dst = block_page_get(block_dest_addr, &cache_addr_dst);
    bp_mix_dst = block_change(bp_mix_dst);
    bp_mix_dst &= ~0x3f;
    cache_addr_dst &= 0x0fff;
    if (nand_flash.plane_select == 1) { //XT26G02C(2Gb)
        if (bp_mix_dst & BIT(6)) {
            cache_addr_dst |= BIT(12);
        }
    }

    u32 wnum = (len + offset) / nand_flash.page_size;
    u32 wlen;
    if (((len + offset) % nand_flash.page_size) || (0 == (len + offset))) {
        wnum += 1;
    }

    for (page_cnt = 0; page_cnt < 64; page_cnt++) {
        //<1、PAGE READ to cache
        bad_block = nand_page_read_to_cache(bp_mix_src + page_cnt);
        if (bad_block < 0) {
            return bad_block;
        }
        //<2、PROGRAM LOAD RANDOM DATA
        if (nand_flash.write_enable_position == 1) { //XT26G02C(2Gb)
            nand_write_enable();
        }
        if ((page_cnt >= page_num) && (page_cnt < (page_num + wnum))) {
            wlen = len;
            if ((wlen + offset) > nand_flash.page_size) {
                len -= (nand_flash.page_size - offset);
                wlen = (nand_flash.page_size - offset);
            }
            /* r_printf(">>>[test]:dst-ofset:0x%x, buf-addr:0x%x, len:%d, page_cnt:%d-w", cache_addr_dst + offset, buf, wlen, page_cnt); */
            if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_4BIT) {
                /* nand_set_features(0xa0,0x02);//bit1=1 */
                /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
                /* printf("featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
                nand_program_load_random_data_x4(buf, cache_addr_dst + offset, wlen);
                /* nand_set_features(0xa0,0x00);//bit1=1 */
                /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
                /* printf("X4featrue(a0):p0x%x\n",nand_get_features(0xa0)); */
            } else {
                nand_program_load_random_data(buf, cache_addr_dst + offset, wlen);
            }
            if (buf) {
                buf += wlen;
                offset = 0;
            }
        } else {
            if (_nandflash.spi_r_width == SPI_MODE_UNIDIR_4BIT) {
                /* nand_set_features(0xa0,0x02);//bit1=1 */
                /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
                /* printf("featrue(a0):r0x%x\n",nand_get_features(0xa0)); */
                nand_program_load_random_data_x4(NULL, cache_addr_dst, 0);
                /* nand_set_features(0xa0,0x00);//bit1=1 */
                /* nand_flash_wait_ok(NAND_FLASH_TIMEOUT); */
                /* printf("X4featrue(a0):p0x%x\n",nand_get_features(0xa0)); */
            } else {
                nand_program_load_random_data(NULL, cache_addr_dst, 0);
            }
        }
        // If the random data is not sequential, then another PROGRAM LOAD RANDOM DATA x1 (84h) command must be issued with a new column address.*******************
        //<3、WRITE ENABLE
        //<4、PROGRAM EXECUTE
        if (nand_flash.write_enable_position == 0) { //XT26G01C(1Gb)
            nand_write_enable();
        }
        nand_program_excute(bp_mix_dst + page_cnt);
    }
    //<5、GET FEATURE
    return nand_get_features(GD_GET_STATUS);

}
#endif


/* void nandflash_spi_set_clk_hd(spi_dev spi); */
int _nandflash_init(const char *name, struct nandflash_dev_platform_data *pdata)
{
    log_info("nandflash_init ! %x %x", pdata->spi_cs_port, pdata->spi_read_width);
    if (_nandflash.spi_num == (int) - 1) {
        _nandflash.spi_num = pdata->spi_hw_num;
        _nandflash.spi_cs_io = pdata->spi_cs_port;
        _nandflash.spi_r_width = pdata->spi_read_width;
        _nandflash.flash_id = 0;
        os_mutex_create(&_nandflash.mutex);
        _nandflash.max_end_addr = 0;
        _nandflash.part_num = 0;
        spi_data_width = pdata->spi_pdata->mode;
        /* nandflash_spi_set_clk_hd(_nandflash.spi_num); */
        gpio_set_hd0(pdata->spi_pdata->port[0], 1);
        gpio_set_hd(pdata->spi_pdata->port[0], 1);
    }
    ASSERT(_nandflash.spi_num == pdata->spi_hw_num);
    ASSERT(_nandflash.spi_cs_io == pdata->spi_cs_port);
    ASSERT(_nandflash.spi_r_width == pdata->spi_read_width);
    struct nandflash_partition *part;
    part = nandflash_find_part(name);
    if (!part) {
        part = nandflash_new_part(name, pdata->start_addr, pdata->size);
        ASSERT(part, "not enough nandflash partition memory in array\n");
        ASSERT(nandflash_verify_part(part) == 0, "nandflash partition %s overlaps\n", name);
        log_info("nandflash new partition %s\n", part->name);
    } else {
        ASSERT(0, "nandflash partition name already exists\n");
    }
    return 0;
}

/* static void clock_critical_enter() */
/* { */

/* } */
/* static void clock_critical_exit() */
/* { */
/* if (!(_nandflash.flash_id == 0 || _nandflash.flash_id == 0xffff)) { */
/* spi_set_baud(_nandflash.spi_num, spi_get_baud(_nandflash.spi_num)); */
/* } */
/* } */
/* CLOCK_CRITICAL_HANDLE_REG(spi_nandflash, clock_critical_enter, clock_critical_exit); */

//2Gb上电自动复位,1Gb未介绍
int _nandflash_open(void *arg)
{
    int reg = 0;
    os_mutex_pend(&_nandflash.mutex, 0);
    log_info("nandflash open\n");
    if (!_nandflash.open_cnt) {
#if NANDFLASH_SFC1_READ_EN
        void sfc1_cache_init();
        sfc1_cache_init();
#endif
        spi_cs_init();
        spi_set_width(_nandflash.spi_r_width);//readX2X4
        spi_open(_nandflash.spi_num);
        // nand_flash_reset();
// nand flash power-up need wait 1.25ms
        mdelay(10);
// wait oip ok

        if (nand_flash_wait_ok(NAND_FLASH_TIMEOUT)) {
            log_error("nand flash power-up error");
            reg = -ENODEV;
            goto __exit;
        }
        _nandflash.flash_id = nand_flash_read_id();
        nand_flash.nand_id = _nandflash.flash_id;
        log_info("nandflash_read_id: 0x%x\n", _nandflash.flash_id);

#ifdef TCFG_NANDFLASH_HEX_ID
        if (_nandflash.flash_id == TCFG_NANDFLASH_HEX_ID) {
            nand_flash.ecc_mask					= TCFG_NANDFLASH_ECC_MASK;
            nand_flash.ecc_err					= TCFG_NANDFLASH_ECC_ERR;
            nand_flash.plane_select				= TCFG_NANDFLASH_PLANE_SEL;
            nand_flash.write_enable_position 	= TCFG_NANDFLASH_WRITE_ENABLE_POS;
            nand_flash.quad_mode_dummy_num		= TCFG_NANDFLASH_QUAD_MODE_DUM_NUM;
            nand_flash.quad_mode_qe				= TCFG_NANDFLASH_QUAD_MODE_QE;
            nand_flash.block_number				= TCFG_NANDFLASH_BLOCK_NUMBER;
            nand_flash.capacity					= TCFG_NANDFLASH_CAPACITY;
            nand_flash.page_num					= TCFG_NANDFLASH_PAGE_NUM;
            nand_flash.page_size				= TCFG_NANDFLASH_PAGE_SIZE;
            nand_flash.oob_size					= TCFG_NANDFLASH_OOB_SIZE;
            nand_flash.oob_user_offset[0]		= TCFG_NANDFLASH_OOB_USER_OFFSET_0;
            nand_flash.oob_user_offset[1]		= TCFG_NANDFLASH_OOB_USER_OFFSET_1;
            nand_flash.oob_user_size[0]			= TCFG_NANDFLASH_OOB_USER_SIZE_0;
            nand_flash.oob_user_size[1]			= TCFG_NANDFLASH_OOB_USER_SIZE_1;
            nand_flash.max_erase_cnt			= TCFG_NANDFLASH_MAX_ERASE_CNT;
#else
        if (0) {
#endif
        } else {
            switch (_nandflash.flash_id) {
            case XT26G01C :
                nand_flash.ecc_mask = 0xf0;
                nand_flash.ecc_err = 0xf0;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 1;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 1024;
                nand_flash.capacity = 128 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 128;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 128;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case XT26G02C :
                nand_flash.ecc_mask = 0x70;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 1;
                nand_flash.write_enable_position = 1;
                nand_flash.quad_mode_dummy_num = 0;
                nand_flash.quad_mode_qe = 0;
                nand_flash.block_number = 2048;
                nand_flash.capacity = 256 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 128;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 128;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case XCSP4AAWH :
                nand_flash.ecc_mask = 0x70;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 1;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 4096;
                nand_flash.capacity = 512 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 64;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 64;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case F35SQA512M :
                nand_flash.ecc_mask = 0x30;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 0;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 512;
                nand_flash.capacity = 64 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 64;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 64;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case F35SQA001G :
                nand_flash.ecc_mask = 0x30;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 0;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 1024;
                nand_flash.capacity = 128 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 64;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 64;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case F35SQA002G :
                nand_flash.ecc_mask = 0x30;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 0;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 2048;
                nand_flash.capacity = 256 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 64;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 64;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case F35UQA001G :
                nand_flash.ecc_mask = 0x30;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 0;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 1024;
                nand_flash.capacity = 128 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 64;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 64;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case ZB35Q01C:
                nand_flash.ecc_mask = 0x30;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 1;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 1024;
                nand_flash.capacity = 128 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 128;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 64;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case GD5F1GM7UEYIG:
                nand_flash.ecc_mask = 0x30;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 1;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 1024;
                nand_flash.capacity = 128 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 64;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 64;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case GD5F2GM7UE:
                nand_flash.ecc_mask = 0x30;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 1;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 2048;
                nand_flash.capacity = 256 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 64;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 0;
                nand_flash.oob_user_size[0]			= 64;
                nand_flash.oob_user_size[1]			= 0;
                break;
            case DS35Q1GA_IB:
                nand_flash.ecc_mask = 0x70;
                nand_flash.ecc_err = 0x20;
                nand_flash.plane_select = 0;
                nand_flash.write_enable_position = 0;
                nand_flash.quad_mode_dummy_num = 1;
                nand_flash.quad_mode_qe = 1;
                nand_flash.block_number = 1024;
                nand_flash.capacity = 128 * 1024 * 1024;
                nand_flash.page_num = 64;
                nand_flash.page_size = 2048;
                nand_flash.oob_size = 64;
                nand_flash.max_erase_cnt = 10 * 10000;
                nand_flash.oob_user_offset[0]			= 0;
                nand_flash.oob_user_offset[1]			= 16;
                nand_flash.oob_user_size[0]			= 8;
                nand_flash.oob_user_size[1]			= 8;
                break;

            }
        }

        if ((_nandflash.flash_id == 0) || (_nandflash.flash_id == 0xffff)) {
            log_error("read nandflash id error !\n");
            reg = -ENODEV;
            goto __exit;
        }

//power-up:need set featurea[A0]=00h
        nand_set_features(0xA0, 0x00);

        if ((nand_flash.quad_mode_qe) && (_nandflash.spi_r_width == SPI_MODE_UNIDIR_4BIT)) {
            nand_flash_set_quad(1);
        }
        nand_flash_set_ecc(true);

        printf("GD_FEATURES:0x%x\n", nand_get_features(GD_FEATURES));

        log_info("nandflash open success !\n");
    }
    if (_nandflash.flash_id == 0 || _nandflash.flash_id == 0xffff)  {
        log_error("re-open nandflash id error !\n");
        reg = -EFAULT;
        goto __exit;
    }
    _nandflash.open_cnt++;
    /* nand_flash_erase_all();// */
__exit:
    os_mutex_post(&_nandflash.mutex);
    return reg;
}

int _nandflash_close(void)
{
    os_mutex_pend(&_nandflash.mutex, 0);
    log_info("nandflash close\n");
    if (_nandflash.open_cnt) {
        _nandflash.open_cnt--;
    }
    if (!_nandflash.open_cnt) {
        spi_close(_nandflash.spi_num);
        nandflash_gpio_disable(_nandflash.spi_num);
        spi_cs_uninit();

        log_info("nandflash close done\n");
    }
    os_mutex_post(&_nandflash.mutex);
    return 0;
}

u32 ftl_get_nand_id()
{
    return _nandflash.flash_id;
}
#include "ftl_api.h"
void ftl_get_nand_info(struct ftl_nand_flash *ftl)
{
    ftl->block_begin = 0;
    ftl->logic_block_num = nand_flash.block_number * 98 / 100; //98%
    ftl->block_end = nand_flash.block_number;
    ftl->page_num = nand_flash.page_num;
    ftl->page_size = nand_flash.page_size;
    ftl->oob_size = nand_flash.oob_size;
    ftl->oob_user_size[0] = nand_flash.oob_user_size[0];
    ftl->oob_user_size[1] = nand_flash.oob_user_size[1];
    ftl->oob_user_offset[0] = nand_flash.oob_user_offset[0];
    ftl->oob_user_offset[1] = nand_flash.oob_user_offset[1];
    ftl->max_erase_cnt = nand_flash.max_erase_cnt;
}
int _nandflash_ioctl(u32 cmd, u32 arg, u32 unit, void *_part)
{
    int reg = 0;
    struct nandflash_partition *part = _part;
    os_mutex_pend(&_nandflash.mutex, 0);
    switch (cmd) {
    case IOCTL_GET_STATUS:
        *(u32 *)arg = 1;
        /* *(u32 *)arg = nand_get_features(GD_GET_STATUS); */
        break;
    case IOCTL_GET_ID:
        *((u32 *)arg) = _nandflash.flash_id;
        break;
    case IOCTL_GET_BLOCK_SIZE:
        *(u32 *)arg = 512;//usb fat
        /* *(u32 *)arg = NAND_BLOCK_SIZE; */
        break;
    case IOCTL_ERASE_BLOCK:
        if ((arg  + part->start_addr) % NAND_BLOCK_SIZE == 0) {
            reg = nand_flash_erase(arg  + part->start_addr);
        }
        break;
    case IOCTL_GET_CAPACITY:
        if (_nandflash.flash_id) {
            *(u32 *)arg = nand_flash.capacity;
        } else {
            log_error("not supported");
        }
        break;
    case IOCTL_FLUSH:
        break;
    case IOCTL_CMD_RESUME:
        /* log_info("%s IOCTL_CMD_RESUME", __func__); */
        break;
    case IOCTL_CMD_SUSPEND:
        /* log_info("%s IOCTL_CMD_SUSPEND", __func__); */
        break;
    case IOCTL_CHECK_WRITE_PROTECT:
        *(u32 *)arg = 0;
        break;
    case IOCTL_GET_PART_INFO:
        u32 *info = (u32 *)arg;
        u32 *start_addr = &info[0];
        u32 *part_size = &info[1];
        *start_addr = part->start_addr;
        *part_size = part->size;
        break;
    default:
        reg = -EINVAL;
        break;
    }
__exit:
    os_mutex_post(&_nandflash.mutex);
    return reg;
}


/*************************************************************************************
 *                                  挂钩 device_api
 ************************************************************************************/

static int nandflash_dev_init(const struct dev_node *node, void *arg)
{
    struct nandflash_dev_platform_data *pdata = arg;
    return _nandflash_init(node->name, pdata);
}

static int nandflash_dev_open(const char *name, struct device **device, void *arg)
{
    struct nandflash_partition *part;
    part = nandflash_find_part(name);
    if (!part) {
        log_error("no nandflash partition is found\n");
        return -ENODEV;
    }
    *device = &part->device;
    (*device)->private_data = part;
    if (atomic_read(&part->device.ref)) {
        return 0;
    }
    return _nandflash_open(arg);
}
static int nandflash_dev_close(struct device *device)
{
    return _nandflash_close();
}
//return: 0:err, =len:ok, <0:block err
static int nandflash_dev_read(struct device *device, void *buf, u32 len, u32 offset)
{

    int reg;
    offset = offset * 512;
    len = len * 512;
    /* r_printf("flash read sector = %d, num = %d\n", offset, len); */
    struct nandflash_partition *part;
    part = (struct nandflash_partition *)device->private_data;
    if (!part) {
        log_error("nandflash partition invalid\n");
        return -EFAULT;
    }
    offset += part->start_addr;

    reg = nand_flash_read(offset, buf, len);
    if (reg) {
        r_printf(">>>[r error]:\n");
        len = 0;
        if (reg < 0) {
            return reg;
        }
    }
    len = len / 512;
    return len;
}
//写前需确保该block(128k)已擦除
static int nandflash_dev_write(struct device *device, void *buf, u32 len, u32 offset)
{
    /* r_printf("flash write sector = %d, num = %d\n", offset, len); */
    int reg = 0;
    offset = offset * 512;
    len = len * 512;
    struct nandflash_partition *part = device->private_data;
    if (!part) {
        log_error("nandflash partition invalid\n");
        return -EFAULT;
    }
    offset += part->start_addr;
    reg = nand_flash_write(offset, buf, len);
    if (reg) {
        r_printf(">>>[w error]:\n");
        len = 0;
    }
    len = len / 512;
    return len;
}
static bool nandflash_dev_online(const struct dev_node *node)
{
    return 1;
}
static int nandflash_dev_ioctl(struct device *device, u32 cmd, u32 arg)
{
    struct nandflash_partition *part = device->private_data;
    if (!part) {
        log_error("nandflash partition invalid\n");
        return -EFAULT;
    }
    return _nandflash_ioctl(cmd, arg, 512,  part);
}
const struct device_operations nandflash_dev_ops = {
    .init   = nandflash_dev_init,
    .online = nandflash_dev_online,
    .open   = nandflash_dev_open,
    .read   = nandflash_dev_read,
    .write  = nandflash_dev_write,//写前需确保该block(128k)已擦除
    .ioctl  = nandflash_dev_ioctl,
    .close  = nandflash_dev_close,
};

u32 get_nand_flash_info(u8 *buf[])
{
    *buf = (u8 *)&nand_flash;
    return sizeof(nand_flash);
}

/*{{{*/
#if NANDFLASH_SFC1_READ_EN
#if 0/*{{{*/
#define     PORT_SPI0_CSC       C
#define     SPI0_CSC            3

#define     PORT_SPI0_CLKC      C
#define     SPI0_CLKC           1

#define     PORT_SPI0_DOC       C
#define     SPI0_DOC            2

#define     PORT_SPI0_DIC       C
#define     SPI0_DIC            4

#define     PORT_SPI0_D2C       C
#define     SPI0_D2C            5

#define     PORT_SPI0_D3C       C
#define     SPI0_D3C            0
void spi1_io_init(u32 mode)
{
    //CS
    SPI_PORT(PORT_SPI0_CSC)->OUT |=  BIT(SPI0_CSC);//从上拉，马上切换到输出1
    SPI_PORT(PORT_SPI0_CSC)->DIR &= ~BIT(SPI0_CSC);
    SPI_PORT(PORT_SPI0_CSC)->PU  &= ~BIT(SPI0_CSC);
    SPI_PORT(PORT_SPI0_CSC)->PD  &= ~BIT(SPI0_CSC);
    /* SFC_FUNC_OUT(PORT_SPI0_CSC, SPI0_CSC) &= ~(3); */

    //CLK
    SPI_PORT(PORT_SPI0_CLKC)->PU  &= ~BIT(SPI0_CLKC);
    SPI_PORT(PORT_SPI0_CLKC)->PD  &= ~BIT(SPI0_CLKC);
    SPI_PORT(PORT_SPI0_CLKC)->DIR &= ~BIT(SPI0_CLKC);
    SPI_PORT(PORT_SPI0_CLKC)->DIE |= BIT(SPI0_CLKC);


    //DO(D0)
    SPI_PORT(PORT_SPI0_DOC)->PU  &= ~BIT(SPI0_DOC);
    SPI_PORT(PORT_SPI0_DOC)->PD  |= BIT(SPI0_DOC);
    SPI_PORT(PORT_SPI0_DOC)->DIR |= BIT(SPI0_DOC);
    SPI_PORT(PORT_SPI0_DOC)->DIE |= BIT(SPI0_DOC);


    //DI(D1)
    SPI_PORT(PORT_SPI0_DIC)->PU  |= BIT(SPI0_DIC);
    SPI_PORT(PORT_SPI0_DIC)->PD  &= ~BIT(SPI0_DIC);
    SPI_PORT(PORT_SPI0_DIC)->DIR |=  BIT(SPI0_DIC);
    SPI_PORT(PORT_SPI0_DIC)->DIE |=  BIT(SPI0_DIC);

    if (mode == 4) {
        /* //WP(D2)                         */
        SPI_PORT(PORT_SPI0_D2C)->PU  |= BIT(SPI0_D2C);
        SPI_PORT(PORT_SPI0_D2C)->PD  &= ~BIT(SPI0_D2C);
        SPI_PORT(PORT_SPI0_D2C)->DIE |=  BIT(SPI0_D2C);
        SPI_PORT(PORT_SPI0_D2C)->DIR |=  BIT(SPI0_D2C);


        //HOLD(D3)
        SPI_PORT(PORT_SPI0_D3C)->PU  |= BIT(SPI0_D3C);
        SPI_PORT(PORT_SPI0_D3C)->PD  &= ~BIT(SPI0_D3C);
        SPI_PORT(PORT_SPI0_D3C)->DIE |=  BIT(SPI0_D3C);
        SPI_PORT(PORT_SPI0_D3C)->DIR |=  BIT(SPI0_D3C);

    }

}
#endif/*}}}*/

#define     SPI_2WIRE_MODE  0
#define     SPI_ODD_MODE    1
#define     SPI_DUAL_MODE   2
#define     SPI_QUAD_MODE   4
#define     FAST_READ_OUTPUT_MODE   (0)
#define     FAST_READ_IO_MODE       (1)
#define     FAST_READ_IO_CONTINUOUS_READ_MODE   (2)
#define     FAST_READ_IO_QPI_READ_MODE   (3)

enum {
    NORMAL_READ = 0,
    FAST_READ,
    DUAL_FAST_READ_OUTPUT,      //CMD 1 WIRE, ADDR 1 WIRE
    QUAD_FAST_READ_OUTPUT,      //CMD 1 WIRE, ADDR 1 WIRE
    DUAL_FAST_READ_IO,          //CMD 1 WIRE, ADDR 2 WIRE
    QUAD_FAST_READ_IO,          //CMD 1 WIRE, ADDR 4 WIRE
    NO_SEND_COMMAND_DUAL_MODE,  //ADDR 2 WIRE
    NO_SEND_COMMAND_QUAD_MODE,  //ADDR 4 WIRE
    DUAL_WIRE_SEND_COMMAND_MODE,//CMD 2 WIRE, ADDR 2 WIRE
    QUAD_WIRE_SEND_COMMAND_MODE,//CMD 4 WIRE, ADDR 4 WIRE
};
#define SFC_SPIE(x)             SFR(sfc_con, 0,  1,  x)
#define SFC_BIDIR(x)            SFR(sfc_con, 3,  1,  x)
#define SFC_RMODE(x)            SFR(sfc_con, 8,  4,  x)
#define SFC_DUMMY(x)            SFR(sfc_con, 16,  4,  x)
#define SFC_CSCNT(x)            SFR(sfc_con, 20,  4,  x)
#define SFC_ADDR4_SEL(x)        SFR(sfc_con, 1,  1,  x)
static void nand_sfc1_init(u32 data_width, u32 mode, u32 clk, u8 dummy, u8 mid)
{
    /* log_debug("sfc1 data_width:%d mode:%d clk:%d,cmd:0x%x,dummy:%d,mid:0x%x\n", data_width, mode, clk,cmd,dummy,mid); */

    /* JL_SPI0->CON &= ~BIT(0); */

    JL_SFC1->CON = 0xf << 20;//cs
    JL_SFC1->CON = 0;//sfc idle cs status 1

    JL_SFC1->BAUD = clk;

    /* JL_SFC1->CON |= BIT(7);//bit7为0的时候cs空闲时是0，有效时是1 */
    u32 sfc_con = BIT(7) | BIT(26) | BIT(25);
    /* u32 sfc_con = BIT(7) | BIT(26); */
    /* u32 capacity = get_flash_capacity(); */
    /* if (capacity > 16 * 1024 * 1024) { */
    /* SFC_ADDR4_SEL(1); */
    /* } */
    /* mode = FAST_READ_IO_CONTINUOUS_READ_MODE; */
    if (data_width == SPI_QUAD_MODE) {
        if (mode == FAST_READ_OUTPUT_MODE) {
            SFC_RMODE(QUAD_FAST_READ_OUTPUT);
            SFC_DUMMY(dummy);
        } else if (mode == FAST_READ_IO_MODE) {
            SFC_RMODE(QUAD_FAST_READ_IO);
            SFC_DUMMY(dummy);
        } else if (mode == FAST_READ_IO_CONTINUOUS_READ_MODE) {
            SFC_RMODE(NO_SEND_COMMAND_QUAD_MODE);
            SFC_DUMMY(dummy);
        }
    } else if (data_width == SPI_DUAL_MODE) {
        if (mode == FAST_READ_OUTPUT_MODE) {
            SFC_RMODE(DUAL_FAST_READ_OUTPUT);
            SFC_DUMMY(dummy);
        } else if (mode == FAST_READ_IO_MODE) {
            SFC_RMODE(DUAL_FAST_READ_IO);
            SFC_DUMMY(dummy);
        } else if (mode == FAST_READ_IO_CONTINUOUS_READ_MODE) {
            SFC_RMODE(NO_SEND_COMMAND_DUAL_MODE);
            SFC_DUMMY(dummy);
        }
    } else {
        SFC_BIDIR(1);
        if (mode == FAST_READ_IO_MODE) {
            SFC_RMODE(FAST_READ);
            SFC_DUMMY(dummy);
        } else {
            JL_SFC1->CON |= BIT(3);
            SFC_RMODE(NORMAL_READ);
            SFC_DUMMY(0);
        }
    }

    SFC_CSCNT(0);
    /* SFC_SPIE(1); */

    SFR(JL_SFC1->CON, 28, 2, 3);
    JL_SFC1->CON |= sfc_con;
    JL_SFC1->CODE = ((mid) << 8) | 0x00;
    /* printf("JL_SFC1->CON = 0x%x\n", JL_SFC1->CON); */
    /* printf("JL_SFC1->BAUD = 0x%x\n", JL_SFC1->BAUD); */
    /* printf("JL_SFC1->CODE = 0x%x\n", JL_SFC1->CODE); */
    /* spi1_io_init(4); */
}
void sfc1_cache_init()
{
    JL_SFC1->BASE_ADR = 0;
    JL_SFC1->ENC_CON = 0;
    /* IcuInitial(); */
    /* DcuInitial(); */
    //sfc1 4bit
    nand_sfc1_init(4, FAST_READ_IO_CONTINUOUS_READ_MODE, 3, 2, 0xff);
}


static void nand_sfc1_resume(u32 disable_spi, u32 clk_div)
{
    if (disable_spi) {
        if (_nandflash.spi_num == SPI0) {
            JL_SPI0->CON &= ~BIT(0);
        } else if (_nandflash.spi_num == SPI1) {
            JL_SPI1->CON &= ~BIT(0);
        } else if (_nandflash.spi_num == SPI2) {
            JL_SPI2->CON &= ~BIT(0);
        }
    }

    JL_SFC1->BAUD = clk_div;
    JL_SFC1->CON |= BIT(0);
}
static void nand_sfc1_suspend(u32 enable_spi)
{
    IcuWaitIdle();
    DcuWaitIdle();
    //wait sfc idle
    while (JL_SFC1->CON & BIT(31));
    //disable sfc
    gpio_set_pull_up(IO_PORTC_03, 1);
    gpio_set_direction(IO_PORTC_03, 1);

    JL_SFC1->BAUD = 0;
    JL_SFC1->CON &= ~BIT(0);

    gpio_set_output_value(IO_PORTC_03, 1);
    gpio_set_pull_up(IO_PORTC_03, 0);
    gpio_set_direction(IO_PORTC_03, 0);

    if (enable_spi) {
        if (_nandflash.spi_num == SPI0) {
            JL_SPI0->CON |= BIT(0);
        } else if (_nandflash.spi_num == SPI1) {
            JL_SPI1->CON |= BIT(0);
        } else if (_nandflash.spi_num == SPI2) {
            JL_SPI2->CON |= BIT(0);
        }
    }
}
u32 clock_get_sfc_freq(void);
static u32 iomc_con0_save  = 0;
void spi12_iomc_deinit(spi_dev spi, u8 mode);
static void nand_exit_spi_code(u32 cache_addr)
{
    u8 temp[3];
    temp[0] = GD_READ_FROM_CACHE_X4;   //PageRead to cache
    temp[1] = (u8)((cache_addr) >> 8);
    temp[2] = (u8)cache_addr;
    /* local_irq_disable(); */
    spi_cs_l();//SPI_CS(0);
    spi_dma_write(temp, 3);

    gpio_set_pull_up(IO_PORTC_03, 0);
    gpio_set_pull_down(IO_PORTC_03, 1);
    gpio_set_direction(IO_PORTC_03, 1);

    u32 sfc_clk = clock_get_sfc_freq();
    if (sfc_clk <= 96000000) {
        sfc_clk = 0;
    } else if (sfc_clk <= 192000000) {
        sfc_clk = 1;
    }
    /* nand_sfc1_resume(1, 1); */
    nand_sfc1_resume(1, sfc_clk);
    iomc_con0_save  = JL_IOMC->CON0;
    SFR(JL_IOMC->CON0, 13, 3, 4);//io mode portc(sfc1)
    spi12_iomc_deinit(_nandflash.spi_num, _nandflash.spi_r_width);
    /* printf("JL_SFC1->CON = 0x%x\n", JL_SFC1->CON); */
    /* printf("JL_SFC1->BAUD = 0x%x,0x%x,0x%x\n", JL_SFC1->BAUD,JL_PORTC->OUT,JL_PORTC->DIR); */
}
void spi12_iomc_init(spi_dev spi, u8 mode);
static void nand_enter_spi_code(void)
{
    nand_sfc1_suspend(1);
    JL_IOMC->CON0 = iomc_con0_save;

    spi12_iomc_init(_nandflash.spi_num, _nandflash.spi_r_width);
    /* local_irq_enable(); */
}

#include "asm/dma_copy.h"

void nand_sfc_read_cache_x4(u8 *buf, u32 offset, u32 len)
{
    nand_exit_spi_code(offset);
    u8 *p = (u8 *)(0x4fcfcfc);
    dma_memcpy(buf, p, len);
    dma_memcpy_wait_idle();
    nand_enter_spi_code();
}
/*************************************************************************************
 *                                 低功耗管理
 ************************************************************************************/
#define EX_FLASH_POWER_WORK     (0)
#define EX_FLASH_POWER_CLOSE    (1)
#define EX_FLASH_POWER_CLOSING  (2)
#define EX_FLASH_POWER_OPENING  (3)

#define USER_MASK_TYPE       (BIT(16)|BIT(17))//删消息池使用
static u16 last_id = 1;//删消息池使用

/* #define FLASH_POWERON_TIME       (1000)//1ms  flash 上电使用延时 */
/* #define FLASH_EXIT_LOWPOWER_TIME  (200)//200us  退出低功耗使用延时 */
#define FLASH_POWEROFF_TIME      (200)//200us  掉电延时

static volatile u8 extern_flash_status = EX_FLASH_POWER_WORK;

extern void udelay(u32 us);
static void nandflash_power_set(int enable)
{
#if (TCFG_EX_FLASH_POWER_IO != NO_CONFIG_PORT)
    if (enable) {
        /* putchar('\n'); */
        /* putchar('n'); */
        /* putchar('p'); */
        /* putchar('e'); */
        /* putchar('\n'); */
        gpio_set_hd0(TCFG_EX_FLASH_POWER_IO, 1);
        gpio_direction_output(TCFG_EX_FLASH_POWER_IO, TCFG_EX_FLASH_POWER_IO_LEVEL);
        udelay(200);
        gpio_set_hd(TCFG_EX_FLASH_POWER_IO, 1);
    } else {
        /* putchar('\n'); */
        /* putchar('n'); */
        /* putchar('p'); */
        /* putchar('d'); */
        /* putchar('\n'); */
        /*默认高阻断电，如果电路上有特殊设计，在此处和board_set_soft_poweroff修改 */
        gpio_set_die(TCFG_EX_FLASH_POWER_IO, 0);
        gpio_set_pull_up(TCFG_EX_FLASH_POWER_IO, 0);
        gpio_set_pull_down(TCFG_EX_FLASH_POWER_IO, 0);
        gpio_set_direction(TCFG_EX_FLASH_POWER_IO, 1);

    }
#endif
}
static void nandflash_poweroff(int priv)
{
    /* printf("norflash_flash_poweroff \n\n"); */
#if (TCFG_EX_FLASH_POWER_IO != NO_CONFIG_PORT)
    ASSERT(extern_flash_status == EX_FLASH_POWER_CLOSING);
    _nandflash_close();
    udelay(FLASH_POWEROFF_TIME);
    nandflash_power_set(0);
    extern_flash_status = EX_FLASH_POWER_CLOSE;
#else
    os_mutex_pend(&_nandflash.mutex, 0);
    //预留，目前大多数nandflash没有低功耗处理，如nandflash支持低功耗命令，可在ioctl中添加流程，并使用下列代码处理
    /* void *hd  = dev_open("nand_flash", NULL); */
    /* if (hd) { */
    /* dev_ioctl(hd, IOCTL_CMD_SUSPEND, NULL); */
    /* } */
    extern_flash_status = EX_FLASH_POWER_CLOSE;
    os_mutex_post(&_nandflash.mutex);
#endif
}




static void nandflash_power_check()
{
    /* log_info("%s stu:%d \n",__func__,extern_flash_status); */
#if (TCFG_EX_FLASH_POWER_IO != NO_CONFIG_PORT)
    if (!extern_flash_status) {
        return;
    }

    local_irq_disable();
    if (extern_flash_status == EX_FLASH_POWER_CLOSING) {
        os_taskq_del_type("app_core", Q_CALLBACK | last_id | USER_MASK_TYPE);
        extern_flash_status = EX_FLASH_POWER_WORK;
        //ASSERT(0);
        //验证临界
    } else if (extern_flash_status == EX_FLASH_POWER_CLOSE) {
        nandflash_power_set(1);
        /* udelay(FLASH_POWERON_TIME); */
        _nandflash_open(NULL);
        extern_flash_status = EX_FLASH_POWER_WORK;
    }
    local_irq_enable();
#else

    if (!extern_flash_status) {
        return;
    }

    local_irq_disable();
    if (extern_flash_status == EX_FLASH_POWER_CLOSING) {
        os_taskq_del_type("app_core", Q_CALLBACK | last_id | USER_MASK_TYPE);
        extern_flash_status = EX_FLASH_POWER_WORK;
        //ASSERT(0);
        //验证临界
    } else if (extern_flash_status == EX_FLASH_POWER_CLOSE) {
        local_irq_enable();
        //预留，目前大多数nandflash没有低功耗处理，如nandflash支持低功耗命令，可在ioctl中添加流程，并使用下列代码处理
        /* void *hd  = dev_open("nand_flash", NULL); */
        /* if (hd) { */
        /* dev_ioctl(hd, IOCTL_CMD_RESUME, NULL); */
        /* } */
        local_irq_disable();
        extern_flash_status = EX_FLASH_POWER_WORK;
    }
    local_irq_enable();
#endif
}

static u8 extern_flash_suspend(u32 timeout)
{
    int msg[3];
    ASSERT(extern_flash_status != EX_FLASH_POWER_OPENING);

    if (extern_flash_status == EX_FLASH_POWER_CLOSE) {
        return 0;
    }

    if (extern_flash_status == EX_FLASH_POWER_CLOSING) {
        return 1;
    }

    extern_flash_status = EX_FLASH_POWER_CLOSING;
    msg[0] = (int)nandflash_poweroff;
    msg[1] = 1;
    msg[2] = (int)NULL;
    last_id++;
    if (!last_id) {
        last_id = 1;
    }
    os_taskq_post_type("app_core", Q_CALLBACK | last_id | USER_MASK_TYPE, 3, msg);

    return 1;
}

static u8 extern_flash_resume(u32 timeout)
{
    /*出低功耗且操作flash时再恢复供电*/
    return 0;
}

//低功耗线程请求所有模块关闭，由对应线程处理
REGISTER_LP_REQUEST(power_flash_target) = {
    .name = "nandflash",
    .request_enter = extern_flash_suspend,
    .request_exit = extern_flash_resume,
};
#endif
#endif

