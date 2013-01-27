#include "condition_variable.h"

#include <iostream>


namespace cpp11 {

/* Constructor */
condition_variable::condition_variable()
{
  int __e = pthread_cond_init(&_M_cond, NULL);

  if (__e)
    throw std::runtime_error("pthread_cond_init failed");
}

/* Destructor */
condition_variable::~condition_variable()
{
  pthread_cond_destroy(&_M_cond);
}

void condition_variable::wait( unique_lock<mutex>& __lock )
{
  /*
    The condition-variable wait call is an atomic operation that encompasses
    two steps:

      (1) release the mutex

      (2) block on the condition variable.

    Here atomically means "atomically with respect to access by another thread
    to the mutex and then the condition variable".

   */
  int __e = pthread_cond_wait(&_M_cond, __lock.mutex()->native_handle());

  if (__e)
  {
    std::cout << "pthread_cond_wait failed\n";
    throw std::runtime_error("pthread_cond_wait failed");
  }
}

void condition_variable::notify_all()
{
  int __e =pthread_cond_broadcast (&_M_cond);

  if (__e)
  {
    std::cout << "pthread_cond_broadcast failed\n";
    throw std::runtime_error("pthread_cond_broadcast failed");
  }
}
void condition_variable::notify_one()
{
  int __e = pthread_cond_signal(&_M_cond);

  if (__e)
  {
    std::cout << "pthread_cond_signal failed\n";
    throw std::runtime_error("pthread_cond_signal failed");
  }
}

} // namespace qm
