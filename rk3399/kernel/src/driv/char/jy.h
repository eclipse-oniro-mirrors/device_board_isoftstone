#ifndef __MCU_JY_H__
#define __MCU_JY_H__

unsigned int CalCheckSum(unsigned char *pcMess,unsigned int wLen);
unsigned char get_mcu_result(unsigned char *rand,unsigned char *result);

#endif
