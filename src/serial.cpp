/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-2003 aprsd Dev Team
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

#include "config.h" 
 
#include <unistd.h>
#include <cstdio>
#include <pthread.h>
#include <string>
#include <cassert>
#include <cerrno>
#include <termios.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <ctime>

#include <fstream>
#include <iostream>
#include <strstream>
#include <iomanip>

//tcpip header files

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include "constant.h" 
#include "serial.h"
#include "utils.h"
#include "cpqueue.h"
#include "history.h"
#include "queryResp.h"
#include "servers.h"
#include "mutex.h"

using namespace std;
using namespace aprsd;

int ttySread ;
int ttySwrite;
pthread_t tidReadCom;
termios newSettings, originalSettings;
speed_t newSpeed, originalOSpeed, originalISpeed;
Mutex pmtxWriteTNC;
char txbuffer[260];
int txrdy;
int CloseAsync, threadAck;
bool  TncSysopMode;         /*Set true when sysop wants direct TNC access */



//---------------------------------------------------------------------

/* Sets various parameters on a COM port for use with TNC*/

int SetupComPort( int fIn, int fOut, const string& TncBaud)
{
    speed_t baud = B0;
    
    if (TncBaud.compare("1200") == 0) 
        baud = B1200;
        
    if (TncBaud.compare("2400") == 0) 
        baud = B2400;
        
    if (TncBaud.compare("4800") == 0) 
        baud = B4800;
        
    if (TncBaud.compare("9600") == 0) 
        baud = B9600;
        
    if (TncBaud.compare("19200") == 0) 
        baud = B19200;
        

    tcgetattr(fIn,&originalSettings);
    newSettings = originalSettings;

    newSettings.c_lflag = 0;
    newSettings.c_cc[VMIN] = 0;
    newSettings.c_cc[VTIME] = 1;		 //.1 second timeout for input chars
    newSettings.c_cflag = CLOCAL | CREAD | CS8 ;
    newSettings.c_oflag = 0;
    newSettings.c_iflag = IGNBRK|IGNPAR ;

    cfsetispeed(&newSettings, baud);
    cfsetospeed(&newSettings, baud);

    if (tcsetattr(fIn, TCSANOW, &newSettings) != 0) {   
        cerr << " Error: Could not set input serial port attrbutes\n";
        return -1;
    }

    if (tcsetattr(fOut, TCSANOW, &newSettings) != 0) {  
        cerr << " Error: Could not set output serial port attrbutes\n";
        return -1;
    }

    return 0;
}


//--------------------------------------------------------------------
int AsyncOpen(const string& szPort, const string& baudrate)
{
    APIRET rc;
    TncSysopMode = false;
    txrdy = 0;

    if ((ttySwrite = open(szPort.c_str(), O_WRONLY | O_NOCTTY)) == -1) {
        cerr << "Error: Could not open serial port in WRITE mode: " 
            << szPort << " [" << strerror_r(errno) << "]" << endl;
        return -1;
    }
 
    if ((ttySread = open(szPort.c_str(), O_RDONLY | O_NOCTTY)) == -1) {
        cerr << "Error: Could not open serial port in READ mode: "
            << szPort << " [" << strerror_r(errno) << "]" << endl;
        return -1;
    }

    if ((rc = SetupComPort(ttySread, ttySwrite, string(baudrate))) != 0) {  
        cerr << "Error in setting up COM port rc=" << rc << endl;
        return -2;
    }

    CloseAsync = false;
    threadAck = false;

    /* Now start the serial port reader thread */

    if ((rc = pthread_create(&tidReadCom, NULL, ReadCom, NULL)) != 0) {
        cerr << "Error: ReadCom thread creation failed. Error code = " << rc << endl;
        CloseAsync = true;
    }

    return 0;
}

//--------------------------------------------------------------------
 int AsyncClose(void)
 {
    timespec ts;
    int retval;
    
    if (tcsetattr(ttySread, TCSANOW, &originalSettings) != 0)
        cerr << " Error: Could not reset input serial port attrbutes\n";

    if (tcsetattr(ttySwrite, TCSANOW, &originalSettings) != 0)
        cerr << " Error: Could not reset input serial port attrbutes\n";
    
    CloseAsync = true;      //Tell the read thread to quit
    ts.tv_sec = 0;
    ts.tv_nsec = 100000;     //100uS timeout for nanosleep()                
    
    while (threadAck == false) 
        nanosleep(&ts,NULL) ;   //wait till it does
    
    retval = close(ttySwrite);
    return retval;     //close COM port
}

//-------------------------------------------------------------------
/*Write NULL terminated string to serial port */
int WriteCom(char *cp)
{
    int rc = 0;
    Lock tncWriteLock(pmtxWriteTNC);
    
    
    timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 10000000; // 10mS  timeout for nanosleep()
    
    strncpy(txbuffer, cp, 256);
  
    txrdy = 1;
    
    while(txrdy) 
        nanosleep(&ts,NULL);   //The ReadCom thread will clear txrdy when it takes data

    tncWriteLock.release();

    return rc;
}

//-------------------------------------------------------------------
 /*
//Write a single character to serial port 
int WriteCom(char c)
{
   int rc;
   if (c == 0x0a) return 1;  //TNCs don't want line feeds!
	pthread_mutex_lock(pmtxWriteTNC);
   rc = fwrite(&c,1,1,ttySwrite);
   
	fflush(ttySwrite);

   //printhex(&c,1);       //debug only

	pthread_mutex_unlock(pmtxWriteTNC);

	return rc;

}

 */
//-------------------------------------------------------------------
//--------------------------------------------------------------------
// This is the COM port read thread.
// It also handles buffered writes to the COM port.

void *ReadCom(void* vp)
{
    unsigned short i;
    unsigned char buf[BUFSIZE];
    unsigned char c;
    size_t BytesRead;
    int count = 0;
    bool lineTimeout;
    aprsString* abuff;

    i = 0;
    c = 0;

    pidlist.SerialInp = getpid();

    while (!CloseAsync) {
        do {                    // Sit here till end of line
                                // or timeout (defined in SetupComPort() )
                                // and read chars from TNC into buffer.
            if (txrdy) {        // Check for data to Transmit
                size_t len = strlen(txbuffer);
                write(ttySwrite, txbuffer, len);      //Send TX data to TNC
                txrdy = 0;                          //Indicate we sent it.
            }
            lineTimeout = false;
            if ((BytesRead = read(ttySread, &c, 1)) == 0) {      //Non-blocking read.  100ms timeout.
                //Serial input timeout
                if (i > 0) 
                    lineTimeout = true; // We have some data but none has arrived lately.
            }

            TotalTncChars += BytesRead ;

            if (BytesRead > 0) {    //Actual serial data from TNC has arrived
                //printhex(&c,1);   //debug
                if (i < (BUFSIZE-4)) 
                    buf[i++] = c; 
                else 
                    i = 0;
                
                if (TncSysopMode) {  
                    i = 1; 
                    charQueue.write((char*)NULL,(int)c); //single char mode...
                    //printhex(&c,1);   //debug
                }
            } else 
                c = 0;
        } while (( c != 0x0a) && (c != 0x0d) && (lineTimeout == false) && (CloseAsync == false));

        WatchDog++;
        tickcount = 0;
        
        if ((i > 0) && ((buf[0] != 0x0d) && (buf[0] != 0x0a)) ) {	
            if (lineTimeout == false) {
                TotalLines++;
                buf[i-1] = 0x0d;
                buf[i++] = 0x0a;
            }
            buf[i++] = '\0'; 
            count = i;       
           
            bool reject=false;
            //printhex((char*)buf,i);		  //debug
            //sleep(1);
            
            if ((count > 0) && (!TncSysopMode) && (configComplete)) { 
                abuff = new aprsString((char*)buf,SRC_TNC,srcTNC,"TNC","*"); // <- Received Packet
                //abuff->print(cout);  //debug

                if (abuff != NULL) {    //don't let a null pointer get past here!
                    /*
                    if(abuff->aprsType == APRSQUERY){ // non-directed query ? 
                       queryResp(SRC_TNC,abuff);   // yes, send our response
                       reject = true;              // don't pass query to other servers 
                       
                    }
                    */ 
                   
                    if (logAllRF || (abuff->ax25Source.compare(MyCall) == 0))
                        WriteLog(abuff->c_str(),RFLOG); //Log our own packets that were digipeated

                    if (abuff->hasBeenIgated()) 
                        reject = true;  /*Check 3rd party
                                                                header for TCPIP , TCPXX, or I*   
                                                                Kill packet if it's been igated */

                    if ((abuff->ax25Source.compare(MyCall) == 0)  && (igateMyCall == false)) 
                        reject = true; //Check for packets from our own TNC

#ifdef DEBUGTHIRDPARTY
                    if ((abuff->aprsType == APRS3RDPARTY) //Log 3rd party for debuging
                            && (reject == false)) {
                        string logEntry = "TNC: " + *abuff;
                        WriteLog(logEntry.c_str(),DEBUGLOG);  //DEBUG CODE
                    }
#endif

                    if ((abuff->aprsType == APRS3RDPARTY) // Convert  3rd party packets not previously
                            && (reject == false)){            // igated to normal packets
                        abuff->thirdPartyToNormal();
                        if (abuff->aprsType == APRSQUERY)  
                            reject = true;  //No ?IGATE? querys inside 3rdparty. 
                      
                    }

                    if (reject) {
                        abuff->aprsType = APRSREJECT; //Mark rejected packed as ERROR
                        sendQueue.write(abuff,0);    //Send it on to be viewed on rejected pkt port 14503
                    } else {  //Not rejected and optionally not from our own TNC...
                        if (abuff->aprsType == APRSMSG) { //find matching posit for Message Packet
                            aprsString* posit = getPosit(abuff->ax25Source, srcIGATE | srcUSERVALID | srcTNC);
                            if (posit != NULL) { 
                                posit->EchoMask = src3RDPARTY;
                                sendQueue.write(posit);  //send matching posit only to msg port
                            } 
                        }
                        
                        if (abuff->aprsType == APRSMIC_E) {  //Optionally reformat if it's a Mic-E packet
                            reformatAndSendMicE(abuff,sendQueue); //Reformats if conversion is enabled
                        } else
                            sendQueue.write(abuff,0);// Now put it in the Internet send queue.            
                                                               // *** abuff must be freed by Queue reader ***.  
                    }
                }
            }
        } 

        i = 0;  //Reset buffer pointer
        c = 0;
    } // Loop until server is shut down

    cerr << "Exiting Async com thread\n" << endl << flush;
    threadAck = true;
    pthread_exit(0);
}

//---------------------------------------------------------------------
/* This reads file "szName" into the TNC.  When the keyword "MYCALL" is
   encounterd  the next word is loaded into the aprsd MyCall variable.
   When the keyword "UNPROTO" is encountered the word following it
   is replaced with "APDxxx" where xxx is the aprsd version number.
   For example UNPROTO APRS VIA WIDE will become UNPROTO APD213 VIA WIDE.
   */

int SendFiletoTNC(const char* szName)
{   
    int nTokens;
    char Line[260];
    const int maxToken = 15;
   
    timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 500000000;  //500ms timeout for nanosleep()

    ifstream file(szName);

    if (file.is_open() == false) {
        cerr << "Can't open " << szName << endl << flush;
        return -1 ;
    }

    Line[0] = 0x03;     //Control C to get TNC into command mode
    Line[1] = '\0';
    WriteCom(Line);
    sleep(1);
    
    while (file.good()) {
        file.getline(Line,256);	  //Read each line in file and send to TNC
        strcat(Line,"\r");

        if ((Line[0] != '*') && (Line[0] != '#') && (strlen(Line) >= 2)  ) { //Reject comments
            string sLine(Line);
            string token[maxToken];
            nTokens = split(sLine, token, maxToken, RXwhite);  //Parse line into tokens
            upcase(token[0]);
            upcase(token[1]);

            if (token[0].compare("MYCALL") == 0) {    //Extract MYCALL from TNC.INIT
                MyCall = strdup(token[1].c_str());   //Put it in MyCall
                if (MyCall.length() > 9) {
                    string tmpstr;
                    tmpstr = MyCall.substr(0,9);
                    //MyCall[9] = '\0';  //Truncate to 9 chars.
                    MyCall = tmpstr;
                }
            }

            if ((token[0].compare("UNPROTO") == 0) && (nTokens >= 2)) {  //Insert APDxxx into ax25 dest addr of UNPROTO
                size_t idx2 = sLine.find(token[1],6);
                sLine.replace(idx2, token[1].length(), PGVERS);
            }

            cout << sLine << endl;      //print it to the console
            int len = sLine.length();				
            write(ttySwrite, sLine.c_str(), len); //Send line to TNC
            nanosleep(&ts, NULL);                   //sleep for 500ms between lines
        }
    }

    file.close();
    return 0;
}
//----------------------------------------------------------------------

