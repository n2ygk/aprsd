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


#include "assert.h"
#include "mutex.h"
#include "osdep.h"

namespace aprsd
{
    Mutex::Mutex() throw(AssertException, exception)
    {
        pthread_mutexattr_init(&mutexAttr);
        pthread_mutex_init(&mutex, &mutexAttr);
        pthread_condattr_init(&condAttr);
        pthread_cond_init(&cond, &condAttr);
    }

    Mutex::~Mutex() throw()
    {
        pthread_mutex_destroy(&mutex);
        pthread_mutexattr_destroy(&mutexAttr);
        pthread_cond_destroy(&cond);
        pthread_condattr_destroy(&condAttr);
    }

    void Mutex::lock() throw(AssertException, exception)
    {
        pthread_mutex_lock(&mutex);
    }

    void Mutex::unlock() throw(AssertException, exception)
    {
        pthread_mutex_unlock(&mutex);
    }

    void Mutex::wait() throw(AssertException, exception)
    {
        pthread_cond_wait(&cond, &mutex);
    }

    void Mutex::notify() throw(AssertException, exception)
    {
        pthread_cond_signal(&cond);
    }

    void Mutex::notifyAll() throw(AssertException, exception)
    {
        pthread_cond_broadcast(&cond);
    }

    RecursiveMutex::RecursiveMutex() throw(AssertException, exception) :
        lockCount(0)
    { }

    RecursiveMutex::~RecursiveMutex() throw() { }

    void RecursiveMutex::lock() throw(AssertException, exception)
    {
        if (!lockCount || !pthread_equal(owner, pthread_self()))
        {
            Mutex::lock();
            owner = pthread_self();
        }
        ++lockCount;
    }

    void RecursiveMutex::unlock() throw(AssertException, exception)
    {
        DEBUG(Assert(lockCount > 0,
                    AssertException("aprsd::RecursiveMutex::unlock(): "
                                    "mutex already unlocked")));
        DEBUG(Assert(pthread_equal(owner, pthread_self()),
                    AssertException("aprsd::RecursiveMutex::unlock(): "
                                    "mutex locked by another thread")));
        if (--lockCount == 0)
            Mutex::unlock();
    }

    void RecursiveMutex::wait() throw(AssertException, exception)
    {
        unsigned int saveCount = lockCount;
        lockCount = 0;
        Mutex::wait();
        owner = pthread_self();
        lockCount = saveCount;
    }

    Lock::Lock(Mutex& arg_mutex, bool autoLock) throw(AssertException, exception) :
        mutex(arg_mutex), locked(false)
    {
        if (autoLock) get();
    }

    Lock::~Lock() throw()
    {
        try
        {
            release();
        }
        catch (...) { }
    }

    void Lock::get() throw(AssertException, exception)
    {
        if (!locked)
        {
            mutex.lock();
            locked = true;
        }
    }

    void Lock::release() throw(AssertException, exception)
    {
        if (locked)
        {
            mutex.unlock();
            locked = false;
        }
    }

    void Lock::wait() throw(AssertException, exception)
    {
        DEBUG(Assert(locked,
                    AssertException("aprsd::Lock::wait(): "
                                    "wait on unlocked mutex")));
        mutex.wait();
    }

    void Lock::notify() throw(AssertException, exception)
    {
        DEBUG(Assert(locked,
                    AssertException ("aprsd::Lock::notify(): "
                                    "notify on unlocked mutex")));
        mutex.notify();
    }

    void Lock::notifyAll() throw(AssertException, exception)
    {
        DEBUG(Assert(locked,
                    AssertException("aprsd::Lock::notifyAll(): "
                                    "notifyAll on unlocked mutex")));
        mutex.notifyAll();
    }
}
