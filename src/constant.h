/* update the next 3 lines with each version change */
#define SIGNON "# aprsd 2.1.4.vk3sb.1 October 26 2000 by WA4DSY/VK3SB \r\n"
#define VERS "aprsd 2.1.4.vk3sb.1"
#define PGVERS "APD214"
/*--------------------------------------------------*/


#define TRUE 1
#define FALSE 0
#define BOOL int
#define ULONG unsigned long
#define LONG long
#define USHORT unsigned short int
#define INT16 unsigned short int
#define APIRET int
#define INT32 int
#define LF 0x0a
#define CR 0x0d
#define RXwhite " \t\n\r"

//#define LINK_SERVER_PORT 1313

#define CMD_END_THREAD -1L
#define SRC_TNC	-2L
#define SRC_USER	-3L
#define SRC_INTERNAL -4L
#define SRC_UDP -5L
#define SRC_IGATE -6L

#define MAXCLIENTS 30

//This sets the Internet line input buffer size.
//Lines longer than this, including CR,LF,NULL, will be truncated.
//Actually, the data will be truncated but a CR,LF,NULL will always be on the end.
#define BUFSIZE 255


#define RUNTSIZE 0
#define MAXRETRYS 7

#define HOSTIPSIZE 33

#define destINET 1
#define destTNC  2


/* To run aprsd from another directory change the next line */
#ifdef USE_FHS
#define HOMEDIR "/var/log/aprsd/"
#define CONFPATH "/etc/aprsd/"
#define LOGPATH "/var/log/aprsd/"
#define VARPATH "/var/lib/aprsd/"
#else
#define HOMEDIR "/home/aprsd2"
#define CONFPATH ""
#define LOGPATH ""
#define VARPATH ""
#endif

#define CONFFILE "aprsd.conf"
#define MAINLOG "aprsd.log"
#define STSMLOG "stsm.log"
#define RFLOG "rf.log"
#define UDPLOG "udp.log"
#define ERRORLOG "error.log"
#define WELCOME "welcome.txt"
#define TNC_INIT  "INIT.TNC"
#define TNC_RESTORE "RESTORE.TNC"
#define APRSD_INIT "INIT.APRSD"
#define SAVE_HISTORY "history.txt"
#define USER_DENY "user.deny"



/* These are for Linux user/pass logons.  They define the group used by
   /etc/group .   You must have these groups and users defined in /etc/group.
   
   The "tnc" group defines which users are allowed direct access to control the TNC.
   The "aprs"  group defines users which log on with a Linux user/password
   instead of the Mac/WinAPRS automatic password. These users are allowed to
   insert data into the data stream.

   To add a group logon as root and edit the /etc/group file by adding
	a line such as: tnc::102:root,wa4dsy,bozo   */

#define APRSGROUP "aprs"
#define TNCGROUP "tnc"


/* this is not used in production code*/
#define TEST "WA4DSY>APRS,WIDE:!3405.31N/08422.46WyWA4DSY APRS Internet Server running on Linux.\r\n"




