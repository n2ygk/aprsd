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

#include <cstdio>
#include <string>
#include <fstream>                    // ifstream
#include <iostream>
#include <strstream>
#include <iomanip>

#include "utils.h"
#include "aprsd.h"
#include "cpqueue.h"
#include "mic_e.h"

using namespace std;
using namespace aprsd;

int CountDefault = 7;		   //Max of 7 instances of one call sign in history list

static Mutex pmtxLog;


int WriteLog(const string& sp, const string& LogFile)
{
    time_t ltime;
    char szTime[40];
    string newTime;
    static Lock locker(pmtxLog, false);

    locker.get();

    ofstream ofs(LogFile.c_str(), ios::out | ios::app);

    if (!ofs)
        cerr << "File I/O Error: Unable to open/create file " << LogFile << endl;

    time(&ltime);                                       // Timestamp
    ctime_r(&ltime, szTime);                            // "threadsafe" ctime
    newTime = string(szTime);
    newTime = newTime.substr(0, newTime.length()-1);    // Strip '\n'

    ofs << newTime << "  " << sp << endl;

    if (ofs.is_open())
        ofs.close();

    locker.release();
    return 0;
}

//----------------------------------------------------------------------
int WriteLog(const char* pch, const char* LogFile)
{
    FILE* f;
    time_t ltime;
    char szTime[40];
    char* p;
    int rc;
    static pthread_mutex_t* pmtxLog; //Mutual exclusion semi for WriteLog function
    static bool logInit = false;

    char* cp = strdup(pch);   //Make local copy of input string.

    if (!logInit) {
        pmtxLog = new pthread_mutex_t;
        pthread_mutex_init(pmtxLog,NULL);
        logInit = true;
    }

    pthread_mutex_lock(pmtxLog);

    char* pLogFile = new char[CONFPATH.length() + strlen(LogFile) +1];
    strcpy(pLogFile,CONFPATH.c_str());
    strcat(pLogFile, LogFile);

    f = fopen(pLogFile, "a");

    if (f == NULL)
        f = fopen(pLogFile, "w");

    if (f == NULL) {
        cerr << "failed to open " << pLogFile << endl;
        rc = -1;
    } else {
        char *eol = strpbrk(cp,"\n\r");
        if (eol)
            *eol = '\0';              //remove crlf

        time(&ltime) ;                    //Time Stamp
        ctime_r(&ltime,szTime);          //Thread safe ctime()

        p = strchr(szTime, (int)'\n');

        if (p)
            *p = ' ';               //convert new line to a space

        fprintf(f,"%s %s\n", szTime, cp);	 //Write log entry with time stamp
        fflush(f);
        fclose(f);
        rc = 0;
    }
    delete cp;
    delete pLogFile;
    pthread_mutex_unlock(pmtxLog);

    return rc;
}

//------------------------------------------------------------------------

void removeHTML(string& sp) 
{
    string search("/<>");

    unsigned int p = sp.find_first_of(search);
    
    while (p < sp.length()) {
        sp.replace(p, 1, "*");
        p = sp.find_first_of(search, p + 1);
    }
}

//------------------------------------------------------------------------
//Convert all lower case characters in a string to upper case.
// Assumes ASCII chars.

char* strupr(char *cp)
{  
    int i;
    int l = strlen(cp);
    
    for (i = 0; i < l; i++) {
        if ((cp[i] >= 'a') && (cp[i] <= 'z')) 
            cp[i] = cp[i] - 32;
    }
   
    return cp;
}

//-----------------------------------------------------------------------
void printhex(char *cp, int n)
{
    for (int i = 0; i < n; i++) 
        printf("%02X ",cp[i]);

    printf("\n");
}

//---------------------------------------------------------------------
//return true if destination of packet matches "ref"
//This is for filtering out unwanted packets
bool CmpDest(const char *line, const char *ref)
{
    bool rv = false;
    char *cp = new char[strlen(ref)+3];
    strcpy(cp,">");
    strcat(cp,ref);
    strcat(cp,",");
    
    if (strstr(line,cp) !=	NULL) 
        rv = true;
    
    delete cp;
    return rv;
}
//----------------------------------------------------------------------

//Return true if any string in the digi path matches "ref".

bool CmpPath(const char *line, const char *ref)
{
    bool rv = false;
    char *cp = new char[strlen(line)+1];
    strcpy(cp,line);
    char *path_end = strchr(cp,':');          //find colon
   
    if (path_end != NULL) {
        *path_end = '\0';             //replace colon with a null
        if (strstr(cp,ref) != NULL) 
            rv = true;
    }

    delete cp;
    return rv;
}

//---------------------------------------------------------------------
//Returns true if "call" matches first string in "s"
//"call" must be less than 15 chars long.
bool callsign(char *s, const char *call)
{
    char cp[17];
    if (strlen(call) > 14) 
        return false;
    
    strncpy(cp,call,14);
    strcat(cp,">");
    char *ss = strstr(s,cp);
    
    if (ss != NULL) 
        return true ;
    else 
        return false;
}
//---------------------------------------------------------------------
//Compares two packets and returns true if the source call signs are equal
bool CompareSourceCalls(char *s1, char *s2)
{
    char call[12];
    strncpy(call,s2,10);
    char *eos = strchr(call,'>');
    
    if (eos != NULL) 
        eos[0] = '\0'; 
    else 
        return false;

    return callsign(s1,call);
}

//---------------------------------------------------------------------
// This sets the time-to-live and max count values for each packet received.

void GetMaxAgeAndCount(int *MaxAge, int *MaxCount)
{
    *MaxAge = ttlDefault;
    *MaxCount = CountDefault;
}


//--------------------------------------------------------------------
//Removes all control codes ( < 1Ch ) from a string
void RemoveCtlCodes(char *cp)
{
    int i,j;
    int len = strlen(cp);
    unsigned char *ucp = (unsigned char*)cp;
    unsigned char *temp = new unsigned char[len+1];

    for(i = 0, j = 0; i < len; i++) {
        //ucp[i] &= 0x7f;      //Clear 8th bit    <-- removed in version 2.1.5 for 8 bit char sets
        if (ucp[i]  >= 0x1C) { //Check for printable plus the Mic-E codes
            temp[j++] = ucp[i] ;   //copy to temp if printable
        }
    }

    temp[j] = ucp[i];  //copy terminating NULL
    strcpy(cp,(char*)temp);  //copy result back to original
    delete temp;
}

//---------------------------------------------------------------------

/*returns the number if instances of char "c" in string "s". */
int freq( string& s, char c)
{
    int count=0;
    int len = s.length();
    
    for (int i = 0; i < len; i++) 
        if(s[i] == c) 
            count++;

    return count;
}

//----------------------------------------------------------------------------

//returns the number of tokens delimited by 'delim'.
//returns zero if error.
//Each token copied into the 'sa' array.
//Stops when no more delimiters are found or saSize is reached.

int split( string& s, string sa[],  int saSize,  const char* delim)
{
    int wordcount;
    unsigned start,end;

    if (delim == NULL)  
        return 0;

    try {
        start = s.find_first_not_of(delim);  //find first token
        end = 0;
        wordcount = 0;
        while ((start != string::npos) && (wordcount < saSize)) {  
            end = s.find_first_of(delim,start+1);
            if (end == string::npos) 
                end = s.length();

            sa[wordcount++] = s.substr(start,end-start);

            start = s.find_first_not_of(delim,end+1);
        }

    }
    catch(exception& error) {
        WriteLog(string("split function error"), ERRORLOG);
        WriteLog(s, ERRORLOG);
        return 0;
    }
    //if(sa[wordcount-1].length() == 0) wordcount--;
    return wordcount;
}

//----------------------------------------------------------------------------

void upcase(string& s)
{
    for (unsigned i = 0; i < s.length(); i++)
        s[i] = toupper(s[i]);
}



//---------------------------------------------------------------------

#define MSB 0x8000
#define MASK 0x8005

void crc_byte(const char data, unsigned int *crc16)
{
    int k;
    unsigned c,d ;

    c = data << 8 ;
    d = c;

    for (k = 0; k < 8; k++) {
        *crc16 = (c & MSB) ^ *crc16;

        if (*crc16 & MSB) {
            *crc16 = *crc16 << 1;
            *crc16 = *crc16 ^ MASK;
        } else
            *crc16 = *crc16 << 1;

        d = d << 1;
        c = d;
    }
}

//--------------------------------------------------------------------
unsigned int string_hash(const string& s)
{
    int i,j;
    unsigned int hash = 0xffff;
    j = s.length();
    string work = s;
    upcase(work);   //Convert to upper case
   
    for(i = 0; i < j; i++) 
        crc_byte(work[i], &hash);

    return hash;
}

//------------------------------------------------------------------
//Create an alias from a domain name.  Use up to 6 characters
//or until the first "." is encountered then append a 3 char hash code.
//eg: third.aprs.net becomes thirdFCA

void makeAlias(string& s)
{ 
    unsigned hash;
    char shash[5];
    int i;

    hash = string_hash(s);           //Get hash value for input string
    hash &= 0xfff;                   //Limit to 12 bits
    s = s.substr(0,6);               //use up to 6 characters of input string
    i = s.find(".",0);               // up to first "." or 6 characters max.
    
    if (i <= 5) 
        s = s.substr(0,i);
 
    sprintf(shash,"%03X",hash);      //Convert int to hex string
    s = s + shash;                   //Append hex string to truncated original string
}


//--------------------------------------------------------------------
/* Return true if string s is in the list cl. 
   Added wild card support in version 2.1.5 . Thank you VK3SB  */

bool find_rfcall(const string& s, string **cl)
{
    bool rc = false;
    int i = 0, pos;

    while((cl[i] != NULL) && (rc == false)) {  
        pos = (cl[i])->find('*');

        if ((pos != 0) && (s.substr(0, pos).compare(cl[i]->substr(0, pos)) == 0))
            rc = true;
         
        i++;
    }
    return rc;
}


//------------------------------------------------------------------

//Case insensitive c char string compare function
int stricmp(const char* szX, const char* szY)
{
    int i;
    int len = strlen(szX);
    char* a = new char[len+1];
    
    for (i = 0; i < len; i++) 
        a[i] = tolower(szX[i]);
   
    a[i] = '\0';
    len = strlen(szY);
    char* b = new char[len+1];
    
    for (i = 0; i < len; i++) 
        b[i] = tolower(szY[i]);
   
    b[i] = '\0';

    int rc = strcmp(a,b);
    delete a;
    delete b;
    return rc;
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

    ifstream file(USER_DENY.c_str());
    if (!file)
        return rc;

    string User = string(user);
    unsigned i = user.find("-");
    if (i != string::npos) 
        User = user.substr(0,i);  //remove ssid

    do {  
        file.getline(Line,maxl);	  //Read each line in file
        if (!file.good()) 
            break;

        if (strlen(Line) > 0) {
            if (Line[0] != '#') {  //Ignore comments
                string sLine(Line);
                string token[maxToken];
                nTokens = split(sLine, token, maxToken, RXwhite);  //Parse into tokens
                upcase(token[0]);
                baduser = token[0];
                
                if ((stricmp(baduser.c_str(),User.c_str()) == 0) 
                        && (nTokens >= 2)) {
                    
                    rc = token[1][0];
                    break;
                }
            }
        }
    } while(file.good());

    file.close();
    return rc;
}

//---------------------------------------------------------------------
void reformatAndSendMicE(aprsString* inetpacket, cpQueue& sendQueue)
{
    //WriteLog(inetpacket->getChar(),"mic_e.log");

    if (ConvertMicE) {
        aprsString* posit = NULL;
        aprsString* telemetry = NULL;
        inetpacket->mic_e_Reformat(&posit,&telemetry);
        if (posit) 
            sendQueue.write(posit);          //Send reformatted packets
        
        if(telemetry) 
            sendQueue.write(telemetry);
   
        delete inetpacket; //Note: Malformed Mic_E packets that failed to convert are discarded
    } else
        sendQueue.write(inetpacket);  //Send raw Mic-E packet
}

//------------------------------------------------------------------------
/* return ascii H:M:S string with elapsed time ( NOW - starttime) */

void strElapsedTime(time_t starttime,  char* timeStr)
{
    if(starttime == -1) {
        sprintf(timeStr, "N/A");
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

//--------------------------------------------------------------------------

