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

#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <termios.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include <fstream.h>
#include <iostream.h>
#include <strstream.h>
#include <iomanip.h>

#include "constant.h"
#include "serial.h"
#include "utils.h"
#include "rf.h"
#include "cpqueue.h"

int ttySread;
int ttySwrite;

termios newSettings, originalSettings;
speed_t newSpeed, originalOSpeed, originalISpeed;

extern ULONG TotalTncChars;
extern cpQueue charQueue;
extern string MyCall;

//---------------------------------------------------------------------
// Sets various parameters on a COM port for use with TNC

int AsyncSetupPort (int fIn, int fOut)
{

    tcgetattr (fIn, &originalSettings);
    newSettings = originalSettings;

    newSettings.c_lflag = 0;
    newSettings.c_cc[VMIN] = 0;
    newSettings.c_cc[VTIME] = 1;	//.1 second timeout for input chars
    newSettings.c_cflag = CLOCAL | CREAD | CS8;
    newSettings.c_oflag = 0;
    newSettings.c_iflag = IGNBRK | IGNPAR;

    cfsetispeed (&newSettings, B1200);
    cfsetospeed (&newSettings, B1200);

    if (tcsetattr (fIn, TCSANOW, &newSettings) != 0) {
        cerr << " Error: Could not set input serial port attrbutes\n";
        return -1;
    }

    if (tcsetattr (fOut, TCSANOW, &newSettings) != 0) {
        cerr << " Error: Could not set output serial port attrbutes\n";
        return -1;
    }
    return 0;
}

//--------------------------------------------------------------------
// Open and initialise the COM port for the TNC

int AsyncOpen (char *szPort)
{
    APIRET rc;

    ttySwrite = open (szPort, O_WRONLY | O_NOCTTY);
    if (ttySwrite == -1) {
        cerr << "Error: Could not open serial port in WRITE mode: "
            << szPort << " [" << sys_errlist[errno] << "]" << endl;
        return -1;
    }

    ttySread = open (szPort, O_RDONLY | O_NOCTTY);

    if (ttySread == -1) {
        cerr << "Error: Could not open serial port in READ mode: "
            << szPort << " [" << sys_errlist[errno] << "]" << endl;
        return -1;
    }

    if ((rc = AsyncSetupPort (ttySread, ttySwrite)) != 0) {
        cerr << "Error in setting up COM port rc=" << rc << endl << flush;
        return -2;
    }
    return 0;
}

//--------------------------------------------------------------------
// Close the COM port

int AsyncClose (void)
{
    if (tcsetattr (ttySread, TCSANOW, &originalSettings) != 0)
        cerr << " Error: Could not reset input serial port attrbutes\n";

    if (tcsetattr (ttySwrite, TCSANOW, &originalSettings) != 0)
        cerr << " Error: Could not reset input serial port attrbutes\n";

    close (ttySwrite);
    return(close(ttySread));            // close COM port
}

//--------------------------------------------------------------------
// Read a line from the COM port.

bool AsyncReadWrite (char* buf)
{
    USHORT i;
    unsigned char c;
    size_t BytesRead;
    bool lineTimeout;

    lineTimeout = FALSE;

    i = 0;

    do {

        if (txrdy) {          //Check for data to Transmit
            size_t len = strlen (tx_buffer);
            write (ttySwrite, tx_buffer, len);       //Send TX data to TNC
            txrdy = 0;      //Indicate we sent it.
        }

        BytesRead = read (ttySread, &c, 1); //Non-blocking read.  100ms timeout.

        if (BytesRead == 0) {           // Serial input timeout
            if (i > 0)
                lineTimeout = TRUE;     // We have some data but none has arrived lately.
        }

        TotalTncChars += BytesRead;

        if (BytesRead > 0) {            // Actual serial data from TNC has arrived
            if (i < (BUFSIZE - 4))
                buf[i++] = c;
            else
                i = 0;

            if (TncSysopMode) {
                i = 1;
                charQueue.write ((char *)c, (int)c);  // single char mode...
            }

        } else
            c = 0;

    } while ((c != 0x0a) && (c != 0x0d) && (lineTimeout == FALSE));

    buf[i] = '\0';

    return lineTimeout;

}


//---------------------------------------------------------------------
/* This reads file "szName" into the TNC.  When the keyword "MYCALL" is
   encounterd  the next word is loaded into the aprsd MyCall variable.
   When the keyword "UNPROTO" is encountered the word following it
   is replaced with "APDxxx" where xxx is the aprsd version number.
   For example UNPROTO APRS VIA WIDE will become UNPROTO APD213 VIA WIDE.
   */


int AsyncSendFiletoTNC (char *szName)
{
    int nTokens;
    char Line[256];
    const int maxToken = 15;

    ifstream file (szName);
    if (file.is_open () == FALSE) {
        cerr << "Can't open " << szName << endl << flush;
        return -1;
    }

    Line[0] = 0x03;                     // Control C to get TNC into command mode
    Line[1] = '\0';
    rfWrite (Line);
    sleep (1);

    while (file.good ()) {
        file.getline(Line, 256);       // Read each line in file and send to TNC
        strncat(Line, "\r", 256);

        if ((Line[0] != '*') && (Line[0] != '#') && (strlen (Line) >= 2)) { //Reject comments
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

            if ((token[0].compare("UNPROTO") == 0)
                    && (nTokens >= 2)) {       // Insert APDxxx into ax25 dest addr of UNPROTO
                size_t idx2 = sLine.find(token[1],6);
                sLine.replace(idx2, token[1].length(), PGVERS);
            }

            cout << sLine.c_str() << endl;      //print it to the console
            int len = sLine.length();

            write (ttySwrite, Line, len);
            reliable_usleep(500000);	//sleep for 500ms between lines
        }
    }
    file.close ();
    return 0;
}

