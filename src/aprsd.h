

#ifndef APRSD_H
#define APRSD_H

#include <string>

#include "constant.h"
#include "cpqueue.h"

/* update the next 3 lines with each version change */
#define SIGNON "# " PACKAGE_STRING " Oct 2002 aprsd group\r\n"
#define VERS PACKAGE_STRING
//The next char string is put into the AX25 "TO" field.  Keep length at 6 chars.
#define PGVERS TOCALL
/*--------------------------------------------------*/


// To run aprsd from another directory change the next line
extern const std::string HOMEDIR;

extern const std::string CONFPATH;
extern const std::string CONFFILE;
extern const std::string MAINLOG;
extern const std::string STSMLOG;
extern const std::string RFLOG;
extern const std::string UDPLOG;
extern const std::string ERRORLOG;
extern const std::string DEBUGLOG;
extern const std::string REJECTLOG;
extern const std::string LOOPLOG;
extern const std::string WELCOME;
extern const std::string TNC_INIT;
extern const std::string TNC_RESTORE;
extern const std::string APRSD_INIT;
extern const std::string SAVE_HISTORY;
extern const std::string USER_DENY;



#endif      // APRSD_H
