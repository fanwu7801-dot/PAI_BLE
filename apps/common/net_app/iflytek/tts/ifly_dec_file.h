#ifndef __IFLY_DEC_FILE_H
#define __IFLY_DEC_FILE_H

void ifly_net_tts_dec_close(void);
int ifly_net_tts_dec_open(void *read_priv, int (*read)(void *priv, void *buf, int len, int tmp_flag, int tmp_offset));
void ifly_net_tts_dec_resume(void *priv);
bool ifly_net_tts_dec_check_run(void);

#endif
