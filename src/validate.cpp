/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2001 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001 aprsd Dev Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

/* validate.cpp version 2  May 23 1999

 Put in thread-safe verisons of getgrnam and getpwnam, vers 2.1.2 Jun 22 2000
*/

//#define DEBUG 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <crypt.h>
#include <stdio.h>
#include <iostream.h>
#include <strstream.h>
#include <iomanip.h>
#include <pwd.h>
#include <grp.h>


#include <shadow.h>


#include "validate.h"

/* Steves servers response to a user logon:

    APRSERV>APRS,TCPIP*:USERLIST :Verified user KE3XY-4 logged on using WinAPRS 2.1.7.{3544
    
    APRSERV>APRS,TCPIP*:USERLIST :Unverified user KE3XY-4 logged on using WinAPRS 2.1.7.{3544

    
 Note:  The doHash(char*) function is Copyright Steve Dimse 1998    
*/
    
short doHash(const char *theCall);

    
#define kKey 0x73e2		// This is the key for the data


//-----------------------------------------------------------------------------------

int  checkSystemPass(const char *szUser, const char *szPass, const char *szGroup)
{
	passwd *ppw = NULL;
	group *pgrp = NULL;
   spwd *pspwd = NULL;
	char *member = NULL;
   struct group grp;
   struct passwd pwd;
	int i;
	char salt[16];
	int usrfound = 0 ;
   int pwLength = 0;
	int rc = BADGROUP;
   

#ifdef DEBUG
   cout << szUser << " " << szPass << " " << szGroup << endl;  //debug
#endif

  
   size_t bufsize = sysconf(_SC_GETGR_R_SIZE_MAX);
   char *buffer1 = new char[bufsize];
   //Thread-Safe getgrnam()
   getgrnam_r(szGroup,         /* Does group name szGroup exist? */
              &grp,
              buffer1,
              bufsize,
              &pgrp);


   
   			  
	if (pgrp == NULL) {
      delete buffer1;
      return rc;	  /* return BADGROUP if not */
   }

   
   bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
   
   char *buffer2 = new char[bufsize];
   //Thread-Safe getpwnam()
   getpwnam_r(szUser,
            &pwd,
            buffer2,
            bufsize,
            &ppw);

   
  
	if (ppw == NULL){ 
      delete buffer2;
      delete buffer1;
      return BADUSER ; /* return BADUSER if no such user */
   }
   
	i = 0;
		
	/* find out if user is a member of szGroup */
	while(((member = pgrp->gr_mem[i++]) != NULL) && (usrfound == 0 ))
		{	
 #ifdef DEBUG
         cerr << member << endl;	 //debug code
 #endif
			if(strcmp(member,szUser) == 0)  usrfound = 1	;
		}
	
	if(usrfound == 0){ 
      delete buffer1;
      delete buffer2;
      return BADGROUP;	 /* return BADGROUP if user not in group */
   }

   /* check the password */

#ifdef DEBUG
   cout << ppw->pw_passwd << endl
      << crypt(szPass,salt) << endl;
#endif

   pwLength = strlen(ppw->pw_passwd);

	if(ppw->pw_passwd[0] != '$'){
      /* DES salt */
	   strncpy(salt,ppw->pw_passwd,2);
	   salt[2] = '\0';
   }else
     {   /* MD5 salt */
         int i;
         for(i=0;i<3;i++) 
            if(i < pwLength) salt[i] = ppw->pw_passwd[i];

         while((i < 14) && (ppw->pw_passwd[i] != '$')) 
            salt[i++] = ppw->pw_passwd[i];

         salt[i++] = '$';
         salt[i] = '\0';
         
   }

   #ifdef DEBUG
   cout << "salt=" << salt << endl;
   #endif

   
   if (strcmp(crypt(szPass,salt), ppw->pw_passwd) == 0 ) 
		rc = 0; 
		else 
			rc = BADPASSWD;
	
   if ((rc == BADPASSWD) && (strcmp("x",ppw->pw_passwd) == 0)) {
#ifdef DEBUG
      cout << "Shadow passwords enabled\n";
#endif
      pspwd = getspnam(szUser);  //Get shadow password file data for user
      if (pspwd == NULL) {
         cout << "validate: Can't read shadowed password file.  This program must run as root\n";
         delete buffer1;
         delete buffer2;
         return MUSTRUNROOT;
      }

      pwLength = strlen(pspwd->sp_pwdp);

#ifdef DEBUG
      cout << "pw=" << pspwd->sp_pwdp << endl;
#endif

      if(pspwd->sp_pwdp[0] != '$'){
         /* DES salt */
         strncpy(salt,pspwd->sp_pwdp,2);
	      salt[2] = '\0';
      }else
         {     /* MD5 salt */
         int i;
         for(i=0;i<3;i++) 
            if(i < pwLength) salt[i] = pspwd->sp_pwdp[i];

         while((i < 14) && (pspwd->sp_pwdp[i] != '$')) 
            salt[i++] = pspwd->sp_pwdp[i];

         salt[i++] = '$';
         salt[i] = '\0';
         
   }

 #ifdef DEBUG
   cout << "salt=" << salt << endl;

#endif



      if (strcmp(crypt(szPass,salt), pspwd->sp_pwdp) == 0 ) 
      rc = 0; 
		else 
			rc = BADPASSWD;

#ifdef DEBUG
      cout  << pspwd->sp_pwdp 
            << " :  " 
            << crypt(szPass,salt) 
            << "  :  " 
            << szPass 
            << endl;
#endif

   }
   delete buffer1;
   delete buffer2;
	return rc;



}


//------------------------------------------------------------------------------------

bool validate(const char* user,const char* pass, const char* group, bool allow_hash)
{
   
  
   if(allow_hash && (strcmp("tnc",group) != 0)){  //Don't allow tnc users to use aprs numerical password
                                            //"allow_hash" must be TRUE to use the HASH test.
                                    //If allow_hash is false only the Linux user/pass validation is used
      short iPass = atoi(pass);

      if (iPass == -1) return BADUSER;        // -1 is known to be not registered
      if(strlen(user) <= 9){                  // Limit user call sign to 9 chars or less (2.1)
      if (doHash(user) == iPass) return 0;    // return Zero if hash test is passed
      }
   }
                                               //if hash fails, test for
   int rc = checkSystemPass(user,pass,group); // valid  Linux system user/pass

#ifdef DEBUG
   cout << "checkSystemPass returned: " << rc << endl;
#endif

  if (rc == BADPASSWD) sleep(10);
  return rc;   
  
  
}

//-----------------------------------------------------------------------------------

/* As of April 11 2000 Steve Dimse has released this code to the open
source aprs community */

short doHash(const char *theCall)
{
	char 			rootCall[10];			// need to copy call to remove ssid from parse
	char 			*p1 = rootCall;
	
	while ((*theCall != '-') && (*theCall != 0)) *p1++ = toupper(*theCall++);
	*p1 = 0;
	
	short hash = kKey;			// Initialize with the key value
	short i = 0;
	short len = strlen(rootCall);
	char *ptr = rootCall;
	while (i<len)				// Loop through the string two bytes at a time
	{
		hash ^= (*ptr++)<<8;	// xor high byte with accumulated hash
		hash ^= (*ptr++);		// xor low byte with accumulated hash
		i += 2;
	}

   
	return hash & 0x7fff;		// mask off the high bit so number is always positive
}

//--------------------------------------------------------------------------------------
//End of file



   




