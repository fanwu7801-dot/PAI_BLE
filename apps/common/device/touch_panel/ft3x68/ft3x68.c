/*
 * Privileged & confidential  All Rights/Copyright Reserved by FocalTech.
 *       ** Source code released bellows and hereby must be retained as
 * FocalTech's copyright and with the following disclaimer accepted by
 * Receiver.
 *
 * "THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
 * EVENT SHALL THE FOCALTECH'S AND ITS AFFILIATES'DIRECTORS AND OFFICERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE."
 */

/*****************************************************************************
*
*    The initial work you should do is to implement the i2c communication function based on your platform:
*  platform_i2c_write() and platform_i2c_read().
*
*    In your system, you should register a interrupt at initialization stage(The interrupt trigger mode is
* falling-edge mode), then interrupt handler will run while host detects the falling-edge of INT port.
*
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "touch_panel/ft3x68/ft3x68.h"

#if TCFG_TP_FT3X68_ENABLE
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_CMD_START_DELAY                 		12
#define FTS_DEV_SLAVE_ADDR                    		(0x38<<1)
/* register address */
#define FTS_REG_WORKMODE                    		0x00
#define FTS_REG_WORKMODE_FACTORY_VALUE      		0x40
#define FTS_REG_WORKMODE_SCAN_VALUE					0xC0
#define FTS_REG_FLOW_WORK_CNT               		0x91
#define FTS_REG_POWER_MODE                  		0xA5
#define FTS_REG_GESTURE_EN                  		0xD0
#define FTS_REG_GESTURE_ENABLE              		0x01
#define FTS_REG_GESTURE_OUTPUT_ADDRESS      		0xD3

/*Max point numbers of gesture trace*/
#define MAX_POINTS_GESTURE_TRACE                	6
/*Length of gesture information*/
#define MAX_LEN_GESTURE_INFO            			(MAX_POINTS_GESTURE_TRACE * 4 + 2)

/*Max point numbers of touch trace*/
#define MAX_POINTS_TOUCH_TRACE                  	2
/*Length of touch information*/
#define MAX_LEN_TOUCH_INFO            			    (MAX_POINTS_TOUCH_TRACE * 6 + 2)

/*Max touch points that touch controller supports*/
#define	FTS_MAX_POINTS_SUPPORT						1
#define TCFG_FT3X68_USER_IIC_TYPE					TCFG_TOUCH_USER_IIC_TYPE
#if TCFG_FT3X68_USER_IIC_TYPE
#define iic_init(iic)                       hw_iic_init(iic)
#define iic_uninit(iic)                     hw_iic_uninit(iic)
#define iic_start(iic)                      hw_iic_start(iic)
#define iic_stop(iic)                       hw_iic_stop(iic)
#define iic_tx_byte(iic, byte)              hw_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               hw_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         hw_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        hw_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    hw_iic_suspend(iic)
#define iic_resume(iic)                     hw_iic_resume(iic)
#else
#define iic_init(iic)                       soft_iic_init(iic)
#define iic_uninit(iic)                     soft_iic_uninit(iic)
#define iic_start(iic)                      soft_iic_start(iic)
#define iic_stop(iic)                       soft_iic_stop(iic)
#define iic_tx_byte(iic, byte)              soft_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               soft_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         soft_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        soft_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    soft_iic_suspend(iic)
#define iic_resume(iic)                     soft_iic_resume(iic)
#endif

/*****************************************************************************
* Private variables/functions
*****************************************************************************/
static struct fts_ts_data _fts_data = {
    .suspended = 0,
    .gesture_support = FTS_GESTURE_EN,
    .esd_support = 1,
    .iic_hdl = 0,
    .iic_delay = 70,
    .int_io_init = 0,
};

static int fts_gpio_interrupt_handler(void);
/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data = &_fts_data;
void mdelay(u32 ms);
void delay(volatile u32 t);
/*delay, unit: millisecond */
void fts_msleep(unsigned long msec)
{
    mdelay(msec);
}

/*****************************************************************************
  platform_tp_deal
*****************************************************************************/
void ctp_enter_sleep(void)
{
    fts_ts_suspend();
}
void ctp_exit_sleep(void)
{
    fts_ts_resume();
}

static u8 tp_last_staus = ELM_EVENT_TOUCH_UP;
static int tp_down_cnt = 0;
#ifndef abs
#define abs(x)  ((x)>0?(x):-(x) )
#endif

#undef log_info
#define log_info(fmt, ...)

#define ENERGY_XDISTANCE	15
#define ENERGY_YDISTANCE	15
#define ENERGY_TIME			20 //ms
struct touch_kinetic_energy {
    int t1;
    int x1;
    int y1;

    int t2;
    int x2;
    int y2;
};
static struct touch_kinetic_energy tke = {0};

static void touch_info_reset()
{
    tp_last_staus = ELM_EVENT_TOUCH_UP;
    tp_down_cnt = 0;
}
static void tke_start(int x, int y)
{
    memcpy(&tke.t2, &tke.t1, sizeof(tke) / 2);

    if ((tke.x1 == x) && (tke.y1 == y)) {
        tke.t1 = tke.t1;
    } else {
        tke.t1 = jiffies_msec();
    }
    tke.x1 = x;
    tke.y1 = y;
    /* printf("tke start %d, %d, %d\n", tke.t1, tke.x1, tke.y1); */
}

static void tke_stop(int x, int y, struct touch_event *up_t)
{
    struct touch_event t = {0};
    u8 xdir, ydir;

    if (tke.t1 == 0) {
        //在这里发送up消息，因为没有惯性，所以直接发送
        ui_touch_msg_post(up_t);
        return;
    }
    tke.t1 = tke.t1 - tke.t2;
    tke.x1 = tke.x1 - tke.x2;
    tke.y1 = tke.y1 - tke.y2;

    /* printf("tke end %d, %d, %d\n", tke.t1, tke.x1, tke.y1); */

    if ((tke.t1 < ENERGY_TIME) &&
        ((abs(tke.x1) > ENERGY_XDISTANCE) || abs(tke.y1) > ENERGY_YDISTANCE)) {

        //在这里发送up消息，并附上有惯性能量标志
        up_t->has_energy = 1;
        ui_touch_msg_post(up_t);

        if (abs(tke.x1) <= ENERGY_XDISTANCE) {
            tke.x1 = 0;
        }
        if (abs(tke.y1) <= ENERGY_YDISTANCE) {
            tke.y1 = 0;
        }
        xdir = (tke.x1 < 0) ? 1 : 2;
        ydir = (tke.y1 < 0) ? 1 : 2;
        /* printf("xdir %d, ydir %d\n", xdir, ydir); */

        t.event = ELM_EVENT_TOUCH_ENERGY;
        t.x = (tke.x1 << 16) | (tke.t1 & 0xffff);
        t.y = (tke.y1 << 16) | (ydir << 8) | (xdir & 0xff);
        ui_touch_msg_post(&t);

        /* printf("tke out %d, %d, %d, %d, %d\n", (t.x & 0xffff), (t.x >> 16), (t.y >> 16), (t.y & 0xff), (t.y >> 8) & 0xff); */

    } else {
        //在这里发送up消息，因为没有惯性，所以直接发送
        ui_touch_msg_post(up_t);
    }

    memset(&tke, 0, sizeof(tke));

}


struct touch_info {
    int y;
    int x;
    int p;
    int id;
    int count;
};

#if TCFG_LCD_FLIP_ENABLE
#define Y_MIRROR  0
#define X_MIRROR  0
#else
#define Y_MIRROR  1
#define X_MIRROR  1
#endif
#define SW_X_Y	  1
#define VK_Y      		466
#define VK_X      		466
#define VK_Y_MOVE   	100//227
#define VK_X_MOVE      	100//227
#define VK_X_Y_DIFF    	47
#define X_SINGLE_MOVE   0 //单次滑动距离上限阈值，用于过滤多点触摸时误发L/R_MOVE事件 0 关闭 ；阈值，根据tp实际情况调整
#define Y_SINGLE_MOVE   0 //y方向过滤



static void tpd_down(int raw_x, int raw_y, int x, int y, int p)
{
    struct touch_event t = {0};
    static int first_x = 0;
    static int first_y = 0;
#if (X_SINGLE_MOVE && Y_SINGLE_MOVE)
    static s16 last_x = 0;
    static s16 last_y = 0;
    static s16 dist_x = 0;
    static s16 dist_y = 0;
#endif
    static u8 move_flag = 0;

    if (x < 0) {
        x = 0;
    }
    if (x > (VK_X - 1)) {
        x = VK_X - 1;
    }
    if (y < 0) {
        y = 0;
    }
    if (y > (VK_Y - 1)) {
        y = VK_Y - 1;
    }
#if Y_MIRROR
    x = VK_X - x - 1;
#endif

#if X_MIRROR
    y = VK_Y - y - 1;
#endif

    if ((tp_last_staus == ELM_EVENT_TOUCH_DOWN) && (x == first_x) && (y == first_y)) {
        tp_down_cnt++;
        if (tp_down_cnt < 30) {
            return;
        }
        tp_last_staus = ELM_EVENT_TOUCH_HOLD;
        tp_down_cnt = 0;

        t.event = tp_last_staus;
        t.x = x;
        t.y = y;
        log_info("----tpd_hold----");
        ui_touch_msg_post(&t);
        return;
    }

    /* log_info("D[%4d %4d %4d]\n", x, y, p); */
    //log_info("tp_last_status:%d mf:%d x(%d->%d=%d:%d:%d),y(%d->%d=%d:%d:%d)",tp_last_staus,move_flag,first_x,x,x - first_x,last_x,dist_x,first_y,y,y-first_y,last_y,dist_y);
    if (tp_last_staus != ELM_EVENT_TOUCH_UP) {
        int x_move = abs(x - first_x);
        int y_move = abs(y - first_y);
#if (X_SINGLE_MOVE && Y_SINGLE_MOVE)
        int x_last_move = (x - last_x);
        int y_last_move = (y - last_y);
        if (!move_flag && (abs(dist_x) >= VK_X_MOVE || abs(dist_y) >= VK_Y_MOVE) && (abs(abs(dist_x) - abs(dist_y)) >= VK_X_Y_DIFF)) {
            if (abs(dist_x)  > abs(dist_y)) {
                if (dist_x > 0) {
                    tp_last_staus = ELM_EVENT_TOUCH_R_MOVE;
                    log_info("----tpd_rigth_move----");
                } else {
                    tp_last_staus = ELM_EVENT_TOUCH_L_MOVE;
                    log_info("----tpd_left_move----");
                }

            } else {
                if (dist_y > 0) {
                    tp_last_staus = ELM_EVENT_TOUCH_D_MOVE;
                    log_info("----tpd_down_move----");
                } else {
                    tp_last_staus = ELM_EVENT_TOUCH_U_MOVE;
                    log_info("----tpd_up_move----");
                }
            }
            last_x = x;
            last_y = y;
            dist_x += x_last_move;
            dist_y += y_last_move;
            move_flag = 1;
#else
        if (!move_flag && (x_move >= VK_X_MOVE || y_move >= VK_Y_MOVE) && (abs(x_move - y_move) >= VK_X_Y_DIFF)) {
            if (x_move > y_move) {
                if (x > first_x) {
                    tp_last_staus = ELM_EVENT_TOUCH_R_MOVE;
                    log_info("----tpd_rigth_move----");
                } else {
                    tp_last_staus = ELM_EVENT_TOUCH_L_MOVE;
                    log_info("----tpd_left_move----");
                }

            } else {

                if (y > first_y) {
                    tp_last_staus = ELM_EVENT_TOUCH_D_MOVE;
                    log_info("----tpd_down_move----");
                } else {
                    tp_last_staus = ELM_EVENT_TOUCH_U_MOVE;
                    log_info("----tpd_up_move----");
                }
            }
            move_flag = 1;
#endif

        } else {
            if ((x == first_x) && (y == first_y)) {
                return;
            }
#if (X_SINGLE_MOVE && Y_SINGLE_MOVE)
            if ((abs(x_last_move) > X_SINGLE_MOVE) || (abs(y_last_move) > Y_SINGLE_MOVE)) {
                return;
            }
            last_x = x;
            last_y = y;
            dist_x += x_last_move;
            dist_y += y_last_move;
#endif
            tp_last_staus = ELM_EVENT_TOUCH_MOVE;
            log_info("----tpd_move----");
        }
        /* tp_last_staus = ELM_EVENT_TOUCH_HOLD; */
        tke_start(x, y);
    } else {
#if (X_SINGLE_MOVE && Y_SINGLE_MOVE)
        if (tp_last_staus != ELM_EVENT_TOUCH_UP) {
            return;
        }
        tp_last_staus = ELM_EVENT_TOUCH_DOWN;
        first_x = x;
        first_y = y;
        last_x = x;
        last_y = y;
        dist_x = 0;
        dist_y = 0;
#else
        tp_last_staus = ELM_EVENT_TOUCH_DOWN;
        first_x = x;
        first_y = y;
#endif
        move_flag = 0;
        log_info("----tpd_down----");
    }

    t.event = tp_last_staus;
    t.x = x;
    t.y = y;
    ui_touch_msg_post(&t);
}

static void tpd_up(int raw_x, int raw_y, int x, int y, int p)
{
    struct touch_event t = {0};

    if (x < 0) {
        x = 0;
    }
    if (x > (VK_X - 1)) {
        x = VK_X - 1;
    }
    if (y < 0) {
        y = 0;
    }
    if (y > (VK_Y - 1)) {
        y = VK_Y - 1;
    }

#if Y_MIRROR
    x = VK_X - x - 1;
#endif

#if X_MIRROR
    y = VK_Y - y - 1;
#endif

    /* log_info("U[%4d %4d %4d]\n", x, y, 0); */
    tp_last_staus = ELM_EVENT_TOUCH_UP;
    tp_down_cnt = 0;
    t.event = tp_last_staus;
    t.x = x;
    t.y = y;
    log_info("----tpd_up----");
    //不在这里发送up消息，在tke_stop里面!!!
    /* ui_touch_msg_post(&t); */

    tke_stop(x, y, &t);
}





/*****************************************************************************
* reset chip
*****************************************************************************/
static int fts_hw_reset(uint8_t msec)
{
    /*firsty. set reset_pin low, and then delay 10ms*/
    gpio_write(TCFG_TP_RESET_IO, 0);
    fts_msleep(10);

    /*secondly. set reset_pin high, and then delay 200ms*/
    gpio_write(TCFG_TP_RESET_IO, 1);
    fts_msleep(msec);
    return 0;
}

/*****************************************************************************
* Initialize i2c
*****************************************************************************/
static int platform_i2c_init(void)
{
    /*Initialize I2C bus, you should implement it based on your platform*/
    iic_init(fts_data->iic_hdl);
    return 0;
}

/*****************************************************************************
* Initialize reset pin
*****************************************************************************/
static int platform_reset_pin_cfg(void)
{
    /*Initialize reset_pin,  you should implement it based on your platform*/

    /*firstly,set the reset_pin to output mode*/
    /*secondly,set the reset_pin to low */
    if (TCFG_TP_RESET_IO == NO_CONFIG_PORT) {
        return -1;
    }
    gpio_set_hd0(TCFG_TP_RESET_IO, 1); //set_strengh 0x01
    gpio_set_direction(TCFG_TP_RESET_IO, 0); //set dir_output
    return 0;
}

/*****************************************************************************
* Initialize gpio interrupt, and set trigger mode to falling edge.
*****************************************************************************/
static int platform_interrupt_gpio_init(void)
{
    /*Initialize gpio interrupt , and the corresponding interrupt function is fts_gpio_interrupt_handler,
    you should implement it based on your platform*/


    /*firstly,set int_pin to input mode with pull-up*/
    /*secondly,and set int_pin's trigger mode to falling edge trigger */
    if (TCFG_TP_INT_IO == NO_CONFIG_PORT) {
        return -1;
    }
    if (fts_data->int_io_init == 0) {
        port_edge_wkup_set_callback_by_index(1,   fts_gpio_interrupt_handler); // 序号需要和板级配置中的wk_param对应上
        fts_data->int_io_init = 1;
    } else {
        power_wakeup_index_enable(1, true);//中断使能
    }
    gpio_set_pull_up(TCFG_TP_INT_IO, 1);
    gpio_set_die(TCFG_TP_INT_IO, 1);
    return 0;
}

/*****************************************************************************
* TP power on
*****************************************************************************/
static void fts_power_on(void)
{
    /*refer to ic datasheet*/
    if (TCFG_TP_POWER_IO == NO_CONFIG_PORT) {
        return ;
    }
    printf("fts power on\n");
    gpio_direction_output(TCFG_TP_POWER_IO, 1);
}

/*****************************************************************************
* Initialize timer and set to interrupt mode which period is 1 second
*****************************************************************************/
static int platform_interrupt_timer_init(void)
{

    /*Initialize timer and set to interrupt mode which period is 1 second,
    and the corresponding interrupt function is fts_timer_interrupt_handler,
    you should implement it based on your platform*/
    return 0;
}
/* ------------------------------------------------------------------------------------*/
//* @brief platform_i2c_write
/* ------------------------------------------------------------------------------------*/
u16 platform_i2c_write(u8 *buf, u16 len)
{
    u16 i = 0;
    iic_start(fts_data->iic_hdl);
    if (0 == iic_tx_byte(fts_data->iic_hdl, FTS_DEV_SLAVE_ADDR)) {
        iic_stop(fts_data->iic_hdl);
        FTS_ERROR("%s tx slave_addr error\n", __func__);
        return -1;
    }

    for (i = 0; i < len; i++) {
        delay(fts_data ->iic_delay);
        if (0 == iic_tx_byte(fts_data->iic_hdl, buf[i])) {
            iic_stop(fts_data->iic_hdl);
            FTS_ERROR("%s tx buf error\n", __func__);
            return -2;
        }
    }
    iic_stop(fts_data->iic_hdl);
    return i;
}
/* ------------------------------------------------------------------------------------*/
//* @brief platform_i2c_read
/* ------------------------------------------------------------------------------------*/
u16 platform_i2c_read(u8 *buf, u16 len)
{
    u16 i = 0;
    iic_start(fts_data->iic_hdl);
    if (0 == iic_tx_byte(fts_data->iic_hdl,  FTS_DEV_SLAVE_ADDR + 1)) {
        iic_stop(fts_data->iic_hdl);
        FTS_ERROR("%s tx slave_addr error\n", __func__);
        return -1;
    }
    for (i = 0; i < len - 1; i++) {
        delay(fts_data->iic_delay);
        *buf++ = iic_rx_byte(fts_data->iic_hdl, 1);
    }
    i++;
    *buf = iic_rx_byte(fts_data->iic_hdl, 0);
    iic_stop(fts_data->iic_hdl);

    return i;
}
/*****************************************************************************
* Name: fts_write
* Brief:
*   Write function via I2C bus, you should implement it based on your platform.
*
*   The code below is only a sample code, a pseudo code. you must implement it
*  following standard I2C protocol based on your platform
*
*
* Input: @addr: the command or register address
*        @data: the data buffer, data buffer can be NULL for commands without data fields.
*        @datalen: length of data buffer
* Output:
* Return:
*   return 0 if success, otherwise error code.
*****************************************************************************/
int fts_write(uint8_t addr, uint8_t *data, uint16_t datalen)
{
    /*TODO based your platform*/
    int ret = 0;
    uint8_t txbuf[256] = { 0 };
    uint16_t txlen = 0;
    int i = 0;

    if (datalen >= 256) {
        FTS_ERROR("txlen(%d) fails", datalen);
        return -1;
    }

    memcpy(&txbuf[0], &addr, 1);
    txlen = 1;
    if (data && datalen) {
        memcpy(&txbuf[txlen], data, datalen);
        txlen += datalen;
    }

    /*call platform_i2c_write function to transfer I2C package to TP controller
     *platform_i2c_write() is different for different platform, based on your platform.
     */
    for (i = 0; i < 3; i++) {
        ret = platform_i2c_write(txbuf, txlen);
        if (ret < 0) {
            FTS_ERROR("platform_i2c_write(%d) fails,ret:%d,retry:%d", addr, ret, i);
            continue;
        } else {
            ret = 0;
            break;
        }
    }

    return ret;
}

/*****************************************************************************
* Name: fts_read
* Brief:
*   Read function via I2C bus, you should implement it based on your platform.
*
*   The code below is only a sample code, a pseudo code. you must implement it
*  following standard I2C protocol based on your platform
*
*
* Input: @addr: the command or register data
*        @datalen: length of data buffer
* Output:
*        @data: the data buffer read from TP controller
* Return:
*   return 0 if success, otherwise error code.
*****************************************************************************/
int fts_read(uint8_t addr, uint8_t *data, uint16_t datalen)
{
    /*TODO based your platform*/
    int ret = 0;
    int i = 0;

    if (!data || !datalen) {
        FTS_ERROR("data is null, or datalen is 0");
        return -1;
    }

    for (i = 0; i < 3; i++) {
        /*call platform_i2c_write function to transfer I2C package to TP controller
         *platform_i2c_write() is different for different platform, based on your platform.
         */
        ret = platform_i2c_write(&addr, 1);
        if (ret < 0) {
            FTS_ERROR("platform_i2c_write(%d) fails,ret:%d,retry:%d", addr, ret, i);
            continue;
        }


        /*call platform_i2c_read function to transfer I2C package to read data from TP controller
         *platform_i2c_read() is different for different platform, based on your platform.
         */
        ret = platform_i2c_read(data, datalen);
        if (ret < 0) {
            FTS_ERROR("platform_i2c_read fails,ret:%d,retry:%d", addr, ret, i);
            continue;
        }

        ret = 0;
        break;
    }

    return ret;
}

/*****************************************************************************
* Name: fts_write_reg
* Brief:
*   write function via I2C bus, you should implement it based on your platform.
*
*   The code below is only a sample code, a pseudo code. you must implement it
*  following standard I2C protocol based on your platform
*
*
* Input: @addr: the command or register address
*        @val: the data write to TP controller
* Return:
*   return 0 if success, otherwise error code.
*****************************************************************************/
int fts_write_reg(uint8_t addr, uint8_t val)
{
    return fts_write(addr, &val, 1);
}

/*****************************************************************************
* Name: fts_read_reg
* Brief:
*   read function via I2C bus, you should implement it based on your platform.
*
*   The code below is only a sample code, a pseudo code. you must implement it
*  following standard I2C protocol based on your platform
*
*
* Input: @addr: the command or register address
* Output:
*        @val: the data read from TP controller
* Return:
*   return 0 if success, otherwise error code.
*****************************************************************************/
int fts_read_reg(uint8_t addr, uint8_t *val)
{
    return fts_read(addr, val, 1);
}

/*****************************************************************************
* Name: fts_check_id
* Brief:
*   The function is used to check id.
* Input:
* Output:
* Return:
*   return 0 if check id successfully, otherwise error code.
*****************************************************************************/
static int fts_check_id(void)
{
    int ret = 0;

    uint8_t chip_id[2] = { 0 };


    /*delay 200ms,wait fw*/
    fts_msleep(200);

    /*get chip id*/
    fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
    fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
    if ((FTS_CHIP_IDH == chip_id[0]) && (FTS_CHIP_IDL == chip_id[1])) {
        FTS_INFO("get ic information, chip id = 0x%02x%02x",  chip_id[0], chip_id[1]);
        return 0;
    }

    /*get boot id*/
    FTS_INFO("fw is invalid, need read boot id");
    ret = fts_write_reg(0x55, 0xAA);
    if (ret < 0) {
        FTS_ERROR("start cmd write fail");
        return ret;
    }

    fts_msleep(FTS_CMD_START_DELAY);
    ret = fts_read(FTS_CMD_READ_ID, chip_id, 2);
    if ((ret == 0) && ((FTS_CHIP_IDH == chip_id[0]) && (FTS_CHIP_IDL == chip_id[1]))) {
        FTS_INFO("get ic information, boot id = 0x%02x%02x", chip_id[0], chip_id[1]);
        ret = 0;
    } else {
        FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0], chip_id[1]);
        return -1;
    }
    extern void fts_fwupg_auto_upgrade(void);
    fts_fwupg_auto_upgrade();
    return ret;
}

/*****************************************************************************
*  Name: fts_esdcheck_algorithm
*  Brief:
*    The function is use to esd check.It should be called in timer interrupt handler.
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_esdcheck_process(void)
{
    uint8_t reg_value = 0;
    static uint8_t flow_work_hold_cnt = 0;
    static uint8_t flow_work_cnt_last = 0;

    /* 1. check power state, if suspend, no need check esd */
    if (fts_data->suspended == 1) {
        FTS_DEBUG("In suspend, not check esd");
        /* because in suspend state, when upgrade FW, will
         * active ESD check(active = 1); when resume, don't check ESD again
         */
        return ;
    }

    /* 2. In factory mode, can't check esd */
    fts_read_reg(FTS_REG_WORKMODE, &reg_value);
    if ((reg_value == FTS_REG_WORKMODE_FACTORY_VALUE) || (reg_value == FTS_REG_WORKMODE_SCAN_VALUE)) {
        FTS_DEBUG("in factory mode(%x), no check esd", reg_value);
        return ;
    }

    /* 3. get Flow work cnt: 0x91 If no change for 5 times, then ESD and reset */
    fts_read_reg(FTS_REG_FLOW_WORK_CNT, &reg_value);
    if (flow_work_cnt_last == reg_value) {
        flow_work_hold_cnt++;
    } else {
        flow_work_hold_cnt = 0;
    }

    flow_work_cnt_last = reg_value;

    /* 4. If need hardware reset, then handle it here */
    if (flow_work_hold_cnt >= 5) {
        FTS_DEBUG("ESD, Hardware Reset");
        flow_work_hold_cnt = 0;
        fts_hw_reset(200);
    }
}


/*****************************************************************************
* Name: fts_gesture_process
* Brief:
*   The function is used to read and parse gesture information. It should be
*  called in gpio interrupt handler while system is in suspend state.
* Input:
* Output:
* Return:
*   return 0 if getting and parsing gesture successfully,
*   return 1 if gesture isn't enabled in FW,
*   otherwise error code.
*****************************************************************************/
static int fts_gesture_process(void)
{
    int ret = 0;
    uint8_t i = 0;
    uint8_t base = 0;
    uint8_t regaddr = 0;
    uint8_t value = 0xFF;
    uint8_t buf[MAX_LEN_GESTURE_INFO] = { 0 };
    uint8_t gesture_id = 0;

    /*Read a byte from register 0xD0 to confirm gesture function in FW is enabled*/
    ret = fts_read_reg(FTS_REG_GESTURE_EN, &value);
    if ((ret < 0) || (value != FTS_REG_GESTURE_ENABLE)) {
        FTS_DEBUG("gesture isn't enable in fw, don't process gesture");
        return 1;
    }

    /*Read 26 bytes from register 0xD3 to get gesture information*/
    regaddr = FTS_REG_GESTURE_OUTPUT_ADDRESS;
    ret = fts_read(regaddr, buf, MAX_LEN_GESTURE_INFO);
    if (ret < 0) {
        FTS_DEBUG("read gesture information from reg0xD3 fails");
        return ret;
    }

    /*get gesture_id, and the gestrue_id table provided by our technicians */
    gesture_id = buf[0];

    /* Now you have parsed the gesture information, you can recognise the gesture type based on gesture id.
     * You can do anything you want to do, for example,
     *     gesture id 0x24, so the gesture type id "Double Tap", now you can inform system to wake up
     *     from gesture mode.
     */

    /*TODO...(report gesture to system)*/

    return 0;
}


/*****************************************************************************
* print touch buffer read from register address 0x01
*****************************************************************************/
static void log_touch_buf(uint8_t *buf, uint32_t buflen)
{
    int i = 0;
    int count = 0;
    char tmpbuf[512] = { 0 };

    for (i = 0; i < buflen; i++) {
        count += snprintf(tmpbuf + count, 1024 - count, "%02X,", buf[i]);
        if (count >= 1024) {
            break;
        }
    }
    FTS_DEBUG("point buffer:%s", tmpbuf);
}


/*****************************************************************************
* print touch points information of this interrupt
*****************************************************************************/
static void log_touch_info(struct fts_ts_event *events, uint8_t event_nums)
{
    uint8_t i = 0;

    for (i = 0; i < event_nums; i++) {
        FTS_DEBUG("[%d][%d][%d,%d,%d]%d", events[i].id, events[i].type, events[i].x,
                  events[i].y, events[i].p, events[i].area);
    }
}

/*****************************************************************************
* Name: fts_touch_process
* Brief:
*   The function is used to read and parse touch points information. It should be
*  called in gpio interrupt handler while system is in display(normal) state.
* Input:
* Output:
* Return:
*   return 0 if getting and parsing touch points successfully, otherwise error code.
*****************************************************************************/
static int fts_touch_process(void)
{
    int ret = 0;
    uint8_t i = 0;
    uint8_t base = 0;
    uint8_t regaddr = 0x01;
    uint8_t buf[MAX_LEN_TOUCH_INFO];/*A maximum of two points are supported*/
    uint8_t point_num = 0;
    uint8_t touch_event_nums = 0;
    uint8_t point_id = 0;
    struct fts_ts_event events[FTS_MAX_POINTS_SUPPORT];    /* multi-touch */


    /*read touch information from reg0x01*/
    ret = fts_read(regaddr, buf, MAX_LEN_TOUCH_INFO);
    if (ret < 0) {
        FTS_DEBUG("Read touch information from reg0x01 fails");
        return ret;
    }

    /*print touch buffer, for debug usage*/
    log_touch_buf(buf, MAX_LEN_TOUCH_INFO);

    if ((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF)) {
        FTS_INFO("FW initialization, need recovery");
        if (fts_data->gesture_support && fts_data->suspended) {
            fts_write_reg(FTS_REG_GESTURE_EN, FTS_REG_GESTURE_ENABLE);
        }
    }

    /*parse touch information based on register map*/
    memset(events, 0xFF, sizeof(struct fts_ts_event) * FTS_MAX_POINTS_SUPPORT);
    point_num = buf[1] & 0x0F;
    if (point_num > FTS_MAX_POINTS_SUPPORT) {
        FTS_DEBUG("invalid point_num(%d)", point_num);
        return -1;
    }

    for (i = 0; i < MAX_POINTS_TOUCH_TRACE; i++) {
        base = 2 + i * 6;
        point_id = buf[base + 2] >> 4;
        if (point_id >= MAX_POINTS_TOUCH_TRACE) {
            break;
        }

        events[i].x = ((buf[base] & 0x0F) << 8) + buf[base + 1];
        events[i].y = ((buf[base + 2] & 0x0F) << 8) + buf[base + 3];
        events[i].id = point_id;
        events[i].type = (buf[base] >> 6) & 0x03;
        events[i].p = buf[base + 4];
        events[i].area = buf[base + 5];
        if (((events[i].type == 0) || (events[i].type == 2)) & (point_num == 0)) {
            FTS_DEBUG("abnormal touch data from fw");
            return -2;
        }

        touch_event_nums++;
    }

    if (touch_event_nums == 0) {
        FTS_DEBUG("no touch point information(%02x)", buf[1]);
        return -3;
    }

    /*print touch information*/
    log_touch_info(events, touch_event_nums);

    /*Now you have get the touch information, you can report anything(X/Y coordinates...) you want to system*/
    /*TODO...(report touch information to system)*/
    /*Below sample code is a pseudo code*/
    for (i = 0; i < touch_event_nums; i++) {
        if ((events[i].type == 0) || (events[i].type == 2)) {
            /* The event of point(point id is events[i].id) is down event, the finger of this id stands for is
             * pressing on the screen.*/
            /*TODO...(down event)*/
#if SW_X_Y
            tpd_down(events[i].y, events[i].x, events[i].y, events[i].x, 0);
#else
            tpd_down(events[i].x, events[i].y, events[i].x, events[i].y, 0);
#endif
        } else {
            /*TODO...(up event)*/
#if SW_X_Y
            tpd_up(events[i].y, events[i].x, events[i].y, events[i].x, 0);
#else
            tpd_up(events[i].x, events[i].y, events[i].x, events[i].y, 0);
#endif
        }
    }

    return 0;
}


/*****************************************************************************
* An interrupt handler, will be called while the voltage of INT Port changes from high to low(falling-edge trigger mode).
* The program reads touch data or gesture data from TP controller, and then sends it into system.
*****************************************************************************/
static int fts_gpio_interrupt_handler(void)
{
    int ret = 0;

    if (fts_data->gesture_support && fts_data->suspended) {
        /*if gesture is enabled, interrupt handler should process gesture at first*/
        ret = fts_gesture_process();
        if (ret == 0) {
            FTS_DEBUG("success to process gesture.");
            return 1;
        }
    }

    /*if gesture isn't enabled, the handler should process touch points*/
    fts_touch_process();

    return 0;
}


/*****************************************************************************
* An interrupt handler, will be called while the timer interrupt trigger , the code based on your platform.
* The program use to check esd.
*****************************************************************************/
static void fts_timer_interrupt_handler(void)
{
    /* esd check */
    fts_esdcheck_process();

}

/*****************************************************************************
*  Name: fts_suspend
*  Brief: System suspends and update the suspended state
*  Input:
*  Output:
*  Return:
*     return 0 if enter suspend successfully, otherwise error code
*****************************************************************************/
int fts_ts_suspend(void)
{
    int ret = 0;

    if (fts_data->suspended) {
        FTS_INFO("Already in suspend state");
        return 0;
    }

    if (fts_data->gesture_support) {
        /*Host writes 0x01 to register address 0xD0 to enable gesture function while system suspends.*/
        ret = fts_write_reg(FTS_REG_GESTURE_EN, FTS_REG_GESTURE_ENABLE);
        if (ret < 0) {
            FTS_ERROR("enable gesture fails.ret:%d", ret);
        } else {
            FTS_INFO("enable gesture success.");
        }
    } else {
        /*Host writes 0x03 to register address 0xA5 to enter into sleep mode.*/
        ret = fts_write_reg(FTS_REG_POWER_MODE, 0x03);
        if (ret < 0) {
            FTS_ERROR("system enter sleep mode fails.ret:%d", ret);
        } else {
            FTS_INFO("system enter sleep mode success.");
        }
    }

    fts_data->suspended = 1;
    return 0;
}

/*****************************************************************************
*  Name: fts_resume
*  Brief: System resume and update the suspended state
*  Input:
*  Output:
*  Return:
*    return 0 if enter resume successfully, otherwise error code
*****************************************************************************/
int fts_ts_resume(void)
{
    if (!fts_data->suspended) {
        FTS_INFO("Already in resume state");
        return 0;
    }

    fts_data->suspended = 0;
    fts_hw_reset(200);
    touch_info_reset();
    return 0;
}

/*****************************************************************************
* Name: fts_init
* Brief:
*   The function is used to i2c init��tp_rst pin init��interrupt_pin init��timer init.
* Input:
* Output:
* Return:
*   return 0 if success, otherwise error code.
*****************************************************************************/
extern u32 soft_iic_real_delay[];
int fts_ts_init(void)
{
    int ret = 0;
    printf("%s %d", __func__, __LINE__);
    /*Initialize I2C*/
    soft_iic_real_delay[0] = 10;
    ret = platform_i2c_init();
    if (ret < 0) {
        FTS_ERROR("I2C init fails.ret:%d", ret);
        return ret;
    }

    /*reset pin cfg*/
    ret = platform_reset_pin_cfg();
    if (ret < 0) {
        FTS_ERROR("reset pin init fails.ret:%d", ret);
        return ret;
    }

    /*tp power on*/
    fts_power_on();

    /*tp_reset*/
    fts_hw_reset(0);
    touch_info_reset();
    /*check chip id*/
    ret = fts_check_id();
    if (ret < 0) {
        FTS_ERROR("get chip id fails.ret:%d", ret);
        return ret;
    }

    /*Register gpio interrupt handler,which for touch process or gestrue process*/
    ret = platform_interrupt_gpio_init();
    if (ret < 0) {
        FTS_ERROR("Register gpio interrupt handler fails.ret:%d", ret);
        return ret;
    }

    /*Initialize timer and set to interrupt mode with one second one period, which for esd check*/
    ret = platform_interrupt_timer_init();
    if (ret < 0) {
        FTS_ERROR("Initialize timer fails.ret:%d", ret);
        return ret;
    }
    soft_iic_real_delay[0] = 75;
    printf("%s %d", __func__, __LINE__);
    return ret;
}


#endif
