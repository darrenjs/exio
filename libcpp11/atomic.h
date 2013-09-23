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
#ifndef CPP11_ATOMIC_H
#define CPP11_ATOMIC_H

namespace cpp11
{

/* make this look like C++ */
class atomic_bool
{
    bool m_value;
    mutable cpp11::mutex m_mutex;

  public:

             atomic_bool()       : m_value(false) {}
    explicit atomic_bool(bool b) : m_value(b) {}

    bool operator=(bool __i)
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
      m_value = __i;
      return m_value;
    }

    operator bool() const
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
      return  m_value;
    }

  private:

    // atomic types are not CopyConstructable
    atomic_bool(const atomic_bool &);

    // atomic types are not CopyAssignable
    atomic_bool& operator=(const atomic_bool&);
};
}

#endif
