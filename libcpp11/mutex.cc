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
