/* serialp.h */

#ifndef SERIAL_P
#define SERIAL_P

#include "constant.h"

BOOL AsyncReadWrite(char* buf);
int AsyncOpen(char* szPort);
int AsyncClose(void);
int AsyncSendFiletoTNC(char *szName);

#endif

