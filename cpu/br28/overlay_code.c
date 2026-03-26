#include "asm/includes.h"
#include "media/includes.h"
#include "overlay_code.h"


struct code_type {
    u32 type;
    u32 dst;
    u32 src;
    u32 size;
};

extern int aec_addr, aec_begin, aec_size;
extern int aac_addr, aac_begin, aac_size;
#if OVERLAY_DEMO_EN
extern int demo0_addr, demo0_begin, demo0_size;
extern int demo1_addr, demo1_begin, demo1_size;
#endif


const struct code_type  ctype[] = {
    {OVERLAY_AEC, (u32) &aec_addr, (u32) &aec_begin, (u32) &aec_size},
    {OVERLAY_M4A, (u32) &aac_addr, (u32) &aac_begin, (u32) &aac_size },
#if OVERLAY_DEMO_EN
    {OVERLAY_DEMO0, (u32) &demo0_addr, (u32) &demo0_begin, (u32) &demo0_size },
    {OVERLAY_DEMO1, (u32) &demo1_addr, (u32) &demo1_begin, (u32) &demo1_size },
#endif
};

struct audio_overlay_type {
    u32 atype;
    u32 otype;
};

const struct audio_overlay_type  aotype[] = {

    {AUDIO_CODING_MSBC, OVERLAY_AEC },
    {AUDIO_CODING_CVSD, OVERLAY_AEC },
    /* {AUDIO_CODING_M4A, OVERLAY_M4A }, */
    /* {AUDIO_CODING_ALAC, OVERLAY_M4A }, */
    {AUDIO_CODING_AAC, OVERLAY_M4A },
};


void overlay_load_code(u32 type)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(ctype); i++) {
        if (type == ctype[i].type) {
            if (ctype[i].dst != 0) {
                memcpy((void *)ctype[i].dst, (void *)ctype[i].src, (int)ctype[i].size);
            }
            break;
        }
    }
}

void audio_overlay_load_code(u32 type)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(aotype); i++) {
        if (type == aotype[i].atype) {
            overlay_load_code(aotype[i].otype);
        }
    }
}

#if OVERLAY_DEMO_EN  // overlay demo code

AT(.demo0_code0)
static void demo0_code0(void)
{
    u8 buf[32];
    memset(buf, 0xaa, sizeof(buf));
    printf("overlay demo0 code0: ");
    put_buf(buf, 32);
}

AT(.demo1_code0)
static void demo1_code0(void)
{
    u8 buf[32];
    memset(buf, 0xcc, sizeof(buf));
    printf("overlay demo1 code0: ");
    put_buf(buf, 32);

}

AT(.demo1_code1)
static void demo1_code1(void)
{
    u8 buf[32];
    memset(buf, 0xdd, sizeof(buf));
    printf("overlay demo1 code1: ");
    put_buf(buf, 32);

}

void overlay_demo_timer(void *p)
{
    static u8 cnt = 0;
    switch (cnt) {
    case 0x00:
        overlay_load_code(OVERLAY_DEMO0);
        printf("func demo0_code0 at addr: 0x%x\n", demo0_code0);
        demo0_code0();
        cnt = 1;
        break;
    case 0x01:
        overlay_load_code(OVERLAY_DEMO1);
        printf("func demo1_code0 at addr: 0x%x\n", demo1_code0);
        demo1_code0();
        cnt = 2;
        break;
    case 0x02:
        printf("func demo1_code1 at addr: 0x%x\n", demo1_code1);
        demo1_code1();
        cnt = 0;
        break;
    }
}

void overlay_demo_init(void)
{
    sys_timer_add(NULL, overlay_demo_timer, 5 * 1000);
}
#endif

