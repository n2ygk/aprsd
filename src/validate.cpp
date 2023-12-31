/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-2002 aprsd Dev Team
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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <cctype>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <strstream>
#include <iomanip>

using namespace std;

#ifdef USE_PAM
#include <security/pam_appl.h>
#include <security/pam_misc.h>

struct user_info {
   const string username;
   const string password;
   const string group;
};
#else
#include <crypt.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#endif

#include "validate.hpp"

using namespace std;


/*
    Steves servers response to a user logon:

    APRSERV>APRS,TCPIP*:USERLIST :Verified user KE3XY-4 logged on using WinAPRS 2.1.7.{3544

    APRSERV>APRS,TCPIP*:USERLIST :Unverified user KE3XY-4 logged on using WinAPRS 2.1.7.{3544


    Note:  The doHash(char*) function is Copyright Steve Dimse 1998
*/

short doHash(const char *theCall);

#define kKey 0x73e2		// This is the key for the data


//-----------------------------------------------------------------------------------
#ifdef USE_PAM
static int my_conv(int num_msg, const struct pam_message **msg,
        struct pam_response **response, void *data)
{
    struct user_info *userInfo = (struct user_info *)data;
    struct pam_response *reply;

    reply = (struct pam_response *)malloc(num_msg * sizeof(struct pam_response));

    if (reply == NULL)
        return PAM_CONV_ERR;

    for (int count = 0; count < num_msg; count++) {
        reply[count].resp_retcode = 0;
        reply[count].resp = NULL;

        switch (msg[count]->msg_style) {
            case PAM_PROMPT_ECHO_ON:
                reply[count].resp = strdup(userInfo->username);
                break;

            case PAM_PROMPT_ECHO_OFF:
                reply[count].resp = strdup(userInfo->password);
                break;

            default:
                for (int i = 0; i < count; i++)
                    if (reply[i].resp != NULL)
                        free(reply[i].resp);

                free(reply);
                return PAM_CONV_ERR;
        }
    }

    *response = reply;
    return PAM_SUCCESS;
}
#endif


//-----------------------------------------------------------------------------------

int  checkSystemPass(const string szUser, const string szPass, const string szGroup)
{
#ifdef USE_PAM
    struct user_info userInfo;
    struct pam_conv conv = { my_conv, (void *)&userInfo };
    pam_handle_t *pamh = NULL;

    userInfo.username = szUser;
    userInfo.password = szPass;
    userInfo.group = szGroup;

    if (pam_start("aprsd", szUser, &conv, &pamh) != PAM_SUCCESS)
        return BADUSER;

    if (pam_authenticate(pamh, 0) != PAM_SUCCESS)
        return BADPASSWD;

    if (pam_acct_mgmt(pamh, 0) != PAM_SUCCESS)
        return BADUSER;

    pam_end(pamh, PAM_SUCCESS);
    return 0;
#else
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
    getgrnam_r(szGroup.c_str(),         /* Does group name szGroup exist? */
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
    getpwnam_r(szUser.c_str(),
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
    while(((member = pgrp->gr_mem[i++]) != NULL) && (usrfound == 0 )) {
#ifdef DEBUG
        cerr << member << endl; //debug code
 #endif
        if (strcmp(member, szUser.c_str()) == 0)
            usrfound = 1	;
    }

    if (usrfound == 0) {
        delete buffer1;
        delete buffer2;
        return BADGROUP;	 /* return BADGROUP if user not in group */
    }

    /* check the password */

#ifdef DEBUG
    cout << ppw->pw_passwd << endl
        << crypt(szPass.c_str(),salt) << endl;
#endif

    pwLength = strlen(ppw->pw_passwd);

    if (ppw->pw_passwd[0] != '$') {
        /* DES salt */
        strncpy(salt,ppw->pw_passwd,2);
        salt[2] = '\0';
    } else {   /* MD5 salt */
        int i;
        for (i = 0; i < 3; i++)
            if (i < pwLength)
                salt[i] = ppw->pw_passwd[i];

        while ((i < 14) && (ppw->pw_passwd[i] != '$'))
            salt[i++] = ppw->pw_passwd[i];

        salt[i++] = '$';
        salt[i] = '\0';
    }
#ifdef DEBUG
    cout << "salt=" << salt << endl;
#endif

    if (strcmp(crypt(szPass.c_str(), salt), ppw->pw_passwd) == 0 )
        rc = 0;
    else
        rc = BADPASSWD;

    if ((rc == BADPASSWD) && (strcmp("x",ppw->pw_passwd) == 0)) {
#ifdef DEBUG
        cout << "Shadow passwords enabled\n";
#endif
        pspwd = getspnam(szUser.c_str());  //Get shadow password file data for user
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
        if (pspwd->sp_pwdp[0] != '$') {
            /* DES salt */
            strncpy(salt,pspwd->sp_pwdp,2);
            salt[2] = '\0';
        } else {     /* MD5 salt */
            int i;
            for (i = 0; i < 3; i++) 
                if (i < pwLength) 
                    salt[i] = pspwd->sp_pwdp[i];

            while ((i < 14) && (pspwd->sp_pwdp[i] != '$')) 
                salt[i++] = pspwd->sp_pwdp[i];

            salt[i++] = '$';
            salt[i] = '\0';
        }

#ifdef DEBUG
        cout << "salt=" << salt << endl;
#endif
        if (strcmp(crypt(szPass.c_str(), salt), pspwd->sp_pwdp) == 0 ) 
            rc = 0; 
        else 
            rc = BADPASSWD;

#ifdef DEBUG
        cout  << pspwd->sp_pwdp 
            << " :  " 
            << crypt(szPass.c_str(), salt) 
            << "  :  " 
            << szPass 
            << endl;
#endif
    }
    delete buffer1;
    delete buffer2;
    return rc;
#endif
}


//------------------------------------------------------------------------------------

int validate(const string& user, const string& pass, const string& group, bool allow_hash)
{
	
	cerr << "validate: user == " << user << " pass == " << pass << " group == " << group << endl;
	
    if (allow_hash && (group.compare("tnc") != 0)) {    // Don't allow tnc users to
                                                        // use aprs numerical password
                                                        // "allow_hash" must be TRUE
                                                        // to use the HASH test.
                                                        // If allow_hash is false only
                                                        // the Linux user/pass
                                                        // validation is used
        short iPass = atoi(pass.c_str());
		cerr << "validate: iPass == " << iPass << endl;
		
        if (iPass == -1) {
			cerr << "validate: baduser " << endl;
            return BADUSER;        // -1 is known to be not registered
		}

        if (user.size() <= 9) {                  // Limit user call sign to 9 chars or less (2.1)
            if (doHash(user.c_str()) == iPass) {
				cerr << "validate: hash succeeded, user passed" << endl;
                return 0;    // return Zero if hash test is passed
			}
        }
    }
    cerr << "validate: hash failed" << endl;
													//if hash fails, test for
    int rc = checkSystemPass(user, pass, group); 	// valid  Linux system user/pass

//#ifdef DEBUG
    cerr << "checkSystemPass returned: " << rc << endl;
//#endif

    if (rc == BADPASSWD)
        sleep(10);

    return rc;
}

//-----------------------------------------------------------------------------------

/*
    As of April 11 2000 Steve Dimse has released this code to the open
    source aprs community
*/

short doHash(const char* theCall)
{
    int retVal = 0;
	char rootCall[10];      // need to copy call to remove ssid from parse
    char *p1 = rootCall;

	cerr << "doHash: theCall == " << theCall << endl;
	
    while ((*theCall != '-') && (*theCall != 0)) *p1++ = toupper(*theCall++);
        *p1 = 0;
	
	cerr << "doHash: p1 == " << p1 << endl;
	
		
    short hash = kKey;      // Initialize with the key value
    short i = 0;
    short len = strlen(rootCall);
    char *ptr = rootCall;

    while (i < len) {           // Loop through the string two bytes at a time
        hash ^= (*ptr++) << 8;  // xor high byte with accumulated hash
        hash ^= (*ptr++);       // xor low byte with accumulated hash
        i += 2;
    }
	retVal = hash & 0x7fff;
	cerr << "doHash retVal == " << retVal << endl;
    return retVal;       // mask off the high bit so number is always positive
}
