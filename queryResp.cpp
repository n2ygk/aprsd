/* queryResp.cpp   Generate response message for ?IGATE? queries */

 
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define _REENTRANT
#define _PTHREADS

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netdb.h>
#include <pthread.h>

#include <fstream.h>
#include <iostream.h>
#include <strstream.h>
#include <iomanip.h>
#include <errno.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#include "constant.h"
#include "utils.h"
#include "aprsString.h"
#include "queryResp.h"

void BroadcastString(char *cp);

extern char* szAPRSDPATH ;
extern char* szServerCall ;
extern char* MyLocation ;
extern char* MyCall;
extern char* MyEmail;

extern int msgsn;
extern cpQueue tncQueue;
extern pthread_mutex_t* pmtxCount;
extern pthread_mutex_t* pmtxDNS;

char szHostIP[HOSTIPSIZE];
int queryCounter;



void queryResp(int session, const aprsString* pkt)
{  int rc;
   struct hostent *h = NULL;
   struct hostent hostinfo_d;
   char h_buf[512];
   int h_err;

   aprsString *rfpacket, *ackRFpacket;
   char* hostname = new char[80];
   unsigned char hip[5];
   char* cp = new char[256];
   char* cpAck = new char[256];
   ostrstream reply(cp,256);
   ostrstream ack(cpAck,256);
   BOOL wantAck = FALSE;

   for (int i=0;i<4;i++) hip[i] = 0;

   /* Get hostname and host IP */
   rc = gethostname(hostname,80);
   if (rc != 0) strcpy(hostname,"Host_Unknown");
   else {
     
      //pthread_mutex_lock(pmtxDNS);
      //Thread-Safe verison of gethostbyname2() ? 
      rc = gethostbyname2_r(hostname, AF_INET,
                                &hostinfo_d,
                                h_buf,
                                512,
                                &h,
                                &h_err);

      
     // pthread_mutex_unlock(pmtxDNS);

      if (h != NULL) {
         strncpy(hostname,h->h_name,80);            //Full host name
         strncpy((char*)hip,h->h_addr_list[0],4);   //Host IP
      }

   }


   /*debug*/
   /*
   cout  << "Hostname: " << hostname << endl
         << "IP: " 
         << (int)hip[0] << "." 
         << (int)hip[1] << "."
         << (int)hip[2] << "."
         << (int)hip[3] 
         <<endl; 
    */    



   //cout << pkt << endl;

   //Send ack only if there was a line number on the end of the query.
   if (pkt->acknum.length() == 0) wantAck = FALSE;
   else wantAck = TRUE;

   char sourceCall[] = "         ";  //9 blanks
   int i = 0;
   while ((i <9) && (pkt->ax25Source.c_str()[i] != '\0')) {
      sourceCall[i] = pkt->ax25Source.c_str()[i];
      i++;
   }


   if (wantAck) {  /* construct an ack packet */
      ack   << pkt->stsmDest << szAPRSDPATH << ":"
      << sourceCall << ":ack"
      << pkt->acknum << "\r\n"  /*use senders sequence number */
      << ends;
   }

   /* Now build the query specfic packet(s) */

   if (pkt->query.compare("IGATE") == 0) {

      pthread_mutex_lock(pmtxCount) ;
      queryCounter++;                   //Count this query
      pthread_mutex_unlock(pmtxCount);

      reply << szServerCall << szAPRSDPATH << ":"
      << sourceCall << ":" 
      << MyCall << " "
      << MyLocation << " "
      << hostname << " " 
      << (int)hip[0] << "." 
      << (int)hip[1] << "." 
      << (int)hip[2] << "." 
      << (int)hip[3] << " "
      << MyEmail << " "
      << VERS
      << "\r\n"   /* Don't send a sequence number! */
      << ends;


      switch (session) {

      case SRC_IGATE: if (wantAck) BroadcastString(cpAck);  // To the whole aprs network
         BroadcastString(cp);
         break;

      case SRC_TNC:    if (wantAck) {
            ackRFpacket = new aprsString(cpAck);  //Ack reply
            ackRFpacket->stsmReformat(MyCall);
            tncQueue.write(ackRFpacket);
         }

         rfpacket = new aprsString(cp);   // Query reply                                                 
         rfpacket->stsmReformat(MyCall);  // Reformat it for RF delivery
         //cout << rfpacket << endl;      //debug
         tncQueue.write(rfpacket);        // queue read deletes rfpacket

         break;


      default:   if (session >= 0) {
            if (wantAck) rc = send(session,(const void*)cpAck,strlen(cpAck),0);  //Only to one user
            rc = send(session,(const void*)cp,strlen(cp),0);   
         }
      }

   }
  
   delete cp;
   delete cpAck;
   delete hostname;


}


//---------------------------------------



