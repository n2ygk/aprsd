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


#ifndef OSDEP_H
#define OSDEP_H

#include "config.h"
#include <string>

#include <netdb.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if !defined(HAVE_LIBPTHREAD) && defined(HAVE_LIBDCE) && \
    defined(HAVE_LIBCMA)
#include <pthread.h>
#endif  // !defined(HAVE_LIBPTHREAD) && defined(HAVE_LIBDCE) &&
    // defined(HAVE_LIBCMA)

#ifndef __GLIBC__
    extern int h_errno;
#endif // __GLIBC__


namespace aprsd
{
    using std::string;

    static inline int gethostbyname_r(const string& name, struct hostent *result_buf,
                                      char *buf, size_t buflen, struct hostent **result,
                                      int *h_errnop)
    {
#ifdef __GLIBC__
        return ::gethostbyname_r (name.c_str (), result_buf,
                                  buf, buflen, result, h_errnop);
#else
        if (buflen < sizeof (hostent_data))
        {
            if (h_errnop != NULL)
                *h_errnop = NO_RECOVERY;
            return -1;
        }
        int rc = ::gethostbyname_r (name.c_str (), *result = result_buf,
                                    reinterpret_cast<hostent_data*>(buf));
        if ((rc < 0) && (h_errnop != NULL))
            *h_errnop = h_errno;
        return rc;
#endif
    }

    extern string hstrerror_r(int ERRNUM);

    static inline string strerror_r(int ERRNUM)
    {
        char buf [1024];

#ifdef __GLIBC__
        return ::strerror_r (ERRNUM, buf, sizeof (buf));
#else   // !__GLIBC__
        ::strerror_r (ERRNUM, buf, sizeof (buf));
        return buf;
#endif  // !__GLIBC__
    }


#if !defined(HAVE_LIBPTHREAD) && defined(HAVE_LIBDCE) && \
    defined(HAVE_LIBCMA)

#ifdef select
#undef SELECT_TYPE_ARG234
#define SELECT_TYPE_ARG234 (int*)
#endif

    static inline int pthread_mutexattr_init(pthread_mutexattr_t *mutex_attr)
    {
        return ::pthread_mutexattr_create(mutex_attr);
    }

    static inline int pthread_mutexattr_destroy(pthread_mutexattr_t *mutex_attr)
    {
        return ::pthread_mutexattr_delete(mutex_attr);
    }

    static inline int pthread_mutex_init(pthread_mutex_t *mutex,
                                         const pthread_mutexattr_t *mutex_attr)
    {
        if(mutex_attr == NULL)
            return -1;
        return ::pthread_mutex_init(mutex, *mutex_attr);
    }

    static inline int pthread_condattr_init(pthread_condattr_t *cond_attr)
    {
        return ::pthread_condattr_create(cond_attr);
    }

    static inline int pthread_condattr_destroy(pthread_condattr_t *cond_attr)
    {
        return ::pthread_condattr_delete(cond_attr);
    }

    static inline int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *cond_attr)
    {
        if (cond_attr == NULL)
            return -1;
        return ::pthread_cond_init (cond, *cond_attr);
    }

    static inline int pthread_attr_init(pthread_attr_t *attr)
    {
        return ::pthread_attr_create (attr);
    }

    static inline int pthread_attr_destroy(pthread_attr_t *attr)
    {
        return ::pthread_attr_delete (attr);
    }

    static inline int pthread_create(pthread_t *thread, pthread_attr_t *attr,
                                     void *(*start_routine)(void *), void *arg)
    {
        if (attr == NULL)
            return -1;
        return ::pthread_create (thread, *attr, start_routine, arg);
    }

    static inline int pthread_detach(pthread_t th)
    {
        return ::pthread_detach(&th);
    }
#endif  // !defined(HAVE_LIBPTHREAD) && defined(HAVE_LIBDCE) &&
    // defined(HAVE_LIBCMA)

    extern void reliable_usleep(int usecs);
}

#endif  // OSDEP_H
