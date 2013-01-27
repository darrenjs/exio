#include "mutex.h"

#include <pthread.h>

namespace cpp11
{
//----------------------------------------------------------------------
mutex::mutex()
{
  // according to POSIX Threads, non-static mutexes must be initialised using
  // the function: pthread_mutex_init. It is only for static mutex instances
  // that the PTHREAD_MUTEX_INITIALIZER can be used.
  pthread_mutex_init(&m_mutex, NULL);  // TODO: test retval
}
//----------------------------------------------------------------------
mutex::~mutex()
{
  // Note: in C++11, if a mutex is destroyed while owned by some thread the
  // behaviour is undefined.  This is likely because the underlying posix
  // mutex has undefined behaviour too.

  // release our mutex resource -- note, it is an error to release a locked
  // mutex, so caller beware!
  pthread_mutex_destroy(&m_mutex);
}
//----------------------------------------------------------------------
void mutex::lock()
{
  int e = pthread_mutex_lock( &m_mutex );
  if (e) throw std::runtime_error("pthread_mutex_lock failed");
}
//----------------------------------------------------------------------
void mutex::unlock()
{
  pthread_mutex_unlock( &m_mutex );
}
//----------------------------------------------------------------------
}
