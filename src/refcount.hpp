/*
 * $Id$
 *
 * aprsd, Automatic Packet Reporting System Daemon
 * Copyright (C) 1997,2002 Dale A. Heatherington, WA4DSY
 * Copyright (C) 2001-2004 aprsd Dev Team
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


#ifndef REFCOUNT_HPP
#define REFCOUNT_HPP

#include <iostream>
#include <assert.h>

#include "mutex.hpp"

namespace aprsd {
class BaseRefHandle;

/**
   All elements controlled by the RefHandler class must enherit from this class
   to enable the propper counter. There is no need to know anything about this
   class other than it need to be part of the class you like to make referance
   counting onto.

   @see RefHandle
*/
class RefCounted {

    friend class BaseRefHandle;

protected:
    virtual ~RefCounted() = 0;

public:
    explicit RefCounted( void ) : m_nNbppRefCount( 0 ), m_bDestroying( false ) {}
    RefCounted( const RefCounted & ) : m_nNbppRefCount( 0 ), m_bDestroying( false ) {}

    /// Peek the ref count value, not really thread safe !
    int peekRefCount( void ) const {return m_nNbppRefCount;}

    // RefCounted &operator=( const RefCounted & ) {return *this;}
private:
    void lock( void ) {
        Lock sync( m_mutex );

        if( m_bDestroying )
            throw RefCountException( "Can't lock a destructed data object." );

        m_nNbppRefCount++;
    }

    void release( void ) {
        Lock sync( m_mutex );

        if( --m_nNbppRefCount == 0 ) {
            m_bDestroying = true;
            sync.release();
            delete this;    // Are we in danger here, or does'nt out refcounter help us out ?
        }
    }

    int m_nNbppRefCount;
    Mutex m_mutex;
    bool m_bDestroying;
};

    inline RefCounted::~RefCounted() {}

class BaseRefHandle {
public:
    BaseRefHandle( RefCounted *pRef ) : m_pRef( pRef ) {
        if( m_pRef ) {
            m_pRef->lock();
        }
    }

    virtual ~BaseRefHandle() {
        if( m_pRef  )
            m_pRef->release();
    }

    BaseRefHandle( const BaseRefHandle &refHndl ) {
        m_pRef = refHndl.m_pRef;

        if( m_pRef )
            m_pRef->lock();
    }

    BaseRefHandle &operator=( const BaseRefHandle &refHndl ) {
        if( m_pRef != refHndl.m_pRef ) {
            RefCounted *pOld = m_pRef;

            m_pRef = refHndl.m_pRef;

            if( m_pRef )
                m_pRef->lock();

            if( pOld )
                pOld->release();
        }
        return *this;
    }

protected:
    RefCounted *m_pRef;
};

/**
   A referance handler class taking a class that at least inherits from the
   RefCounted class, but can be anything specified in the template parameter.

   If more that one handler have a reference to the same RefCounted data, they do
   not need to be aware of this fact, as the counter ellement is to be found on the
   RefCounted class.

   @see RefCounted
*/
template<class T> class RefHandle : protected BaseRefHandle {
 public:
    /**
       Make a new handler, and if pRef is set make a reference to this class.

       @param pRef
     */
    RefHandle( T *pRef = NULL ) : BaseRefHandle( pRef ) {}

    /**
       Make a new handle and a referende to the data pointed to by hndl.
     */
    RefHandle( const RefHandle<T> &hndl ) : BaseRefHandle( hndl ) {}

    /**
       Copy and refere to the data pointed to by hndl, and release data
       holed onto by this handler.
     */
    RefHandle<T> &operator=( const RefHandle<T> &hndl ) {
        *((BaseRefHandle *)this) = hndl;

        return *this;
    }

    /**
       Converts a RefHandle<T>; into a RefHandle<T2>.  The conversion will compile if a T*
       can be converted to a T2*, e.g. if T is a class derived from T2.
     */
    template<class T2> operator RefHandle<T2>() const {
        return RefHandle<T2>( get() );
    }

    /**
       Does the same as operator ->, as it get a pointer to the refered handler.
     */
    T *get( void ) const throw() { return (T *)m_pRef;}

    /**
       Return refered data as reference.
     */
    T &operator *() const {return *((T *)m_pRef);}

    /**
       Return refered data as a pointer.
     */
    T *operator ->() const throw() {return (T *)m_pRef;}

    operator bool( void ) const {return m_pRef;}
};
};
#endif
