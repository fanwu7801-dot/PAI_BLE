#include "app_config.h"
#include "system/timer.h"
#include "key_event_deal.h"


#include "ui/ui.h"
#include "ui/ui_api.h"
#include "ui/ui_style.h"
#include "ui/ui_sys_param.h"

#if TCFG_UI_ENABLE && (!TCFG_LUA_ENABLE)
#ifdef CONFIG_UI_STYLE_JL_ENABLE
#if TCFG_UI_ENABLE_MENU_STAR_NEW
#define STYLE_NAME  JL
/*##############################################################
                //蜂窝菜单支持功能
* 2023年8月15日
* 六边形坐标
* 圆屏、圆角矩形屏
* 2.5D球面投影
* 图片切换索引
* 中心表盘
* 优化缩放比例
* 删改demo
* 支持不使用math_fast_function
* 算法独立成api
* 2023年8月16日
* 修复方屏在四角移动时存在缩放跳变问题
* 2023年8月24日
* 满天星增加惯性
* 2023年10月18日
* 增加三种缩放模式配置
##############################################################*/
#define STAR_RATIO_MODE_SPHERE_FUNC 1
#define STAR_RATIO_MODE_LINEAR_FUNC 2
#define STAR_RATIO_MODE_EXP_FUNC    3

/*##############################################################
                //蜂窝菜单效果配置区
* 调参说明：
    布局:布局宽高需改为屏幕宽高
    图片:由代码计算坐标;
        计算规则:以屏幕中心为起点，x轴正方向为起始方向，逐圈，按顺时针依次排布
    方屏:方屏提供圆角参数，根据屏幕实际数据填入
    球面:以SPHERE_R为半径的球面在xoy平面做投影，得到图标中心和半径；半径越小，球面感越好
* 如需添加其他内容显示，请新建空白背景的布局处理
##############################################################*/
#define STAR_LAYOUT             MOVING_1    //满天星布局
//功能选择
#define AUTO_SEL_INDEX          0               //自动切换图片索引
#define ICON_DEL_EN             0               //删除图标功能
#define RATIO_EN                1               //缩放使能，默认打开
#define MODE_2_5                1               //球面效果
#define SQUARE_SCREEN           0               //方屏使能
#define CENTER_WATCH_EN         0               //中心表盘
#define ICON_DEBUG              0               //开发板调试方屏时使用,在圆屏上模拟方屏区域
#define USER_MATH_FAST			1				//浮点加速
//参数配置
#define S_LCD_WIDTH             get_lcd_width_from_imd()
#define S_LCD_HEIGHT            get_lcd_height_from_imd()
#define ICON_WIDTH_HEIGHT_MAX   90              //控件宽高
#define ICON_INTERVAL_MAX       25              //控件间隔
#define ICON_INDEX_INIT         0               //初始化图标尺寸
#define ROUNDED_RECTANGLE_R     100              //方屏圆角矩形半径，影响图标到四角的变化，圆屏无效

#if SQUARE_SCREEN
#define STAR_ROTATE_MODE        (STAR_RATIO_MODE_LINEAR_FUNC)  //缩放曲线

#define	SPHERE_R				300             //RATIO_MODE_SPHERE_FUNC时，表示球体半径,需大于屏幕对角线半径
#define LINEAR_FUNC_A           630             //RATIO_MODE_LINEAR_FUNC参数

#define SPHERE_DIST_A           50              //球体图标间距系数,越小图标越发散，过大会造成图标向中心重叠
#define SPHERE_DIST_B			20				//越大图标越发散
#else
#define STAR_ROTATE_MODE        (STAR_RATIO_MODE_SPHERE_FUNC)  //缩放曲线
#define	SPHERE_R				300             //球体半径,需大于屏幕对角线半径
#define LINEAR_FUNC_A           630             //RATIO_MODE_LINEAR_FUNC参数
#define SPHERE_DIST_A           30              //球体图标间距系数,越小图标越发散，过大会造成图标向中心重叠
#define SPHERE_DIST_B			0				//越大图标越发散
#endif//SQUARE_SCREEN

#define MOVE_CENTER_STEP        25              //居中单次滑动距离
#define MOVE_TIME_INTERVAL      (40)            //居中定时器
#define ENERGY_A                (0.07f)         //惯性加速度
#define ENERGY_MAX_DIST         S_LCD_WIDTH     //惯性最大距离，用于防止图标完全移出屏幕
#define ENERGY_TIME_INTERVAL    (40)            //惯性定时器，参考值30-50

#define ANIMA_MAX               (3.0f)          //
#define ANIMA_STEP              (0.1f)          //

#if CENTER_WATCH_EN
#define STAR_WATCH_LAYOUT       STAR_WATCH      //中心表盘布局
#define CENTER_WATCH_ICON_ID    S_0             //中心表盘放置在哪个图标上
#define CENTER_WATCH_HH_X       5               //指针旋转中心x
#define CENTER_WATCH_HH_Y       25              //指针旋转中心y
#define CENTER_WATCH_MM_X       5               //指针旋转中心x
#define CENTER_WATCH_MM_Y       38              //指针旋转中心y
#define CENTER_WATCH_SS_X       5               //指针旋转中心x
#define CENTER_WATCH_SS_Y       39              //指针旋转中心y
#define CENTER_SOURCE_HH        "starhh"        //时针数据源
#define CENTER_SOURCE_MM        "starmm"        //分针数据源
#define CENTER_SOURCE_SS        "starss"        //秒针数据源
#endif //CENTER_WATCH_EN



#if (RATIO_EN)
static float icon_ratio_table[] = { 1.0, 0.9, 0.8, 0.7, 0.6, 0.5};//编码器缩放等级
#else
static float icon_ratio_table[] = { 1.0 };
#endif

/*##############################################################
                //工具函数
##############################################################*/
#if (USER_MATH_FAST)
#include "asm/math_fast_function.h"
#else
#define PI              (3.14159f)
#define complex_abs_float(x,y)  (sqrt((float)((float)x)*((float)x)+((float)y)*((float)y)))
#define complex_dqdt_float(x,y) (sqrt((float)((float)x)*((float)x)-((float)y)*((float)y)))
#define root_float(x)           (sqrt((float)x))
#define sin_float(x)            (sin(PI*(float)x))
#define cos_float(x)            (cos(PI*(float)x))
#endif//USER_MATH_FAST
#define CSS(x,X)        ((x)*10000/(X))             //绝对坐标转相对坐标
#define CSS2ABS(x,X)    ((X)*(x)/10000)             //相对坐标转绝对坐标
#define C2LT(X,WH)      ((X)-(WH/2))                //图标中心转左上角坐标
#define LT2C(X,WH)      ((X)+(WH/2))                //图标左上角转中心坐标
#define ABS(x)          ((x)>0?(x):(-(x)))          //绝对值
#define DIR(x)          ((x)>0?(1):(-1))            //方向
#define MIN(a,b)        ((a<b)?(a):(b))             //最小值
//圆角矩形方位
enum {
    ROUNDED_RECT_NULL,
    ROUNDED_RECT_LEFT_TOP,
    ROUNDED_RECT_RIGHT_TOP,
    ROUNDED_RECT_LEFT_BOTTOM,
    ROUNDED_RECT_RIGHT_BOTTOM,
};
enum {
    STAR_TOUCH_FLAG_MOVE,
    STAR_TOUCH_FLAG_HOLD,
    STAR_TOUCH_FLAG_ENERGY,
};
#if ICON_DEBUG
#define SCREEN_LEFT         67
#define SCREEN_TOP          35
#define SCREEN_RIGHT        387
#define SCREEN_BOTTOM       419
#else
#define SCREEN_LEFT         0
#define SCREEN_TOP          0
#define SCREEN_RIGHT        S_LCD_WIDTH
#define SCREEN_BOTTOM       S_LCD_HEIGHT
#endif


static int icon_postion(struct layout *__layout, u8 ratio_index, int x_offset, int y_offset, int bound_en);
#if CENTER_WATCH_EN
void usr_watch_enable(void);
void usr_watch_disable(void);
void usr_watch_run(void *p);
#endif
/*##############################################################
                //蜂窝菜单效果信息
##############################################################*/
static struct icon_info { 				//图标信息
    struct position icon_center;        //中心坐标
    struct position offset;             //偏移坐标，居中时使用
    u16 timer_id;                       //居中滑动定时器
    u8 icon_ratio_index;                //缩放等级
    u16 center_icon_index;              //中心坐标索引值
#if CENTER_WATCH_EN
    u16 watch_run_id;                   //中心表盘
#endif//CENTER_WATCH_EN
#if ICON_DEL_EN
    u32 icon_hide_ctrl0;                //隐藏标志
    u32 icon_hide_ctrl1;                //隐藏标志
    //……超过60个图标参考上面的流程拓展
#endif//ICON_DEL_EN
    u16 energy_tid;					//惯性定时器
    struct position energy_offset;
    u8 anima_enable;                    //放大动画
    float anima_ratio;                  //放大系数

};
static struct icon_info __icon_info;
#define __this 		(&__icon_info)

struct star_info {
    u8 index;           //索引
    u8 icon_level: 7;   //圈层
    u8 invisible: 1;    //是否隐藏
    int ic_x;           //图标中心点坐标
    int ic_y;           //图标中心点坐标
    int width;          //图标缩放后的宽
    int height;         //图标缩放后的高
    float ratio;        //缩放系数
};

/******************************************************************
                获取触点图标真实索引
	return -1 空白
			other index
******************************************************************/
static int icon_get_touch_icon_index(struct element *_ctrl, struct position *pos)
{
    struct element *elm, *p, *n;
    struct rect r;
    int index = 0;
    elm = _ctrl;
    list_for_each_child_element_safe(p, n, elm) {
        ui_core_get_element_abs_rect(p, &r);
        if (in_rect(&r, pos)) {
            return index;
        }
        index++;
    }
    return -1;
}
/******************************************************************
                通过索引值控制显示与否
******************************************************************/
static int icon_del_by_index(struct element *elm, s8 index, u8 del_en, u8 redraw)
{
#if ICON_DEL_EN
    if (index < 0) {
        return -1;
    } else if (index < 32) {
        if (del_en) {
            __this->icon_hide_ctrl0 |= BIT(index);
        } else {
            __this->icon_hide_ctrl0 &= ~BIT(index);
        }

    } else if (index < 64) {
        if (del_en) {
            __this->icon_hide_ctrl1 |= BIT(index - 32);
        } else {
            __this->icon_hide_ctrl1 &= ~BIT(index - 32);
        }
    } else {
        ASSERT(0);
    }
    if (redraw && elm) {
        icon_postion(elm, __this->icon_ratio_index, 0, 0, 1);
    }
    printf("hide_ctrl:0x%x 0x%x %d\n", __this->icon_hide_ctrl0, __this->icon_hide_ctrl1, index);
    return 0;
#else
    return 0;
#endif

}
/******************************************************************
                获取中心图标id
    return:>0 id
    return:≤0 err
******************************************************************/
static int icon_get_center_icon_id(struct element *_ctrl)
{
    struct element *elm, *p, *n;
    struct rect r;
    elm = _ctrl;
    list_for_each_child_element_safe(p, n, elm) {
        if ((p->css.left < 5000) && ((p->css.left + p->css.width) > 5000) && \
            (p->css.top < 5000) && ((p->css.top + p->css.height) > 5000)) {
            return p->id;
        }
    }
    return 0;
}
/******************************************************************
                获取最接近中心图标id
    return: id
******************************************************************/
static int icon_get_near_center_icon_id(struct element *_ctrl)
{
    int y, x;
    int r_mem = 0;
    u32 id_mem = -1;
    struct element *elm, *p, *n;
    elm = _ctrl;

    //查找最接近圆心的坐标，作为偏移目标
    list_for_each_child_element_safe(p, n, elm) {
        y = (p->css.top + p->css.height / 2) - 5000;
        x = (p->css.left + p->css.width / 2) - 5000;
        int r2 = y * y + x * x;
        if ((r2 <= r_mem) || (!r_mem)) {
            r_mem = r2;
            id_mem = p->id;
        }
    }
    return id_mem;
}
/******************************************************************
                获取触点图标id
    return:>0 id
    return:≤0 err
******************************************************************/
static int icon_get_touch_icon_id(struct element *_ctrl, struct position *pos)
{
    struct element *elm, *p, *n;
    struct rect r;
    elm = _ctrl;
    list_for_each_child_element_safe(p, n, elm) {
        ui_core_get_element_abs_rect(p, &r);
        if (in_rect(&r, pos)) {
            return p->id;
        }
    }
    return -1;
}

/******************************************************************
                回弹后回调处理
******************************************************************/
static int icon_move_center_post(void *_ctrl, int redraw)
{
#if CENTER_WATCH_EN
    u32 center_icon_id = icon_move_get_center_icon_id(_ctrl);
    if (!center_icon_id) {
        return 0;
    }
    if (center_icon_id == CENTER_WATCH_ICON_ID) {
        usr_watch_enable();
        if (__this->watch_run_id) {
            sys_timer_del(__this->watch_run_id);
            __this->watch_run_id = 0;
        }
        __this->watch_run_id = sys_timer_add(NULL, usr_watch_run, 166);
    }
#endif
    return 0;
}
/******************************************************************
                回弹后回调的逆处理
                比如打断执行等
******************************************************************/
static int icon_move_center_restore(void *_ctrl)
{
#if CENTER_WATCH_EN
    if (__this->watch_run_id) {
        sys_timer_del(__this->watch_run_id);
        __this->watch_run_id = 0;
    }
    usr_watch_disable();
#endif
    return 0;
}
/******************************************************************
                居中回弹定时器回调
******************************************************************/
static int icon_move_center_do(void *_ctrl)
{
    int x_offset, y_offset;
    //计算单次滑动距离
    int offset_r = complex_abs_float(__this->offset.x, __this->offset.y);
    int move_center_step_x = ABS(__this->offset.x) * MOVE_CENTER_STEP / offset_r;
    int move_center_step_y = ABS(__this->offset.y) * MOVE_CENTER_STEP / offset_r;
    if (ABS(__this->offset.x) > move_center_step_x) {
        x_offset = DIR(__this->offset.x) * move_center_step_x;
        __this->offset.x -= DIR(__this->offset.x) * move_center_step_x;
    } else {
        x_offset = __this->offset.x;
        __this->offset.x = 0;
    }
    if (ABS(__this->offset.y) > move_center_step_y) {
        y_offset = DIR(__this->offset.y) * move_center_step_y;
        __this->offset.y -= DIR(__this->offset.y) * move_center_step_y;
    } else {
        y_offset = __this->offset.y;
        __this->offset.y = 0;
    }
    //滑动居中
    icon_postion(_ctrl, __this->icon_ratio_index, x_offset, y_offset, 1);
    //若已居中，关定时器
    if ((__this->timer_id) && (!__this->offset.y) && (!__this->offset.x)) {
        sys_timer_del(__this->timer_id);
        __this->timer_id = 0;
        icon_move_center_post(_ctrl, 1);
    }

    return true;
}
/******************************************************************
                居中回弹
******************************************************************/
static int icon_move_center(void *_ctrl)
{
    int y, x;
    int r_mem = 0;
    struct element *elm, *p, *n;
    elm = _ctrl;
    //查找最接近圆心的坐标，作为偏移目标
    list_for_each_child_element_safe(p, n, elm) {
        y = (p->css.top + p->css.height / 2) - 5000;
        x = (p->css.left + p->css.width / 2) - 5000;
        int r2 = y * y + x * x;
        if ((r2 <= r_mem) || (!r_mem)) {
            r_mem = r2;
            __this->offset.y = CSS2ABS(-1 * y, get_lcd_height_from_imd());
            __this->offset.x = CSS2ABS(-1 * x, get_lcd_width_from_imd());
        }
    }
    //注册定时器执行居中
    if (!__this->timer_id) {
        __this->timer_id = sys_timer_add(_ctrl, icon_move_center_do, MOVE_TIME_INTERVAL);
    }
    return true;
}
/******************************************************************
                缩放回弹效果
******************************************************************/
static int icon_ratio_timer_do(struct layout *__layout)
{
    __this->icon_center.x = get_lcd_width_from_imd() / 2;
    __this->icon_center.y = get_lcd_height_from_imd() / 2;
    __this->icon_ratio_index -- ;
    icon_postion(__layout, __this->icon_ratio_index, 0, 0, 1);
    if (__this->icon_ratio_index == 0) {
        if (__this->timer_id) {
            sys_timer_del(__this->timer_id);
            __this->timer_id = 0;
            return false;
        }
    }
    return true;
}
/******************************************************************
                满天星算法
* __info : 图标信息
* total_ratio ：整体缩放系数，用于编码器调节
* center_x 中心图标位置
* center_y 中心图标位置
* icon_index 图标序号
* bound_en 边框约束
******************************************************************/
static int star_icon_info_do(struct star_info *__info, float total_ratio, int center_x, int center_y, int icon_index, int bound_en)
{
    //获取屏幕尺寸信息
    int lcd_width = S_LCD_WIDTH;
    int lcd_height = S_LCD_HEIGHT;
    int lcd_width_half = lcd_width / 2;
    int lcd_height_half = lcd_height / 2;
    //获取图标信息
    int icon_width_height = ICON_WIDTH_HEIGHT_MAX;
    int icon_distance = icon_width_height + ICON_INTERVAL_MAX;
    int icon_distance_r = icon_distance * total_ratio;
    int icon_distance_r_half = icon_distance_r / 2;

    float x_abs, y_abs;                         //位置参数abs
    double ratio_val = 1.0f;                           //缩放系数
    float triangle_a, triangle_b, triangle_c;   //当前图标、圈层首图标、屏幕中心维成三角形的三边长
    float angle_offset;                         //偏移角度 按60度差偏移得到全屏坐标
    float sin_angle, cos_angle;                 //与水平轴夹角正余弦值（0-60°）
    float sin_offset, cos_offset;               //与水平轴夹角正余弦值（0-360°）
    float dist_x, dist_y, dist_c;               //图标中心到屏幕中心的水平、垂直和径向距离
    float dist_c_big, dist_c_small;             //图标所在圆与屏幕中心的最大和最小距离
    int icon_abs_left, icon_abs_right, icon_abs_top, icon_abs_bottom;//图标边界与中心轴的距离
    int icon_level;                             //图标圈层
    int real_index = 0;
    int screen_lock = 0;          //2.5D投影时，隐藏z负轴球面图标
    int invisible = 0;
    //计算图标位于第几圈
    if (icon_index <= 0) {
        icon_level = 0;
    } else if (icon_index <= 6) {
        icon_level = 1;
    } else if (icon_index <= 18) {
        icon_level = 2;
    } else if (icon_index <= 36) {
        icon_level = 3;
    } else if (icon_index <= 60) {
        icon_level = 4;
    } else {
        ASSERT(icon_index <= 60); //上限60个，需要再拓展
    }
    if (icon_level) {
        //围成三角形的三边长
        triangle_a = icon_level * icon_distance;
        triangle_b = ((icon_index - 1) % icon_level) * icon_distance;
        triangle_c = (!triangle_b) ? triangle_a : \
                     (root_float(triangle_a * triangle_a + triangle_b * triangle_b - triangle_a * triangle_b)); //cos60°，简化余弦
        //根据三边关系计算0-60度内图标的三角函数值
        cos_angle = (float)(triangle_a * triangle_a + triangle_c * triangle_c - triangle_b * triangle_b) / (2 * triangle_a * triangle_c); //求水平夹角余弦值
        sin_angle =  root_float((1 - cos_angle * cos_angle)); //求水平夹角正弦值
        //从x正半轴开始顺时针旋转，得到对应角度三角函数值
        angle_offset = (float)((icon_index - (3 * icon_level * icon_level - 3 * icon_level + 1)) / icon_level) / 3;
        cos_offset = cos_angle * cos_float(angle_offset) - sin_angle * sin_float(angle_offset);
        sin_offset = sin_angle * cos_float(angle_offset) + cos_angle * sin_float(angle_offset);
    } else {
        triangle_a = 0;
        triangle_b = 0;
        triangle_c = 0;
        cos_angle = 0;
        sin_angle = 0;
        angle_offset = 0;
        cos_offset = 0;
        sin_offset = 0;
    }
    //图标的中心坐标
    y_abs = (float)center_y + (float)(triangle_c * sin_offset * total_ratio);
    x_abs = (float)center_x + (float)(triangle_c * cos_offset * total_ratio);
    dist_x = (x_abs - lcd_width_half);          //到屏幕中心的水平距离
    dist_y = (y_abs - lcd_height_half);         //到屏幕中心的垂直距离
    dist_c = complex_abs_float(dist_x, dist_y); //到屏幕中心的径向距离
#if MODE_2_5        //2.5D立体效果
    /*
        简化的球面投影模型
    */

#if (STAR_ROTATE_MODE == STAR_RATIO_MODE_SPHERE_FUNC)
    if (SPHERE_R > dist_c) {
        float cos_z_angle = (float)complex_dqdt_float(SPHERE_R, dist_c) / SPHERE_R;             //计算与z轴夹角余弦值
#elif (STAR_ROTATE_MODE == STAR_RATIO_MODE_LINEAR_FUNC)
    if (LINEAR_FUNC_A / 2 > dist_c) {
        float cos_z_angle = (float)(LINEAR_FUNC_A - dist_c) / LINEAR_FUNC_A;
#else
#error "ROTATE_MODE not defined"
#endif
        ratio_val = cos_z_angle;
        //重算图标到屏幕中心的径向距离,也就是2.5D下图标的疏密程度
//        float dist_r = ((float) dist_c / SPHERE_R) *((float) dist_c / SPHERE_R) * SPHERE_DIST_A;
        float dist_r = SPHERE_DIST_A * (exp_float((float)dist_c / SPHERE_R) - 1) - SPHERE_DIST_B * sin_float((float)dist_c / SPHERE_R / 2);
//        float dist_r = (float)65*dist_c/SPHERE_R;
        screen_lock = (dist_c > SPHERE_R) ? 1 : 0;          //z轴负球面的图标不显示
        if (dist_c && !screen_lock) {                                       //根据新距离计算坐标值
            x_abs -= dist_r * dist_x / dist_c;
            y_abs -= dist_r * dist_y / dist_c;
        }
        dist_x = (x_abs - lcd_width_half);
        dist_y = (y_abs - lcd_height_half);
        dist_c -= dist_r;
#if (STAR_ROTATE_MODE == STAR_RATIO_MODE_SPHERE_FUNC)
        cos_z_angle = (float)complex_dqdt_float(SPHERE_R, dist_c) / SPHERE_R;             //重算与z轴夹角余弦值
#elif (STAR_ROTATE_MODE == STAR_RATIO_MODE_LINEAR_FUNC)
        cos_z_angle = (float)(LINEAR_FUNC_A - dist_c) / LINEAR_FUNC_A;                    //重算
#else
#error "ROTATE_MODE not defined"
#endif

        ratio_val = cos_z_angle;                                            //重新获取缩放系数
        //printf("[star]index:%d ratio:%.2f,dist_r:%.2f dist_c:%.2f",icon_index,cos_z_angle,dist_r,dist_c);
    }

#endif//MODE_2_5
    //计算图标中心到屏幕中心的水平、垂直和径向距离
    icon_abs_left = x_abs - icon_distance_r * ratio_val  / 2;
    icon_abs_right = x_abs + icon_distance_r * ratio_val / 2;
    icon_abs_top = y_abs - icon_distance_r * ratio_val  / 2;
    icon_abs_bottom = y_abs + icon_distance_r * ratio_val / 2;

#if SQUARE_SCREEN       //方屏处理
    int rounded_rect_left    = SCREEN_LEFT + ROUNDED_RECTANGLE_R;
    int rounded_rect_right   = SCREEN_RIGHT - ROUNDED_RECTANGLE_R;
    int rounded_rect_top     = SCREEN_TOP + ROUNDED_RECTANGLE_R;
    int rounded_rect_bottom  = SCREEN_BOTTOM - ROUNDED_RECTANGLE_R;

    /*    int is_rounded_rect_mode = (icon_abs_left<=rounded_rect_left&&icon_abs_top<=rounded_rect_top)?ROUNDED_RECT_LEFT_TOP:ROUNDED_RECT_NULL;
            is_rounded_rect_mode = (is_rounded_rect_mode)?is_rounded_rect_mode:(icon_abs_right>=rounded_rect_right&&icon_abs_top<=rounded_rect_top)?ROUNDED_RECT_RIGHT_TOP:ROUNDED_RECT_NULL;
            is_rounded_rect_mode = (is_rounded_rect_mode)?is_rounded_rect_mode:(icon_abs_left<=rounded_rect_left&&icon_abs_bottom>=rounded_rect_bottom)?ROUNDED_RECT_LEFT_BOTTOM:ROUNDED_RECT_NULL;
            is_rounded_rect_mode = (is_rounded_rect_mode)?is_rounded_rect_mode:(icon_abs_right>=rounded_rect_right&&icon_abs_bottom>=rounded_rect_bottom)?ROUNDED_RECT_RIGHT_BOTTOM:ROUNDED_RECT_NULL;
    */
    int is_rounded_rect_mode = (x_abs <= rounded_rect_left && y_abs <= rounded_rect_top) ? ROUNDED_RECT_LEFT_TOP : ROUNDED_RECT_NULL;
    is_rounded_rect_mode = (is_rounded_rect_mode) ? is_rounded_rect_mode : (x_abs >= rounded_rect_right && y_abs <= rounded_rect_top) ? ROUNDED_RECT_RIGHT_TOP : ROUNDED_RECT_NULL;
    is_rounded_rect_mode = (is_rounded_rect_mode) ? is_rounded_rect_mode : (x_abs <= rounded_rect_left && y_abs >= rounded_rect_bottom) ? ROUNDED_RECT_LEFT_BOTTOM : ROUNDED_RECT_NULL;
    is_rounded_rect_mode = (is_rounded_rect_mode) ? is_rounded_rect_mode : (x_abs >= rounded_rect_right && y_abs >= rounded_rect_bottom) ? ROUNDED_RECT_RIGHT_BOTTOM : ROUNDED_RECT_NULL;
    int rect_center_x = 0;
    int rect_center_y = 0;
    //完全超出屏幕边界的 或者在Z轴负半球面的图标，隐藏
    if ((icon_abs_right <= SCREEN_LEFT) || \
        (icon_abs_bottom <= SCREEN_TOP) || \
        (icon_abs_left >= SCREEN_RIGHT) || \
        (icon_abs_top >= SCREEN_BOTTOM) || screen_lock)
    {

        invisible = 1;
        ratio_val = 1.0f;
    }
    //压在屏幕四角圆边界上的，进行二次缩放
    else if (is_rounded_rect_mode)
    {
        switch (is_rounded_rect_mode) {
        case ROUNDED_RECT_LEFT_TOP:
            rect_center_x = rounded_rect_left;
            rect_center_y = rounded_rect_top;
            break;
        case ROUNDED_RECT_RIGHT_TOP:
            rect_center_x = rounded_rect_right;
            rect_center_y = rounded_rect_top;
            break;
        case ROUNDED_RECT_LEFT_BOTTOM:
            rect_center_x = rounded_rect_left;
            rect_center_y = rounded_rect_bottom;
            break;
        case ROUNDED_RECT_RIGHT_BOTTOM:
            rect_center_x = rounded_rect_right;
            rect_center_y = rounded_rect_bottom;
            break;
        }
        dist_x = (x_abs - rect_center_x);
        dist_y = (y_abs - rect_center_y);

        float rect_center_r = complex_abs_float(dist_x, dist_y);

        dist_c_big = rect_center_r + (float)icon_distance_r * ratio_val / 2;
        dist_c_small = rect_center_r - (float)icon_distance_r * ratio_val / 2;

        invisible = 0;
        if (dist_c_big >= ROUNDED_RECTANGLE_R && bound_en) {
            ratio_val = (float)(ROUNDED_RECTANGLE_R - dist_c_small) / icon_distance_r;
            x_abs -= (rect_center_r - (dist_c_small + (ROUNDED_RECTANGLE_R - dist_c_small) / 2)) * dist_x / rect_center_r;
            y_abs -= (rect_center_r - (dist_c_small + (ROUNDED_RECTANGLE_R - dist_c_small) / 2)) * dist_y / rect_center_r;
        } else {
            ratio_val *= 1.0f;
        }
    } else
    {
        invisible = 0;
        float ratio_val_x, ratio_val_y;
        //压在屏幕四边非圆角区域内的，进行二次缩放
        if (icon_abs_left < SCREEN_LEFT && bound_en) {
            ratio_val_x = (float)(icon_abs_right - SCREEN_LEFT) / icon_distance_r;
            x_abs = SCREEN_LEFT + (icon_abs_right - SCREEN_LEFT) / 2;
        } else if (icon_abs_right > SCREEN_RIGHT && bound_en) {
            ratio_val_x = (float)(SCREEN_RIGHT - icon_abs_left) / icon_distance_r;
            x_abs = SCREEN_RIGHT - (SCREEN_RIGHT - icon_abs_left) / 2;
        } else {
            ratio_val_x = ratio_val;
        }
        if (icon_abs_top < SCREEN_TOP && bound_en) {
            ratio_val_y = (float)(icon_abs_bottom - SCREEN_TOP) / icon_distance_r;
            y_abs = SCREEN_TOP + (icon_abs_bottom - SCREEN_TOP) / 2;
        } else if (icon_abs_bottom > SCREEN_BOTTOM && bound_en) {
            ratio_val_y = (float)(SCREEN_BOTTOM - icon_abs_top) / icon_distance_r;
            y_abs = SCREEN_BOTTOM - (SCREEN_BOTTOM - icon_abs_top) / 2;
        } else {
            ratio_val_y = ratio_val;
        }
        ratio_val = MIN(ratio_val_x, ratio_val_y);
    }
#else   //圆屏处理
    dist_c_big = dist_c + ratio_val * icon_distance_r / 2;
    dist_c_small = dist_c - ratio_val * icon_distance_r / 2;

    int screen_r = MIN(lcd_width_half, lcd_height_half);
    if (dist_c_small >= screen_r || screen_lock)
    {
        invisible = 1;
        ratio_val = 1.0f;
    } else
    {
        invisible = 0;
        if (dist_c_big >= screen_r && bound_en) {
            ratio_val = (float)(screen_r - dist_c_small) / icon_distance_r;
            x_abs = lcd_width_half + (float)(dist_c_small + (screen_r - dist_c_small) / 2) * dist_x / dist_c;
            y_abs = lcd_height_half + (float)(dist_c_small + (screen_r - dist_c_small) / 2) * dist_y / dist_c;
        } else {
            ratio_val *= 1.0f;
        }
    }
#endif//SQUARE_SCREEN
    ratio_val *= total_ratio;
    if (ratio_val < 0.125)
    {
        invisible = 1;
    }
    __info->index = icon_index;
    __info->icon_level = icon_level;
    __info->invisible = invisible;
    __info->ic_x = x_abs + 0.5f;
    __info->ic_y = y_abs + 0.5f;
    __info->width = icon_width_height * ratio_val;
    __info->height = icon_width_height * ratio_val;
    __info->ratio = ratio_val;
    //printf("%s[%d-%d](%d,%d)(%f) inv:%d",\
    __func__, __info->index, __info->icon_level, __info->ic_x, __info->ic_y, __info->ratio, __info->invisible);
    return 0;
}

/******************************************************************
                图标缩放与坐标初始化
    __layout 图标所在布局
    ratio_index 缩放等级
    x_offset x方向移动距离
    y_offset y方向移动距离
    bound_en 是否对屏幕边框进行约束
******************************************************************/
static int icon_postion(struct layout *__layout, u8 ratio_index, int x_offset, int y_offset, int bound_en)
{
    //防止ratio_index溢出
    __this->icon_ratio_index = (ratio_index >= (ARRAY_SIZE(icon_ratio_table))) ? (ARRAY_SIZE(icon_ratio_table) - 1) : ratio_index;
    __this->icon_ratio_index = (ratio_index < 0) ? 0 : __this->icon_ratio_index;

    //移动中心图标
    __this->icon_center.x += x_offset;
    __this->icon_center.y += y_offset;
    //获取屏幕尺寸信息
    int lcd_width = S_LCD_WIDTH;
    int lcd_height = S_LCD_HEIGHT;
    int icon_level;                             //图标圈层
    int icon_index = 0;                         //图标索引
    int real_index = 0;

    struct element *elm, *p, *n;                //遍历图标使用
    elm = &__layout->elm;                       //获取布局句柄
    list_for_each_child_element_safe(p, n, elm) { //遍历所有图标设置坐标和缩放等级
#if ICON_DEL_EN
        if (real_index < 32) {
            if (__this->icon_hide_ctrl0 & BIT(real_index)) {
                //隐藏并移除屏幕
                p->css.invisible = 1;
                p->css.top = 10000;
                p->css.left = 10000;
                real_index ++;
                continue;
            }
        } else if (real_index < 64) {
            if (__this->icon_hide_ctrl1 & BIT(real_index - 32)) {
                //隐藏并移除屏幕
                p->css.invisible = 1;
                p->css.top = 10000;
                p->css.left = 10000;
                real_index ++;
                continue;
            }
        } else {
            ASSERT(0);//需拓展
        }
        real_index ++;

#endif
        struct star_info __info;
        //计算各图标与中心图标的坐标关系

        star_icon_info_do(&__info, icon_ratio_table[__this->icon_ratio_index], __this->icon_center.x, __this->icon_center.y, icon_index, bound_en);

        //将计算的数据复制给icon
        p->css.width = CSS(__info.width, lcd_width);
        p->css.height = CSS(__info.height, lcd_height);
        p->css.left = C2LT(CSS(__info.ic_x, lcd_width), p->css.width);
        p->css.top  = C2LT(CSS(__info.ic_y, lcd_height), p->css.height);
        p->css.ratio.en = 1;
        p->css.ratio.ratio_h = __info.ratio;
        p->css.ratio.ratio_w = __info.ratio;
        p->css.invisible = __info.invisible;

        //左对齐，图标抖动不明显
//        p->css.align = 0;
        //自动切换图标索引
#if AUTO_SEL_INDEX
#if ICON_DEL_EN
        ui_pic_set_image_index(p, real_index);
#else
        ui_pic_set_image_index(p, icon_index % 28);
#endif
#endif
        icon_index++;
    }
    //启动绘制
    ui_core_redraw(elm);
    return 0;
}
/******************************************************************
                图标缩放与坐标初始化
    __layout 图标所在布局
    ratio_index 缩放等级
    x_offset x方向移动距离
    y_offset y方向移动距离
    bound_en 是否对屏幕边框进行约束
******************************************************************/
static int icon_postion_anima(struct layout *__layout, float ratio, int x_offset, int y_offset, int bound_en)
{
    //移动中心图标
    __this->icon_center.x += x_offset;
    __this->icon_center.y += y_offset;
    //获取屏幕尺寸信息
    int lcd_width = S_LCD_WIDTH;
    int lcd_height = S_LCD_HEIGHT;
    int icon_level;                             //图标圈层
    int icon_index = 0;                         //图标索引
    int real_index = 0;

    struct element *elm, *p, *n;                //遍历图标使用
    elm = &__layout->elm;                       //获取布局句柄
    list_for_each_child_element_safe(p, n, elm) { //遍历所有图标设置坐标和缩放等级
        struct star_info __info;
        //计算各图标与中心图标的坐标关系

        star_icon_info_do(&__info, ratio, __this->icon_center.x, __this->icon_center.y, icon_index, bound_en);

        //将计算的数据复制给icon
        p->css.width = CSS(__info.width, lcd_width);
        p->css.height = CSS(__info.height, lcd_height);
        p->css.left = C2LT(CSS(__info.ic_x, lcd_width), p->css.width);
        p->css.top  = C2LT(CSS(__info.ic_y, lcd_height), p->css.height);
        p->css.ratio.en = 1;
        p->css.ratio.ratio_h = __info.ratio;
        p->css.ratio.ratio_w = __info.ratio;
        p->css.invisible = __info.invisible;

        //左对齐，图标抖动不明显
//        p->css.align = 0;
        //自动切换图标索引
#if AUTO_SEL_INDEX
        ui_pic_set_image_index(p, icon_index % 28);
#endif
        icon_index++;
    }
    //启动绘制
    ui_core_redraw(elm);
    return 0;
}
/******************************************************************
                图标初始化
******************************************************************/
static int icon_init(void)
{
    __this->icon_ratio_index = ICON_INDEX_INIT;
    __this->icon_center.x = get_lcd_width_from_imd() / 2;
    __this->icon_center.y = get_lcd_height_from_imd() / 2;
#if ICON_DEL_EN
    //初始化全部显示
    __this->icon_hide_ctrl0 = 0;
    __this->icon_hide_ctrl1 = 0;
#endif
    __this->anima_enable = 0;
    __this->anima_ratio = 1.0f;
    return 0;
}


/******************************************************************
                控件事件注册
******************************************************************/
static int LAYOUT_STYLE_STAR_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct element *elm = (struct element *)_ctrl;
    switch (event) {
    case ON_CHANGE_INIT:
        printf("%s %d %d %d %d", __func__, elm->css.left, elm->css.top, elm->css.width, elm->css.height);
        elm->css.left = 0;
        elm->css.top = 0;
        elm->css.width = 10000;
        elm->css.height = 10000;
        break;
    case ON_CHANGE_SHOW_POST:
#if ICON_DEBUG
        int rounded_rect_left    = SCREEN_LEFT + ROUNDED_RECTANGLE_R;
        int rounded_rect_right   = SCREEN_RIGHT - ROUNDED_RECTANGLE_R;
        int rounded_rect_top     = SCREEN_TOP + ROUNDED_RECTANGLE_R;
        int rounded_rect_bottom  = SCREEN_BOTTOM - ROUNDED_RECTANGLE_R;
        ui_draw_line(arg, SCREEN_LEFT, SCREEN_TOP, SCREEN_LEFT, SCREEN_BOTTOM, 0x1f);
        ui_draw_line(arg, SCREEN_RIGHT, SCREEN_TOP, SCREEN_RIGHT, SCREEN_BOTTOM, 0x1f);
        ui_draw_line(arg, SCREEN_LEFT, SCREEN_TOP, SCREEN_RIGHT, SCREEN_TOP, 0x1f);
        ui_draw_line(arg, SCREEN_LEFT, SCREEN_BOTTOM, SCREEN_RIGHT, SCREEN_BOTTOM, 0x1f);
        ui_draw_ring(arg, rounded_rect_left, rounded_rect_top, ROUNDED_RECTANGLE_R + 1, ROUNDED_RECTANGLE_R, 180, 270, 0x1f, 100);
        ui_draw_ring(arg, rounded_rect_right, rounded_rect_top, ROUNDED_RECTANGLE_R + 1, ROUNDED_RECTANGLE_R, 270, 360, 0x1f, 100);
        ui_draw_ring(arg, rounded_rect_left, rounded_rect_bottom, ROUNDED_RECTANGLE_R + 1, ROUNDED_RECTANGLE_R, 90, 190, 0x1f, 100);
        ui_draw_ring(arg, rounded_rect_right, rounded_rect_bottom, ROUNDED_RECTANGLE_R + 1, ROUNDED_RECTANGLE_R, 0, 90, 0x1f, 100);
#endif ICON_DEBUG
        break;
    case ON_CHANGE_FIRST_SHOW:
        struct layout *layout = (struct layout *)_ctrl;
        //初始化信息
        icon_init();
        //初始化坐标
        icon_postion(layout, __this->icon_ratio_index, 0, 0, 1);
        icon_move_center_post(layout, 0);
        //接管按键事件
        key_ui_takeover(1);
        break;
    case ON_CHANGE_RELEASE:
        //释放按键事件
        key_ui_takeover(0);
        //释放定时器
        if ((__this->timer_id)) {
            sys_timer_del(__this->timer_id);
            __this->timer_id = 0;
        }
#if CENTER_WATCH_EN
        if (__this->watch_run_id) {
            sys_timer_del(__this->watch_run_id);
            __this->watch_run_id = 0;
        }
#endif // ICON_DEBUG
        break;
    default:
        return FALSE;
    }
    return FALSE;
}
static int LAYOUT_STYLE_STAR_onkey(void *_ctr, struct element_key_event *e)
{
    printf("%s %d ", __func__, e->value);
    switch (e->value) {
#if (RATIO_EN)
    case KEY_UI_PLUS:
        if (__this->timer_id) { //定时器跑时不响应
            break;
        }
        icon_move_center_restore(_ctr);
        if (__this->icon_ratio_index) {
            __this->icon_ratio_index--;
            __this->anima_enable = 0;
            __this->anima_ratio = 1.0f;
            icon_postion(_ctr, __this->icon_ratio_index, 0, 0, 1);
        } else {
            __this->anima_enable = 1;
            __this->anima_ratio += ANIMA_STEP;
            if (__this->anima_ratio >= ANIMA_MAX) {
                __this->anima_ratio = ANIMA_MAX;
            }
            icon_postion_anima(_ctr, __this->anima_ratio, 0, 0, 0);
        }
        icon_move_center_post(_ctr, 1);
        break;
    case KEY_UI_MINUS:
        if (__this->timer_id) { //定时器跑时不响应
            break;
        }
        icon_move_center_restore(_ctr);
        if (__this->anima_enable) {
            __this->anima_ratio -= ANIMA_STEP;
            if (__this->anima_ratio <= 1.0f) {
                __this->anima_enable = 0;
            }
            icon_postion_anima(_ctr, __this->anima_ratio, 0, 0, 0);
        } else {
            __this->icon_ratio_index++;
            if (__this->icon_ratio_index >= ARRAY_SIZE(icon_ratio_table)) {
                __this->timer_id = sys_timer_add(_ctr, icon_ratio_timer_do, 100);
            }
            icon_postion(_ctr, __this->icon_ratio_index, 0, 0, 1);
        }

        icon_move_center_post(_ctr, 1);
        break;
#endif//(RATIO_EN)
    case KEY_UI_HOME://回表盘
        UI_HIDE_CURR_WINDOW();
        UI_SHOW_WINDOW(ID_WINDOW_BT);
        break;
    default:
        break;
    }

    return false;
}
void icon_postion_energy_timer(void *p)
{
    int single_r = ENERGY_A * ENERGY_TIME_INTERVAL * ENERGY_TIME_INTERVAL;
    int single_x = single_r * __this->energy_offset.x / complex_abs_float(__this->energy_offset.x, __this->energy_offset.y);
    int single_y = single_r * __this->energy_offset.y / complex_abs_float(__this->energy_offset.x, __this->energy_offset.y);
    printf("%s %d [%d %d] [%d %d]", __func__, single_r, single_x, single_y, __this->energy_offset.x, __this->energy_offset.y);
    icon_postion(p, __this->icon_ratio_index, single_x, single_y, 1);
    __this->energy_offset.x -= single_x;
    __this->energy_offset.y -= single_y;

    if (complex_abs_float(__this->icon_center.x - S_LCD_WIDTH / 2, __this->icon_center.y - S_LCD_HEIGHT / 2) > ENERGY_MAX_DIST || complex_abs_float(__this->energy_offset.x, __this->energy_offset.y) < single_r) {
        __this->energy_offset.x = 0;
        __this->energy_offset.y = 0;
        if (__this->timer_id) {
            sys_timer_del(__this->timer_id);
            __this->timer_id = 0;
        }
        icon_move_center(p);
    }
}
static int LAYOUT_STYLE_STAR_ontouch(void *_ctrl, struct element_touch_event *e)
{
    struct element *elm = (struct element *)_ctrl;
    /* printf("%s %d",__func__,e->event); */
    if (__this->timer_id) { //定时器跑时不响应
        return false;
    }
    static u8 touch_flag = 0;

    switch (e->event) {
    case ELM_EVENT_TOUCH_R_MOVE:
    case ELM_EVENT_TOUCH_L_MOVE:
        return true;
    case ELM_EVENT_TOUCH_DOWN:
        touch_flag = 0;
        icon_move_center_restore(elm);
        return true;//接管事件
        break;
    case ELM_EVENT_TOUCH_HOLD:
        touch_flag |= BIT(STAR_TOUCH_FLAG_HOLD);
#if ICON_DEL_EN
        int touch_index = icon_get_touch_icon_index(elm, &e->pos);
        int touch_id = icon_get_touch_icon_id(elm, &e->pos);
        icon_del_by_index(elm, touch_index, 1, 1);
        printf("id:%x index:%d \n", touch_id, touch_index);
#endif
        break;
    case ELM_EVENT_TOUCH_MOVE:
        touch_flag |= BIT(STAR_TOUCH_FLAG_MOVE);
        int x_offset = e->xoffset;
        int y_offset = e->yoffset;
        icon_postion(_ctrl, __this->icon_ratio_index, x_offset, y_offset, 1);
        break;
    case ELM_EVENT_TOUCH_ENERGY:
        struct rect r;

        int energy_t = (e->pos.x + 1) & 0xffff;
        int energy_x = ABS(e->pos.x >> 16);
        int energy_y = ABS(e->pos.y >> 16);
        int energy_dir_x = (e->pos.y & 0xff);
        int energy_dir_y = ((e->pos.y >> 8) & 0xff);
        float energy_a = ENERGY_A;
        float energy_v_x = (float)energy_x / energy_t;
        float energy_v_y = (float)energy_y / energy_t;
        float energy_dist_x = energy_v_x * energy_v_x / 2 / energy_a;
        float energy_dist_y = energy_v_y * energy_v_y / 2 / energy_a;
        int energy_dist_r = complex_abs_float(energy_dist_x, energy_dist_y);
        int energy_dist_r_d = energy_dist_r;
        int energy_dist_r_x = energy_dist_r_d * energy_dist_x / energy_dist_r ;
        int energy_dist_r_y = energy_dist_r_d * energy_dist_y / energy_dist_r ;
        //1左上2右下
        __this->energy_offset.x = (energy_dir_x == 2) ? energy_dist_r_x : -energy_dist_r_x;
        __this->energy_offset.y = (energy_dir_y == 2) ? energy_dist_r_y : -energy_dist_r_y;

        printf("t_a(%d %f) x_y(%d %d) vx_y(%f %f) dist_x_t(%f %f) dir_x_y(%d %d) dr_mr(%d %d) drx_y(%d %d) \n",
               energy_t, energy_a, \
               energy_x, energy_y, \
               energy_v_x, energy_v_y, \
               energy_dist_x, energy_dist_y, \
               energy_dir_x, energy_dir_y, \
               energy_dist_r, energy_dist_r_d, \
               energy_dist_r_x, energy_dist_r_y\
              );
        if (!__this->timer_id) {
            __this->timer_id = sys_timer_add(_ctrl, icon_postion_energy_timer, ENERGY_TIME_INTERVAL);
        }

        break;
    case ELM_EVENT_TOUCH_UP:
        if (touch_flag & BIT(STAR_TOUCH_FLAG_MOVE) && !e->has_energy) {
            icon_move_center(_ctrl);

        } else if (touch_flag & BIT(STAR_TOUCH_FLAG_HOLD) && !e->has_energy) {
            // not to do

        } else if (touch_flag & BIT(STAR_TOUCH_FLAG_ENERGY) && e->has_energy) {


        } else {
            int touch_id = icon_get_touch_icon_id(_ctrl, &e->pos);
            if (touch_id != -1) {
                extern int star_menu_in_sel_by_id(u32 icon_id);
                star_menu_in_sel_by_id(touch_id);
            }
        }
        break;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(STAR_LAYOUT)
.onchange = LAYOUT_STYLE_STAR_onchange,
 .onkey = LAYOUT_STYLE_STAR_onkey,
  .ontouch = LAYOUT_STYLE_STAR_ontouch,
};







#if CENTER_WATCH_EN
/************************************************************
    满天星显示表盘
************************************************************/
static u8 usr_watch_flag = 0;
static u8 usr_watch_cnt = 0;
void usr_watch_enable(void)
{
    usr_watch_flag = 1;
    usr_watch_cnt = 0;
}
void usr_watch_disable(void)
{
    usr_watch_flag = 0;
}
void usr_watch_run(void *p)
{
    struct element *elm = ui_core_get_element_by_id(STAR_WATCH_LAYOUT);
    if (!elm) {
        return ;
    }
    ui_core_redraw(elm);
}
/************************************************************
    center_x:指针图片左上角到旋转中心的水平距离
    center_y:指针图片左上角到旋转中心的垂直距离
    涉及invisible修改，在show_probe中调用
************************************************************/
static void usr_watch_rotate_pic(struct ui_pic *pic, int center_x, int center_y, int degree, int show)
{
    struct rect rect = {0};
    struct rect rect_parent = {0};
    ui_core_get_element_abs_rect(&pic->elm, &rect);
    ui_core_get_element_abs_rect(pic->elm.parent, &rect_parent);
    pic->elm.css.rotate.en = 1;
    pic->elm.css.rotate.cent_x = center_x;
    pic->elm.css.rotate.cent_y = center_y;
    pic->elm.css.rotate.dx = rect_parent.left + rect_parent.width / 2 ;
    pic->elm.css.rotate.dy = rect_parent.top + rect_parent.height / 2 ;
    pic->elm.css.rotate.angle = degree;
    pic->elm.css.invisible = !show;
    //printf("%s %d (%d %d) (%d %d)",__func__,degree,pic->elm.css.rotate.cent_x,pic->elm.css.rotate.cent_y,pic->elm.css.rotate.dx,pic->elm.css.rotate.dy);
}
static int STAR_WATCH_onchange(void *_ctrl, enum element_change_event event, void *arg)
{
    struct element *elm = (struct element *)_ctrl;
    struct element *pic_elm;
    struct ui_pic *pic;
    int watch_degree = 0;
    static u8 last_sec = 0;
    switch (event) {
    case ON_CHANGE_INIT:
        usr_watch_cnt = 0;
        break;
    case ON_CHANGE_SHOW_PROBE:
        struct sys_time time;
        watch_file_get_sys_time(&time);
//        time.hour = 3;
//        time.min = 30;
//        time.sec = 45;
        list_for_each_child_element(pic_elm, elm) {
            if (ui_id2type(pic_elm->id) != CTRL_TYPE_PIC) {
                continue;
            }
            pic = (struct ui_pic *)pic_elm;
//            printf("soucre:%s",pic->source);
            if (!strcmp(pic->source, CENTER_SOURCE_HH)) {
                watch_degree = time.hour * 360 / 12 + time.min / 12;
                usr_watch_rotate_pic(pic_elm, CENTER_WATCH_HH_X, CENTER_WATCH_HH_Y, watch_degree, usr_watch_flag);
            } else if (!strcmp(pic->source, CENTER_SOURCE_MM)) {
                watch_degree = time.min * 360 / 60 + time.sec / 12;
                usr_watch_rotate_pic(pic_elm, CENTER_WATCH_MM_X, CENTER_WATCH_MM_Y, watch_degree, usr_watch_flag);
            } else if (!strcmp(pic->source, CENTER_SOURCE_SS)) {
                int ms = (timer_get_ms() % 1000);
                if (last_sec != time.sec) {
                    last_sec = time.sec;
                    usr_watch_cnt = 0;
                }
                watch_degree = time.sec * 360 / 60 + usr_watch_cnt;
                //printf("%s watch_degree:%d sec:%d ms:%d",__func__,watch_degree,time.sec,usr_watch_cnt);
                usr_watch_rotate_pic(pic_elm, CENTER_WATCH_SS_X, CENTER_WATCH_MM_Y, watch_degree, usr_watch_flag);
                usr_watch_cnt++;
            }
        }
        break;
    case ON_CHANGE_RELEASE:
        break;
    default:
        return FALSE;
    }
    return FALSE;
}

REGISTER_UI_EVENT_HANDLER(STAR_WATCH_LAYOUT)
.onchange = STAR_WATCH_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

#endif
#endif
#endif
#endif
