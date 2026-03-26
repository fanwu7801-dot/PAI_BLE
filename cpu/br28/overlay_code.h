
#ifndef _OVERLAY_CODE_H_
#define _OVERLAY_CODE_H_

#include "typedef.h"

#define OVERLAY_DEMO_EN     0

enum {
    OVERLAY_AEC = 0,
    OVERLAY_WAV,
    OVERLAY_APE,
    OVERLAY_FLAC,
    OVERLAY_M4A,
    OVERLAY_AMR,
    OVERLAY_DTS,
    OVERLAY_FM,

#ifdef CONFIG_MP3_WMA_LIB_SPECIAL
    OVERLAY_MP3,
    OVERLAY_WMA,
#endif
    OVERLAY_LC3,
#if OVERLAY_DEMO_EN
    OVERLAY_DEMO0,
    OVERLAY_DEMO1,
#endif
};

void overlay_load_code(u32 type);
void audio_overlay_load_code(u32 type);

#endif

