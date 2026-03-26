/**
 *
 *  This file contains the config parameter of TTS offline.
 *  Developer can include this file in your project to build applications.
 *  For more information, please read the developer guide.

 *  Use of this software is subject to certain restrictions and limitations set
 *  forth in a license agreement entered into between iFLYTEK, Co,LTD.
 *  and the licensee of this software.  Please refer to the license
 *  agreement for license use rights and restrictions.
 *
 * All rights reserved.
 * @file	ttsConfig.h
 * @brief   iFLY Speech Synthesizer Header File
 * @author	xbfeng.
 * @version	1.0
 * @date	2020/12/10
 *
 *
 */

#ifndef TTSCONFIG_H
#define TTSCONFIG_H

#define ifly_TTS_BIG_ENDIAN      0
/*
 *	INSTANCE PARAMETERS
 */

/* constants for values of field nParamID */
#define ifly_TTS_PARAM_PARAMCH_CALLBACK	    0x00000000	/* parameter change callback entry */
#define ifly_TTS_PARAM_LANGUAGE			    0x00000100	/* language, e.g. Chinese */
#define ifly_TTS_PARAM_INPUT_CODEPAGE		0x00000101	/* input code page, e.g. GBK */
#define ifly_TTS_PARAM_TEXT_MARK			    0x00000102	/* text mark, e.g. CSSML */
#define ifly_TTS_PARAM_USE_PROMPTS			0x00000104	/* whether use prompts */
#define ifly_TTS_PARAM_RECOGNIZE_PHONEME	    0x00000105	/* how to recognize phoneme input */
#define ifly_TTS_PARAM_INPUT_MODE			0x00000200	/* input mode, e.g. from fixed buffer, from callback */
#define ifly_TTS_PARAM_INPUT_TEXT_BUFFER	    0x00000201	/* input text buffer */
#define ifly_TTS_PARAM_INPUT_TEXT_SIZE		0x00000202	/* input text size */
#define ifly_TTS_PARAM_INPUT_CALLBACK		0x00000203	/* input callback entry */
#define ifly_TTS_PARAM_PROGRESS_BEGIN		0x00000204	/* current processing position */
#define ifly_TTS_PARAM_PROGRESS_LENGTH		0x00000205	/* current processing length */
#define ifly_TTS_PARAM_PROGRESS_CALLBACK	    0x00000206	/* progress callback entry */
#define ifly_TTS_PARAM_READ_AS_NAME		    0x00000301	/* whether read as name */
#define ifly_TTS_PARAM_READ_DIGIT			0x00000302	/* how to read digit, e.g. read as number, read as value  */
#define ifly_TTS_PARAM_CHINESE_NUMBER_1	    0x00000303	/* how to read number "1" in Chinese */
#define ifly_TTS_PARAM_MANUAL_PROSODY		0x00000304	/* whether use manual prosody */
#define ifly_TTS_PARAM_ENGLISH_NUMBER_0	    0x00000305	/* how to read number "0" in Englsih */
#define ifly_TTS_PARAM_READ_WORD             0x00000306	/* how to read word in Englsih,  e.g. read by word, read as alpha  */
#define ifly_TTS_PARAM_OUTPUT_CALLBACK		0x00000401	/* output callback entry */
#define ifly_TTS_PARAM_ROLE				    0x00000500	/* speaker role */
#define ifly_TTS_PARAM_SPEAK_STYLE			0x00000501	/* speak style */
#define ifly_TTS_PARAM_VOICE_SPEED			0x00000502	/* voice speed */
#define ifly_TTS_PARAM_VOICE_PITCH			0x00000503	/* voice tone */
#define ifly_TTS_PARAM_VOLUME				0x00000504	/* volume value */
#define ifly_TTS_PARAM_CHINESE_ROLE           0x00000510	/* Chinese speaker role */
#define ifly_TTS_PARAM_ENGLISH_ROLE          0x00000511	/* English speaker role */
#define ifly_TTS_PARAM_VEMODE				0x00000600	/* voice effect - predefined mode */
#define ifly_TTS_PARAM_USERMODE			    0x00000701	/* user's mode */
#define ifly_TTS_PARAM_NAVIGATION_MODE		0x00000701	/* Navigation Version*/

#define ifly_TTS_PARAM_EVENT_CALLBACK		0x00001001	/* sleep callback entry */
#define ifly_TTS_PARAM_OUTPUT_BUF			0x00001002	/* output buffer */
#define ifly_TTS_PARAM_OUTPUT_BUFSIZE		0x00001003	/* output buffer size */
#define ifly_TTS_PARAM_DELAYTIME			    0x00001004	/* delay time */


/* constants for values of parameter ifly_TTS_PARAM_LANGUAGE */
#define ifly_TTS_LANGUAGE_AUTO               0           /* Detect language automatically */
#define ifly_TTS_LANGUAGE_CHINESE			1			/* Chinese (with English) */
#define ifly_TTS_LANGUAGE_ENGLISH			2			/* English */

/* constants for values of parameter ifly_TTS_PARAM_INPUT_CODEPAGE */
#define ifly_TTS_CODEPAGE_ASCII			    437			/* ASCII */
#define ifly_TTS_CODEPAGE_GBK				936			/* GBK (default) */
#define ifly_TTS_CODEPAGE_BIG5				950			/* Big5 */
#define ifly_TTS_CODEPAGE_UTF16LE			1200		/* UTF-16 little-endian */
#define ifly_TTS_CODEPAGE_UTF16BE			1201		/* UTF-16 big-endian */
#define ifly_TTS_CODEPAGE_UTF8				65001		/* UTF-8 */
#define ifly_TTS_CODEPAGE_GB2312			    ifly_TTS_CODEPAGE_GBK
#define ifly_TTS_CODEPAGE_GB18030			ifly_TTS_CODEPAGE_GBK
#if ifly_TTS_BIG_ENDIAN
#define ifly_TTS_CODEPAGE_UTF16			ifly_TTS_CODEPAGE_UTF16BE
#else
#define ifly_TTS_CODEPAGE_UTF16			ifly_TTS_CODEPAGE_UTF16LE
#endif
#define ifly_TTS_CODEPAGE_UNICODE			ifly_TTS_CODEPAGE_UTF16
#define ifly_TTS_CODEPAGE_PHONETIC_PLAIN	    23456		/* Kingsoft Phonetic Plain */

/* constants for values of parameter ifly_TTS_PARAM_TEXT_MARK */
#define ifly_TTS_TEXTMARK_NONE				0			/* none */
#define ifly_TTS_TEXTMARK_SIMPLE_TAGS		1			/* simple tags (default) */

/* constants for values of parameter ifly_TTS_PARAM_INPUT_MODE */
#define ifly_TTS_INPUT_FIXED_BUFFER		0			/* from fixed buffer */
#define ifly_TTS_INPUT_CALLBACK			1			/* from callback */

/* constants for values of parameter ifly_TTS_PARAM_READ_DIGIT */
#define ifly_TTS_READDIGIT_AUTO			0			/* decide automatically (default) */
#define ifly_TTS_READDIGIT_AS_NUMBER		1			/* say digit as number */
#define ifly_TTS_READDIGIT_AS_VALUE		2			/* say digit as value */

/* constants for values of parameter ifly_TTS_PARAM_CHINESE_NUMBER_1 */
#define ifly_TTS_CHNUM1_READ_YAO			0			/* read number "1" [yao1] in chinese (default) */
#define ifly_TTS_CHNUM1_READ_YI			1			/* read number "1" [yi1] in chinese */

/* constants for values of parameter ifly_TTS_PARAM_ENGLISH_NUMBER_0 */
#define ifly_TTS_ENNUM0_READ_ZERO			0			/* read number "0" [zero] in english (default) */
#define ifly_TTS_ENNUM0_READ_O				1			/* read number "0" [o] in englsih */

/* constants for values of parameter ifly_TTS_PARAM_SPEAKER */
#define ifly_TTS_ROLE_XIAOYAN				3			/* Xiaoyan (female, Chinese) */

/* constants for values of parameter ifly_TTS_PARAM_SPEAK_STYLE */
#define ifly_TTS_STYLE_PLAIN				    0			/* plain speak style */
#define ifly_TTS_STYLE_NORMAL				1			/* normal speak style (default) */

/* constants for values of parameter ifly_TTS_PARAM_VOICE_SPEED */
/* the range of voice speed value is from -32768 to +32767 */
#define ifly_TTS_SPEED_MIN					-32768		/* slowest voice speed */
#define ifly_TTS_SPEED_NORMAL				0			/* normal voice speed (default) */
#define ifly_TTS_SPEED_MAX					+32767		/* fastest voice speed */

/* constants for values of parameter ifly_TTS_PARAM_VOICE_PITCH */
/* the range of voice tone value is from -32768 to +32767 */
#define ifly_TTS_PITCH_MIN					-32768		/* lowest voice tone */
#define ifly_TTS_PITCH_NORMAL				0			/* normal voice tone (default) */
#define ifly_TTS_PITCH_MAX					+32767		/* highest voice tone */

/* constants for values of parameter ifly_TTS_PARAM_VOLUME */
/* the range of volume value is from -32768 to +32767 */
#define ifly_TTS_VOLUME_MIN				-32768		/* minimized volume */
#define ifly_TTS_VOLUME_NORMAL				0			/* normal volume */
#define ifly_TTS_VOLUME_MAX				+32767		/* maximized volume (default) */

/* constants for values of parameter ifly_TTS_PARAM_VEMODE */
#define ifly_TTS_VEMODE_NONE				    0			/* none */
#define ifly_TTS_VEMODE_WANDER				1			/* wander */
#define ifly_TTS_VEMODE_ECHO				    2			/* echo */
#define ifly_TTS_VEMODE_ROBERT				3			/* robert */
#define ifly_TTS_VEMODE_CHROUS				4			/* chorus */
#define ifly_TTS_VEMODE_UNDERWATER			5			/* underwater */
#define ifly_TTS_VEMODE_REVERB				6			/* reverb */
#define ifly_TTS_VEMODE_ECCENTRIC			7			/* eccentric */

/* constants for values of parameter ifly_TTS_PARAM_USERMODE(ifly_TTS_PARAM_NAVIGATION_MODE) */
#define ifly_TTS_USE_NORMAL				    0			/* synthesize in the Mode of Normal */
#define ifly_TTS_USE_NAVIGATION			    1			/* synthesize in the Mode of Navigation */
#define ifly_TTS_USE_MOBILE			        2			/* synthesize in the Mode of Mobile */
#define ifly_TTS_USE_EDUCATION				3			/* synthesize in the Mode of Education */

/* constants for values of parameter ifly_TTS_PARAM_READ_WORD */
#define ifly_TTS_READWORD_BY_WORD			2			/* say words by the way of word */
#define ifly_TTS_READWORD_BY_ALPHA			1			/* say words by the way of alpha */
#define ifly_TTS_READWORD_BY_AUTO			0			/* say words by the way of auto */


#endif //TTSCONFIG_H