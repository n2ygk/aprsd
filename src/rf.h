/* rf.h */

#ifndef RF_P
#define RF_P

#include "constant.h"

extern int CloseReader, threadAck;
extern int txrdy;
extern char tx_buffer[];
extern BOOL TncSysopMode;

/*--------------------------------------------------------------*/

int rfOpen(char* szPort);
int rfClose(void);
int rfSendFiletoTNC(char* szName);
void* rfReadCom(void* vp);        //Com port read thread
int rfWrite(char* cp);
void rfSetPath(char* path);

#endif

