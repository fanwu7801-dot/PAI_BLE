/**
 *
 *  This file contains the quick application programming interface (API) declarations
 *  of TTS offline. Developer can include this file in your project to build applications.
 *  For more information, please read the developer guide.

 *  Use of this software is subject to certain restrictions and limitations set
 *  forth in a license agreement entered into between iFLYTEK, Co,LTD.
 *  and the licensee of this software.  Please refer to the license
 *  agreement for license use rights and restrictions.
 *
 * All rights reserved.
 * @file	tts.h
 * @brief   iFLY Speech Synthesizer Header File
 * @author	xbfeng.
 * @version	1.0
 * @date	2020/12/10
 *
 *
 */


#ifndef __TTS_H_
#define __TTS_H_


/**
 * @fn		callbackfunction
 * @brief	user defined callback function
 * @return	int 	- error code,When non-zero data is returned, TTS will be terminated
 * @param	void *context -reserved variable
 * @param	int msg	- reserved variable
 * @param	int ds	- data status bit
 * @param	int param2	- reverved variable
 * @param	int dSize	- output data size
 * @param	void* dBuffer	-out put data buffer
 * @see
 */
typedef int (*pUserCallback)(void *context, int msg, int ds, int param2, int dSize, const void *dBuffer);


/**
 * @fn		ttsSessionBegin
 * @brief	create a tts session
 * @return	int 	- error code
 * @param	const char* -appid
 * @param	pUserCallback mCallback	- call back function,call back while tts engine generate audio
 * @param	void* resource	-resource address.
 * @see
 */
int ttsSessionBegin(const char *appid, pUserCallback mCallback, void *resource);


/**
 * @fn		ttsSetParam
 * @brief	set parameter of tts engine
 * @return	int	- error code
 * @param	unsigned int key- key of paramter,see ttsConofig.h for details
 * @param	unsigned int value- value of key,see ttsConofig.h for details
 * @see
 */
int ttsSetParam(unsigned int key, unsigned int value);
/**
 * @fn		ttsTextPut
 * @brief	Enter the text waiting to be synthesized
 * @return	int	- error code
 * @param	const char* textString -text content,utf-8 formate for default
 * @param	unsigned int textLen - input text length
 * @see
 */

int ttsTextPut(const char *textString, unsigned int textLen);

/**
 * @fn		ttsSessionEnd
 * @brief	stop engine and recycle memory
 * @return	int	- error code
 * @see
 */
int ttsSessionEnd();

/**
 * @fn		ttsExit
 * @brief	exit tts
 * @return	int	- error code
 * @see
 */
int ttsExit();

#endif //__TTS_H_
