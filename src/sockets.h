/* sockets.h */

#include "constant.h"

int SocketOpen (char *rfport, char *destcall);
int SocketClose (void);
int SocketWrite (char *cp);
int SocketWrite (void);
BOOL SocketReadWrite (char buf[]);

