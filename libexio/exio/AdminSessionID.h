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
#ifndef EXIO_ADMINSESSIONID_H
#define EXIO_ADMINSESSIONID_H

#include <string>

namespace exio
{

struct sid_desc
{
    std::string username;
    std::string peeraddr;
};

/* Unique Session ID provided to each AdminSession */
class SID
{
  public:

    static SID no_session;

    SID();  // default value represents invalid session ID

    SID(unsigned long long id,
       int fd);

    bool operator==(SID rhs) const
    {
      return (this->m_unqiue_id == rhs.m_unqiue_id)
        and  (this->m_fd == rhs.m_fd);
    }

    bool operator!=(SID rhs) const
    {
      return (this->m_unqiue_id != rhs.m_unqiue_id)
        or   (this->m_fd        != rhs.m_fd);
    }


    bool operator<(SID rhs) const
    {
      return
        (this->m_unqiue_id < rhs.m_unqiue_id)
        or
        (this->m_unqiue_id == rhs.m_unqiue_id and this->m_fd < rhs.m_fd);
    }

    // Textual description of the connection address
    std::string toString() const;

    static SID fromString(const std::string&);

  private:
    unsigned long long  m_unqiue_id;
    int m_fd;
};


} // namespace exio

#endif
