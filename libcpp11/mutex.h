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


}

#endif