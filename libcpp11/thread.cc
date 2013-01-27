#include "thread.h"

#include <stdexcept>
#include <sstream>
#include <errno.h>
#include <iostream>

namespace cpp11
{
  extern "C" void*
  execute_native_thread_routine(void* __p)
  {
    thread::_Impl_base* impl = static_cast<thread::_Impl_base*>(__p);

     // thread::__shared_base_type __local;
    // __local.swap(__t->_M_this_ptr);

    try
    {
      // invoke the callback
      impl -> _M_run();
    }
    catch(...)
    {
      std::terminate();
    }

    // we have taken ownership of the thread::_Impl_base object, so we must
    // delete it
    delete impl;

    return 0;
  }

//----------------------------------------------------------------------
thread::~thread()
{
  // if thread is still joinable, then call std::terminate.  This is because we
  // treat it as an error is the programmer has forgotten to join to a thread
  // before that thread is deleted (ie, has its destructor called).
  if (joinable()) std::terminate();
}
//----------------------------------------------------------------------
void thread::start_thread(_Impl_base * p)
{
  pthread_attr_t thread_attr;

  if (pthread_attr_init(&thread_attr) != 0)
    throw std::runtime_error("pthread_attr_init failed");

  // NOTE: default state of a pthread is JOINABLE
  if (pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE) != 0)
    throw std::runtime_error("pthread_attr_setdetatchstate failed");

  // TODO: need to interograte thread result
  int __e = pthread_create(&_M_id._M_thread,
                           &thread_attr,
                           &execute_native_thread_routine,
                           p);
  if (__e)
  {
    std::ostringstream os;
    os << "pthread_create failed, returned " << __e;
    throw std::runtime_error( os.str() );
  }
  pthread_attr_destroy( &thread_attr );
}
//----------------------------------------------------------------------
void thread::detach()
{
  int __e = EINVAL;

  if ( joinable() )
  {
    // Good, our underlying TOE has not already been detached, so we can go
    // ahead an detach here.
    __e = pthread_detach(_M_id._M_thread);
  }

  if (__e)
  {
    // something went wrong... either the detach operation failed, or,
    // detach() has been called on a thread that has already been detached

    throw std::runtime_error("thread::detach failed");
    // __throw_system_error(__e);   <===== C++11 does this
  }

  // invalidate our thread handle
  _M_id = id();
}

//----------------------------------------------------------------------
void thread::join()
{
  int __e = EINVAL;

  /* We will only attempt to join if we still have a handle to the thread.  We
     have lost control over our thread if either we (1) have detached it, or
     (2) have previously joined to it (in which case the thread nolonger
     exists!)
  */
  if ( joinable() )
  {
    __e = pthread_join(_M_id._M_thread, NULL);
  }
  else
  {
  }

  if (__e)
  {
    // something went wrong... either the join operation failed, or,
    // join() has been called on a thread that cannot be joined
    std::ostringstream os;
    os << "pthread_join failed, returned " << __e;
    throw std::runtime_error( os.str() );
    // __throw_system_error(__e);   <===== C++11 does this
  }

  // our thread no longer exists, so invalid the handle
  _M_id = id();
}

} // namespace
