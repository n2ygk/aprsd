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

#include <cstdlib>
#include <fstream>                      // ifstream
#include <iostream>

#include <unistd.h>                     // write, read
#include <stdio.h>                      // sys_errlist
#include <errno.h>                      // errno
#include <fcntl.h>

#include <termios.h>

#include "serial.h"
#include "aprsd.h"
#include "osdep.h"
#include "constant.h"
#include "utils.h"
#include "rf.h"
#include "cpqueue.h"
#include "mutex.h"


using namespace std;
using namespace aprsd;

int ttySread;
int ttySwrite;
int PortIsFile;
char txbuffer[260];

termios newSettings, originalSettings;
speed_t newSpeed, originalOSpeed, originalISpeed;

extern ULONG TotalTncChars;
extern cpQueue charQueue;
extern string MyCall;
extern char* ComBaud;
//extern bool TncSysopMode;         /*Set true when sysop wants direct TNC access */

Mutex pmtxWriteTNC;

//---------------------------------------------------------------------
// Sets various parameters on a COM port for use with TNC

int AsyncSetupPort(int fIn, int fOut, const string& TncBaud)
{
    int baud;

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

    if(tcsetattr(fIn, TCSANOW, &newSettings) != 0) {
        cerr << " Error: Could not set input serial port attrbutes\n";
        return -1;
    }

    if(tcsetattr(fOut, TCSANOW, &newSettings) != 0) {
        cerr << " Error: Could not set output serial port attrbutes\n";
        return -1;
    }

    return 0;
}

//--------------------------------------------------------------------
// Open and initialise the COM port for the TNC

int AsyncOpen(const string& szPort, const string& baudrate)
{
    APIRET rc;
    struct stat buf;

    // Check to see if the device is a file instead of a device;
    // if so, act a bit differently. Then we can use files as dummy ports.

    if (stat(szPort.c_str(), &buf) == -1) {
        cerr << "Error: could not find device " << szPort << endl;
        return -1;
    }

    PortIsFile = S_ISREG(buf.st_mode);

    if (PortIsFile) {
        cerr << "Note: detected that the serial device is a pipe" << endl;

        ttySwrite = open(szPort.c_str(), O_WRONLY | O_SYNC);
        if (ttySwrite == -1) {
            perror(szPort.c_str());
            cerr << "Error: could not open the file " << szPort << " for write" << endl;
            return -1;
        }

        // Use /dev/null for the input device
        ttySread = open("/dev/null", O_RDONLY);
        if (ttySread == -1) {
            cerr << "Error: could not open /dev/null as input device" << endl;
            return -1;
        }

    } else {

        ttySwrite = open(szPort.c_str(), O_WRONLY | O_NOCTTY);
        if (ttySwrite == -1) {
            char buf[128];
            strerror_r(errno, buf, sizeof(buf));

            cerr << "Error: Could not open serial port in WRITE mode: "
                << szPort << " [" << buf << "]" << endl;
            return -1;
        }

        ttySread = open(szPort.c_str(), O_RDONLY | O_NOCTTY);

        if (ttySread == -1) {
            char buf[128];
            strerror_r(errno, buf, sizeof(buf));
            cerr << "Error: Could not open serial port in READ mode: "
                << szPort << " [" << buf << "]" << endl;
            return -1;
        }


        if ((rc = AsyncSetupPort(ttySread, ttySwrite, baudrate)) != 0) {
            cerr << "Error in setting up COM port rc=" << rc << endl;
            return -2;
        }
    }
    return 0;
}

//--------------------------------------------------------------------
// Close the COM port

int AsyncClose(void)
{
    if (!PortIsFile) {
        if (tcsetattr (ttySread, TCSANOW, &originalSettings) != 0)
            cerr << " Error: Could not reset input serial port attrbutes\n";

        if (tcsetattr (ttySwrite, TCSANOW, &originalSettings) != 0)
            cerr << " Error: Could not reset input serial port attrbutes\n";

    }
    close (ttySwrite);
    return (close(ttySread));            // close COM port
}

//--------------------------------------------------------------------
// Read a line from the COM port.

bool AsyncReadWrite(char* buf)
{
    unsigned short i = 0;
    unsigned char *c;
    size_t BytesRead;
    bool lineTimeout;

    lineTimeout = false;

    do {
        if (txrdy) {          //Check for data to Transmit
            size_t len = strlen (txbuffer);
            write(ttySwrite, txbuffer, len);       //Send TX data to TNC
            if (PortIsFile)
                write(ttySwrite, "\n", 1);

            txrdy = 0;      //Indicate we sent it.
        }

        BytesRead = read(ttySread, &c, 1); //Non-blocking read.  100ms timeout.

        if (BytesRead == 0) {           // Serial input timeout
            if (i > 0)
                lineTimeout = true;     // We have some data but none has arrived lately.
        }

        TotalTncChars += BytesRead;

        if (BytesRead > 0) {            // Actual serial data from TNC has arrived
            if (i < (BUFSIZE - 4))
                buf[i++] = (char&)c;
            else
                i = 0;

            if (TncSysopMode) {
                i = 1;
                charQueue.write(c, (int)c);  // single char mode...
            }

        } else
            c = 0;

    } while (((int)c != 0x0a) && ((int)c != 0x0d) && (lineTimeout == false));
    buf[i] = '\0';

    return lineTimeout;
}


//-------------------------------------------------------------------
/*Write NULL terminated string to serial port */
int WriteCom(char *cp)
{
    int rc;
    timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 10000000; // 10mS  timeout for nanosleep()

    Lock lock(pmtxWriteTNC, false);

    lock.get();

    strncpy(txbuffer, cp, 256);

    txrdy = 1;
    while (txrdy)
        nanosleep(&ts, NULL);   //The ReadCom thread will clear txrdy when it takes data

    lock.release();

    return rc;
}




//---------------------------------------------------------------------
/* This reads file "szName" into the TNC.  When the keyword "MYCALL" is
   encounterd  the next word is loaded into the aprsd MyCall variable.
   When the keyword "UNPROTO" is encountered the word following it
   is replaced with "APDxxx" where xxx is the aprsd version number.
   For example UNPROTO APRS VIA WIDE will become UNPROTO APD213 VIA WIDE.
   */


int AsyncSendFiletoTNC(const char *szName)
{
    int nTokens;
    char Line[256];
    const int maxToken = 15;

    ifstream file(szName);
    if (file.is_open() == false) {
        cerr << "Can't open " << szName << endl;
        return -1;
    }

    Line[0] = 0x03;                     // Control C to get TNC into command mode
    Line[1] = '\0';
    rfWrite(Line);
    reliable_usleep(1);

    while (file.good ()) {
        file.getline(Line, 256);       // Read each line in file and send to TNC
        strncat(Line, "\r", 256);

        if ((Line[0] != '*') && (Line[0] != '#') && (strlen(Line) >= 2)) { //Reject comments
            string sLine(Line);
            string token[maxToken];
            nTokens = split(sLine, token, maxToken, RXwhite);  //Parse line into tokens
            upcase(token[0]);
            upcase(token[1]);

            if (token[0].compare("MYCALL") == 0) {      // Extract MYCALL from TNC.INIT
                MyCall = strdup(token[1].c_str());      // Put it in MyCall
                if (MyCall.length() > 9)
                    MyCall[9] = '\0';       // Truncate to 9 chars.
            }

            if ((token[0].compare("UNPROTO") == 0) && (nTokens >= 2)) { 
                // Insert APDxxx into ax25 dest addr of UNPROTO
                size_t idx2 = sLine.find(token[1],6);
                sLine.replace(idx2, token[1].length(), PGVERS);
            }

            cout << sLine << endl;      //print it to the console
            int len = sLine.length();

            write (ttySwrite, Line, len);
            reliable_usleep(500000);	//sleep for 500ms between lines
        }
    }
    file.close();
    return 0;
}

