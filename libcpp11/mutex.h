/*
    Copyright 2013, Darren Smith

    This file is part of exio, a library for providing administration,
    monitoring and alerting capabilities to an application.

    exio is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    exio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with exio.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CPP11_MUTEX_H
#define CPP11_MUTEX_H

#include <stdexcept>

namespace cpp11
{


/// @brief  Scoped lock idiom.
// Acquire the mutex here with a constructor call, then release with
// the destructor call in accordance with RAII style.
// NOTE: this is mostly copied from C++11 source code.
template<typename _Mutex>
class lock_guard
{
  public:
    typedef _Mutex mutex_type;

    explicit lock_guard(mutex_type& __m) : _M_device(__m)
    { _M_device.lock(); }


    ~lock_guard()
    { _M_device.unlock(); }

  private:
    lock_guard(const lock_guard&) ;
    lock_guard& operator=(const lock_guard&) ;

  private:
    mutex_type&  _M_device;
};


/**
 * This is like lock_guard, providing RAII semantics, but, also allows unlocking.
 * Code is mostly copied from the C++11 source.
 */
template<typename _Mutex>
class unique_lock
{
  public:
    typedef _Mutex mutex_type;

    unique_lock()
      : _M_device(0), _M_owns(false)
    { }

    explicit unique_lock(mutex_type& __m)
      : _M_device(&__m),
        _M_owns(false)
    {
      lock();
      _M_owns = true;
    }

    ~unique_lock()
    {
      if (_M_owns) unlock();
    }


    void unlock()
    {
      if (!_M_owns)
      {
        // we have not locked anything!
        throw std::runtime_error("lock not owned");
      }
      else if (_M_device)
	  {
	    _M_device->unlock();
	    _M_owns = false;
	  }
    }

    void lock()
    {
      if (!_M_device)
        throw std::runtime_error("no mutex available");
      else if (_M_owns)
        throw std::runtime_error("deadlock would occur");
      else
	  {
	    _M_device->lock();
	    _M_owns = true;
	  }
    }

    /* Are we associated with a locked mutex? */
    bool owns_lock() const
    {
      return _M_owns;
    }

    mutex_type* mutex() const
    {
      return _M_device;
    }

  private:
    unique_lock(const unique_lock&);
    unique_lock& operator=(const unique_lock&);

    mutex_type*	_M_device;
    bool _M_owns;
};


class mutex
{
    typedef pthread_mutex_t __native_type;
    pthread_mutex_t m_mutex;

  public:

    typedef __native_type* native_handle_type;

    mutex();
    ~mutex();

    void lock();
    void unlock();


    native_handle_type native_handle()
    {
      return &m_mutex;
    }

  private:
    /*
     * This classes uses pthread mutexes, which cannot be copied, so prevent
     * copy/assignment of this class.
     */
    mutex(const mutex&);
    mutex& operator=(const mutex&);

};


class recursive_mutex
{
    typedef pthread_mutex_t __native_type;
    pthread_mutex_t m_mutex;

  public:

    typedef __native_type* native_handle_type;

    recursive_mutex();
    ~recursive_mutex();

    void lock();
    void unlock();

    native_handle_type native_handle() { return &m_mutex; }

  private:
    /*
     * This classes uses pthread recursive_mutexes, which cannot be copied, so prevent
     * copy/assignment of this class.
     */
    recursive_mutex(const recursive_mutex&);
    recursive_mutex& operator=(const recursive_mutex&);

};


}

#endif
