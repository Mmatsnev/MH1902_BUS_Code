#ifndef _CONTACTLESSAPI_H
#define _CONTACTLESSAPI_H


#define M1_CARD     0
#define CPUAPP      1
#define UNION_PAY   2
#define UNIONPAYCPU 3


unsigned char pro_GetCardID(unsigned char *pCardSn); // no indenped file

unsigned char CaptureCardType(void); //���ؿ���
unsigned char pro_APDU(unsigned char *sendbuf,unsigned short sendlen,unsigned char *recebuf,unsigned short *recelen);

#endif