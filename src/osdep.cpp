#include "mutex.h"
#include "osdep.h"

namespace aprsd
{
    string hstrerror_r(int ERRNUM)
    {
        static Mutex mutex;

        Lock lock (mutex);
#ifdef HAVE_HSTRERROR
        return ::hstrerror (ERRNUM);
#else /* !HAVE_HSTRERROR */
        switch (ERRNUM)
        {
        case 0:
            return "Success.";
        case HOST_NOT_FOUND:
            return "The specified host is unknown.";
        case NO_ADDRESS:
#if NO_ADDRESS != NO_DATA
        case NO_DATA:
#endif // NO_ADDRESS != NO_DATA
            return "The requested name is valid but does not have an IP address.";
        case NO_RECOVERY:
            return "A non-recoverable name server error occurred.";
        case TRY_AGAIN:
            return "A temporary error occurred on an authoritative name server. "
                "Try again later.";
        }

        return "Unknown error number at hstrerror_r.";
#endif/* !HAVE_HSTRERROR */
    }

    void reliable_usleep (int usecs)
    {
        timeval now, end;

        gettimeofday (&now, NULL);
        end = now;
        end.tv_sec  += usecs / 1000000;
        end.tv_usec += usecs % 1000000;

        while ((now.tv_sec < end.tv_sec) ||
               ((now.tv_sec == end.tv_sec) && (now.tv_usec < end.tv_usec)))
        {
            timeval tv;
            tv.tv_sec = end.tv_sec - now.tv_sec;
            if (end.tv_usec >= now.tv_usec)
                tv.tv_usec = end.tv_usec - now.tv_usec;
            else
            {
                tv.tv_sec--;
                tv.tv_usec = 1000000 + end.tv_usec - now.tv_usec;
            }
            select (0, NULL, NULL, NULL, &tv);
            gettimeofday (&now, NULL);
        }
    }
}
