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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include <sys/time.h>                   // gettimeofday
#include <stdio.h>
#include <fstream.h>                    // ifstream
#include <pwd.h>                        // pwd
#include <crypt.h>                      // crypt
#include <grp.h>                        // getgrnam
}

#include "constant.h"
#include "utils.h"
#include "cpqueue.h"

using namespace std;


int ttlDefault = 30;                    // Default time to live of a history item (30 minutes)
int CountDefault = 7;                   // Max of 7 instances of one call sign in history list
extern cpQueue conQueue;
extern bool ConvertMicE;


//----------------------------------------------------------------------
int WriteLog(const char *pch, const char *LogFile)
{
    FILE *f;
    time_t ltime;
    char szTime[40];
    char *p;
    int rc;
    static pthread_mutex_t* pmtxLog;    // Mutual exclusion semi for WriteLog function
    static bool logInit = false;

    char *cp = strdup(pch);             // Make local copy of input string.

    if (!logInit) {
        pmtxLog = new pthread_mutex_t;
        pthread_mutex_init(pmtxLog,NULL);
        logInit = true;
        cout << "logger initialized\n";
    }
    pthread_mutex_lock(pmtxLog);

    char *pLogFile = new char[strlen(LOGPATH) + strlen(LogFile) +1];
    memset(pLogFile, NULLCHR, sizeof(&pLogFile));
    strcpy(pLogFile,LOGPATH);
    strcat(pLogFile,LogFile);

    f = fopen(pLogFile,"a");

    if (f == NULL)
        f = fopen(pLogFile,"w");

    if (f == NULL) {
        cerr << "failed to open " << pLogFile << endl;
        rc = -1;
    } else {
        char *eol = strpbrk(cp,"\n\r");

        if (eol)
            *eol = '\0';                // remove crlf

        time(&ltime) ;                  // Time Stamp
        ctime_r(&ltime,szTime);         // Thread safe ctime()

        p = strchr(szTime,(int)'\n');

        if (p)
            *p = ' ';                   // convert new line to a space

        fprintf(f,"%s %s\n",szTime,cp); // Write log entry with time stamp
        fflush(f);
        fclose(f);
        rc = 0;
    }
    delete cp;
    delete pLogFile;
    pthread_mutex_unlock(pmtxLog);

    return(rc);
}


//------------------------------------------------------------------------
//Convert all lower case characters in a string to upper case.
// Assumes ASCII chars.
//
char * strupr(char *cp)
{
    int i;
    int l = strlen(cp);

    for (i=0;i<l;i++) {
        if ((cp[i] >= 'a') && (cp[i] <= 'z'))
            cp[i] = cp[i] - 32;
    }
    return(cp);
}

//-----------------------------------------------------------------------
void printhex(char *cp, int n)
{
    for (int i=0;i<n;i++)
        printf("%02X ",cp[i]);

    printf("\n");
}

//---------------------------------------------------------------------
//return TRUE if destination of packet matches "ref"
//This is for filtering out unwanted packets
bool CmpDest(const char *line, const char *ref)
{
    bool rv = false;
    char *cp = new char[strlen(ref)+3];
    memset(cp, NULLCHR, sizeof(&cp));
    strcpy(cp,">");
    strcat(cp,ref);
    strcat(cp,",");

    if (strstr(line,cp) !=	NULL)
        rv = true;

    delete[] cp;
    return(rv);
}
//----------------------------------------------------------------------
//Return TRUE if any string in the digi path matches "ref".
//
bool CmpPath(const char *line, const char *ref)
{
    bool rv = false;
    char *cp = new char[strlen(line)+1];
    memset(cp, NULLCHR, sizeof(&cp));
    strcpy(cp, line);
    char *path_end = strchr(cp,':');    // find colon

    if (path_end != NULL) {
        *path_end = '\0';               // replace colon with a null
        if (strstr(cp,ref) != NULL)
            rv = true;
    }

    delete[] cp;
    return(rv);
}

//---------------------------------------------------------------------
//Returns true if "call" matches first string in "s"
//"call" must be less than 15 chars long.
//
bool callsign(char *s, const char *call)
{
    char cp[17];

    if (strlen(call) > 14)
        return false;

    strncpy(cp,call,16);
    strncat(cp,">",16);
    char *ss = strstr(s,cp);

    if (ss != NULL)
        return true;
    else
        return false;
}


//---------------------------------------------------------------------
//Compares two packets and returns TRUE if the source call signs are equal
//
bool CompareSourceCalls(char *s1, char *s2)
{
    char call[12];
    strncpy(call,s2,10);
    char *eos = strchr(call,'>');

    if (eos != NULL)
        eos[0] = '\0';
    else
        return false;

    return(callsign(s1,call));
}

//---------------------------------------------------------------------
// This sets the time-to-live and max count values for each packet received.
//
void GetMaxAgeAndCount( int *MaxAge, int *MaxCount)
{
    *MaxAge = ttlDefault;
    *MaxCount = CountDefault;
}


//------------------------------------------------------------------------
/* Checks for a valid user/password combination from the
   Linux etc/passwd file .  Returns ZERO if user/pass is valid.
   This doesn't work with shadow passwords.

   THIS IS NOT USED ANYMORE AND MAY NOT BE THREAD SAFE
*/
int  checkpass(const char *szUser, const char *szGroup, const char *szPass)
{
    passwd *ppw;
    group *pgrp;
    char *member ;
    int i;
    char salt[3];
    int usrfound = 0 ;
    int rc = BADGROUP;

    //cout << szUser << " " << szPass << " " << szGroup << endl;  //debug

    pgrp = getgrnam(szGroup);		  /* Does group name szGroup exist? */

    if (pgrp == NULL)
        return rc;	  /* return BADGROUP if not */

    ppw = getpwnam(szUser);			  /* get the users password information */

    if (ppw == NULL)
        return BADUSER ; /* return BADUSER if no such user */

    i = 0;

    /* find out if user is a member of szGroup */
    while(((member = pgrp->gr_mem[i++]) != NULL) && (usrfound == 0 )) {
        //cerr << member << endl;	 //debug code
        if (strcmp(member,szUser) == 0)
            usrfound = 1;
    }

    if (usrfound == 0)
        return BADGROUP;    /* return BADGROUP if user not in group */

    /* check the password */

    strncpy(salt,ppw->pw_passwd,2);
    salt[2] = '\0';

    if (strcmp(crypt(szPass,salt), ppw->pw_passwd) == 0 )
        rc = 0;
    else
        rc = BADPASSWD;

    return rc;
}


//--------------------------------------------------------------------
//Removes all control codes ( < 1Ch ) from a string and set the 8th bit to zero
//
void RemoveCtlCodes(char *cp)
{
    int i,j;
    int len = strlen(cp);
    unsigned char *ucp = (unsigned char*)cp;
    unsigned char *temp = new unsigned char[len+1];

    for (i=0, j=0; i<len; i++) {
        ucp[i] &= 0x7f;                 // Clear 8th bit
        if (ucp[i]  >= 0x1C)            // Check for printable plus the Mic-E codes
            temp[j++] = ucp[i];         // copy to temp if printable

    }

    temp[j] = ucp[i];                   // copy terminating NULL
    strcpy(cp,(char*)temp);             // copy result back to original
    delete[] temp;
    delete[] ucp;
}


void makePrintable(char *cp) {
    int i,j;
    int len = (int)strlen(cp);
    unsigned char *ucp = (unsigned char *)cp;
    unsigned char *temp = new unsigned char[len+1];

    for (i=0, j=0; i<=len; i++) {
        ucp[i] &= 0x7f;                 // Clear 8th bit
        if ( ((ucp[i] >= (unsigned char)0x20) && (ucp[i] <= (unsigned char)0x7e))
              || ((char)ucp[i] == '\0') )     // Check for printable or terminating 0
            ucp[j++] = ucp[i] ;        // Copy to (possibly) new location if printable
    }

    ucp[i++] = 0x0d;                    // Put a CR-LF on the end of the buffer
    ucp[i++] = 0x0a;
    ucp[i++] = 0;

    temp[j] = ucp[i];                   // copy terminating NULL
    strcpy(cp,(char*)temp);             // copy result back to original
    delete[] temp;
    delete[] ucp;
}

void removeHTML(string& sp) {
    string search("/<>");

    unsigned int p = sp.find_first_of(search);
    while (p < sp.length()) {
        sp.replace(p, 1, "*");
        p = sp.find_first_of(search, p + 1);
    }
}


//---------------------------------------------------------------------
/*
char* StripPath(char* cp)
{
   char *p = strchr(cp,':');       //Find the colon and strip off path info
   if (p != NULL){
   p++;                          //move pointer beyond colon
   strcpy(cp,p);                //move data back to start of cp
   }
   return cp;

}
*/
//---------------------------------------------------------------------

/*returns the number if instances of char "c" in string "s". */
int freq( string& s, char c)
{
    int count=0;
    int len = s.length();

    for (int i=0;i<len;i++)
        if(s[i] == c)
            count++;

    return count;
}


//----------------------------------------------------------------------------
//
int split( string& s, string sa[],  int saSize,  const char* delim)
{
    int wordcount;
    unsigned start,end;

    start = s.find_first_not_of(delim);     // find first token
    end = 0;
    wordcount = 0;

    while ((start != string::npos) && (wordcount < saSize)) {
        end = s.find_first_of(delim,start+1);

        if (end == string::npos)
            end = s.length();

        sa[wordcount++] = s.substr(start,end-start);

        start = s.find_first_not_of(delim,end+1);
    }
    return(wordcount);
}

//----------------------------------------------------------------------------
//
void upcase(string& s)
{
    for (unsigned i=0; i< s.length(); i++)
        s[i] = toupper(s[i]);
}



//---------------------------------------------------------------------
//
void reformatAndSendMicE(TAprsString* inetpacket, cpQueue& sendQueue)
{
    //WriteLog(inetpacket->getChar(),"mic_e.log");

    if (ConvertMicE) {
        TAprsString* posit = NULL;
        TAprsString* telemetry = NULL;
        inetpacket->mic_e_Reformat(&posit,&telemetry);

        if (posit)
            sendQueue.write(posit);          //Send reformatted packets

        if (telemetry)
            sendQueue.write(telemetry);

        delete inetpacket; //Note: Malformed Mic_E packets that failed to convert are discarded
    } else
        sendQueue.write(inetpacket);  //Send raw Mic-E packet
}

//--------------------------------------------------------------------
//  Return TRUE if string s is in the list cl.
//
bool find_rfcall(const string& s, string **cl)
{
    bool rc = false;
    int i = 0, pos;

    while ((cl[i] != NULL) && (rc == false)) {
        pos = (cl[i])->find('*');
        if ((pos != 0) && (s.substr(0, pos).compare(cl[i]->substr(0, pos)) == 0))
            rc = true;

        i++;
    }
    return(rc);
}

//------------------------------------------------------------------
//
//  Case insensitive c char string compare function
//
int stricmp(const char* szX, const char* szY)
{
    int i;
    int len = strlen(szX);
    char* a = new char[len+1];

    for (i = 0;i<len;i++)
        a[i] = tolower(szX[i]);

    a[i]='\0';
    len = strlen(szY);
    char* b = new char[len+1];

    for (i = 0;i<len;i++)
        b[i] = tolower(szY[i]);

    b[i]='\0';

    int rc = strcmp(a,b);
    delete[] a;
    delete[] b;
    return(rc);
}

//--------------------------------------------------------------------
/* Returns the deny code if user found or "+" if user not in list
   Deny codes:  L = no login   R = No RF access
   Note: ssid suffix on call sign is ignored */

char checkUserDeny(string& user)
{
    const int maxl = 80;
    const int maxToken=32;
    int nTokens ;

    char Line[maxl];
    string baduser;
    char rc = '+';

    ifstream file(USER_DENY);

    if (!file)
        return rc;

    string User = string(user);
    unsigned i = user.find("-");

    if (i != string::npos)
        User = user.substr(0,i);  //remove ssid

    do {
        file.getline(Line,maxl);        // Read each line in file

        if (!file.good())
            break;

        if (strlen(Line) > 0) {
            if (Line[0] != '#') {       // Ignore comments
                string sLine(Line);
                string token[maxToken];
                nTokens = split(sLine, token, maxToken, RXwhite);  //Parse into tokens
                upcase(token[0]);
                baduser = token[0];

                if ((stricmp(baduser.c_str(),User.c_str()) == 0) && (nTokens >= 2)){
                    rc = token[1][0];
                    break;
                }
            }
        }
    } while(file.good());

    file.close();
    return(rc);
}

//------------------------------------------------------------------------
/* return ascii H:M:S string with elapsed time ( NOW - starttime) */
//
void strElapsedTime(time_t starttime,  char* timeStr)
{
    if (starttime == -1) {
        sprintf(timeStr,"N/A");
        return;
    }
    time_t endtime = time(NULL);
    double  dConnecttime = difftime(endtime , starttime);
    int iMinute = (int)(dConnecttime / 60);
    iMinute = iMinute % 60;
    int iHour = (int)dConnecttime / 3600;
    int iSecond = (int)dConnecttime % 60;
    sprintf(timeStr, "%3d:%02d:%02d", iHour, iMinute, iSecond);

}

//--------------------------------------------------------------------
/* Return TRUE if callsign s1 matches callsign s2, where s2 can include wildcards */
bool matchCallsign(const string& s1, const string& s2)
{
   int pos;

   // Try a straight out comparison
   if (s1.compare(s2) == 0)
       return true;

   // Else look for a wildcard
   else if (((pos = s2.find('*')) != 0) &&
       (s1.substr(0, pos).compare(s2.substr(0, pos)) == 0))
       return true;

   // Else failed
   else
       return false;
}

//---------------------------------------------------------------------
void reliable_usleep (int usecs)
{
    timeval now, end;

    gettimeofday (&now, NULL);
    end = now;
    end.tv_sec  += usecs / 1000000;
    end.tv_usec += usecs % 1000000;

    while ((now.tv_sec < end.tv_sec) || ((now.tv_sec == end.tv_sec) && (now.tv_usec < end.tv_usec))) {
        timeval tv;
        tv.tv_sec = end.tv_sec - now.tv_sec;

        if (end.tv_usec >= now.tv_usec)
            tv.tv_usec = end.tv_usec - now.tv_usec;
        else {
            tv.tv_sec--;
            tv.tv_usec = 1000000 + end.tv_usec - now.tv_usec;
        }

        select(0, NULL, NULL, NULL, &tv);
        gettimeofday (&now, NULL);
    }
}




// eof: utils.cpp
