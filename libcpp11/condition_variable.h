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
#ifndef CPP11_CONDITION_VARIABLE_H
#define CPP11_CONDITION_VARIABLE_H

#include "mutex.h" // unique_lock

namespace cpp11 {

class condition_variable
{
    typedef pthread_cond_t __native_type;
    __native_type _M_cond;

  public:
    typedef __native_type* 		native_handle_type;

    condition_variable();
    ~condition_variable();

    void wait( unique_lock<mutex>& __lock );
    void notify_one();
    void notify_all();

    native_handle_type
    native_handle()
    { return &_M_cond; }

  private:
    condition_variable(const condition_variable&); // no copy
    condition_variable& operator=(const condition_variable&); // no assignment
};

} // namespace qm

#endif
