/* dupCheck.h */

#ifndef DUPCHECK
#define DUPCHECK


using namespace std;

#include <string>
#include "constant.h"
#include "aprsString.h"

class dupCheck
{  
 
  
public:                        
   
   dupCheck();
   ~dupCheck();


   BOOL check(aprsString* s, int t);
   void clear(void);
  
private:
   pthread_mutex_t* pmtxdupCheck;  // mutex semaphore pointer
   
   time_t*  hashtime;
   INT16*   hashhash;
   


} ;

#endif
