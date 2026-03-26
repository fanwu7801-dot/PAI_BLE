

#ifndef __CALL_COMMON_H__
#define __CALL_COMMON_H__

#include "typedef.h"
#include "cat1/cat1_common.h"

enum {
    CALL_SEL_AUTO = 0,
    CALL_SEL_BT,
    CALL_SEL_CAT1,

    CALL_SEL_MAX,
};

void call_dial_number(int number_len, char *number);

void call_ctrl_answer();

void call_ctrl_hangup();

u8 call_ctrl_get_status(void);

char *call_ctrl_get_phone_num(void);

void call_ctrl_set_call_sel(int sel);

int call_ctrl_get_call_sel(void);

void call_ctrl_input_mute(u8 mute);

int call_ctrl_get_input_mute(void);

#endif
