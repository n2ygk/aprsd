
/* dupCheck.cpp */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define _REENTRANT
#define _PTHREADS

#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <fstream.h>
#include <iostream.h>
#include <strstream.h>
#include <iomanip.h>
#include <string.h>
#include <time.h>
#include <string>
#include <stdexcept>
#include "constant.h"
#include "dupCheck.h"



#define TABLESIZE  0x10000    /* 64K hash table containing time_t values */


     
dupCheck::dupCheck()
{
   
   pmtxdupCheck = new pthread_mutex_t; 
   pthread_mutex_init(pmtxdupCheck,NULL);
   
   hashtime = new time_t[TABLESIZE];
   hashhash = new INT16[TABLESIZE];
   clear();
   if((hashtime == NULL) || (hashhash == NULL)){
      cerr << "Dup Filter failed to initialize\n";
   }
   
}

dupCheck::~dupCheck()
{
   delete hashtime;
   pthread_mutex_destroy(pmtxdupCheck);
   delete pmtxdupCheck;
}

BOOL dupCheck::check(aprsString* s, int t)
{
  BOOL dup = FALSE;

  if((hashtime == NULL) || (hashhash == NULL) || s->allowdup){
     
      return FALSE;
  }

  pthread_mutex_lock(pmtxdupCheck);

  INT32 hash = s->gethash();
  INT16 hash_lo = hash & 0xffff;  //be sure we stay inside the table!
  INT16 hash_hi = hash >> 16;     //upper 16 bits of hash 
  hash_hi &= 0xffff;
  
  

  if(((s->timestamp - hashtime[hash_lo]) <= t )  // See if time difference is less than t seconds
     && (hash_hi == hashhash[hash_lo])) {    // and hash_hi value is identical
                                           
     dup = TRUE;
  }
  
  hashtime[hash_lo] = s->timestamp;   // put this new data in the tables
  hashhash[hash_lo] = hash_hi;

  pthread_mutex_unlock(pmtxdupCheck);
 // printf("hash32= %08X  hash_hi= %04X  hash_lo= %04X %s",s->hash, hash_hi, hash_lo, s->raw.c_str()); //debug
  return dup;
}




void dupCheck::clear()
{
  for(int i=0;i<TABLESIZE;i++){ 
     if(hashtime) hashtime[i] = 0;  //Initialize tables
     if(hashhash) hashhash[i] = 0;
  }
}
  
  
  
  
      
          
   
   
