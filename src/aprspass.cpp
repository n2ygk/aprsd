/* aprspass.cpp  :  APRS Passcode generator  April 12, 2000*/

#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif


#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <iostream.h>
#include <strstream.h>
#include <iomanip.h>

#include "validate.h"




int main(int argc, char *argv[])
{
	
   if(argc < 2){
       cout << "Usage: aprspass <call sign>\n";
       exit(1);
   }
   
   cout  << "APRS passcode for " 
         << argv[1]
         << " = "
         << doHash(argv[1]) 
         << endl;

   return 0;
   
}
