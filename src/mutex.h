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




#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

#include "aprsdexception.h"
#include "osdep.h"

namespace aprsd
{
    class Lock;

    /**
        * A fast mutex for synchronizing non-recursive methods.  Use a Lock to lock and
        * unlock a Mutex.  Use a RecursiveMutex if you need to be able to lock a mutex
        * more than once before unlocking it.
        *
        * This class is not reference-counted; each instance is intended to be
        * encapsulated as a member variable by the object that uses it.
        *
        */
    class Mutex
    {
    public:
        /**
        * Constructs a Mutex.
        */
        Mutex() throw(AssertException, exception);

        /**
        * Destructor.
        */
        virtual ~Mutex() throw();

    protected:
        virtual void lock() throw(AssertException, exception);
        virtual void unlock() throw(AssertException, exception);
        virtual void wait() throw(AssertException, exception);

    private:
        Mutex(const Mutex&);
        Mutex& operator=(const Mutex&);
        void notify() throw(AssertException, exception);
        void notifyAll() throw(AssertException, exception);

        pthread_mutex_t mutex;
        pthread_mutexattr_t mutexAttr;
        pthread_cond_t cond;
        pthread_condattr_t condAttr;

        friend class Lock;
    };


    /**
    * A mutex for synchronizing recursive methods.  Use a Lock to lock and unlock a
    * RecursiveMutex.
    *
    * If a locked RecursiveMutex is locked again by the same thread,
    * its lock count is incremented.  If it unlocked by the same
    * thread, its lock count is decremented.  When its lock count
    * reaches 0, it is unlocked.
    *
    * This class is not reference-counted; each instance is intended to be
    * encapsulated as a member variable by the object that uses it.
    *
    */
    class RecursiveMutex : public Mutex
    {
    public:
        /**
        * Constructs a RecursiveMutex.
        */
        RecursiveMutex() throw(AssertException, exception);

        /**
        * Destructor.
        */
        virtual ~RecursiveMutex() throw();

    protected:
        virtual void lock() throw(AssertException, exception);
        virtual void unlock() throw(AssertException, exception);
        virtual void wait() throw(AssertException, exception);

    private:
        RecursiveMutex(const RecursiveMutex&);
        RecursiveMutex& operator=(const RecursiveMutex&);

        unsigned int lockCount;
        pthread_t owner;
    };


    /**
    * When constructed, this object acquires (by default) an
    * exclusive lock from the Mutex passed to it in its constructor.
    * When the object goes out of scope, it releases the lock.  This
    * makes it convenient to synchronize whole methods, because the
    * lock will be released even if an exception is thrown.  For
    * example:
    *
    * class Foo
    * {
    * public:
    *     void bar()
    *     {
    *         Lock lock(mutex);
    *         // do some stuff that could throw exceptions...
    *     }
    *
    * private:
    *     Mutex mutex;
    * };
    *
    * This class is not reference-counted; each instance is intended to be
    * encapsulated by one method, as a local variable.
    *
    */
    class Lock
    {
    public:
        /**
        * By default, gets the Mutex's lock.
        */
        Lock(Mutex& mutex, bool autoLock = true) throw(AssertException, exception);

        /**
        * Releases the Mutex's lock.  Does nothing if this Lock is
        * not holding the lock.
        */
        ~Lock() throw();

        /**
        * Gets the Mutex's lock.  Does nothing if this Lock is
        * already holding the lock.
        */
        void get() throw(AssertException, exception);

        /**
        * Releases the Mutex's lock.  Does nothing if this Lock is
        * not holding the lock.
        */
        void release() throw(AssertException, exception);

        /**
        * Waits for another thread to call notify() or notifyAll() on
        * a Lock for the same Mutex.  The calling thread must already
        * have the lock.
        *
        * exception AssertException if NDEBUG was not defined when
        * this class was compiled, and the calling thread does not
        * have the lock.
        *
        */
        void wait() throw(AssertException, exception);

        /**
        * Notifies one thread waiting for this mutex.  The calling
        * thread must already have the lock.
        *
        * exception AssertException AssertException if NDEBUG was not defined when this
        * class was compiled, and the calling thread does not have the lock.
        *
        */
        void notify() throw(AssertException, exception);

        /**
        * Notifies all threads waiting for this mutex.  The calling
        * thread must already have the lock.
        *
        * exception AssertException AssertException if NDEBUG was not defined when this
        * class was compiled, and the calling thread does not have the lock.
        *
        */
        void notifyAll() throw(AssertException, exception);

    private:
        Lock();
        Lock(const Lock&);
        Lock& operator=(Lock&);

        Mutex& mutex;
        bool locked;
    };
}
#endif // MUTEX_H
