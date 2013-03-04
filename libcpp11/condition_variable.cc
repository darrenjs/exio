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
