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
#ifndef EXIO_ADMINSERVERSOCKET_H__
#define EXIO_ADMINSERVERSOCKET_H__

#include "thread.h"

#include <ostream>

namespace exio
{

class AdminInterfaceImpl;

class AdminServerSocket
{
  public:
    AdminServerSocket(AdminInterfaceImpl* ai);

    /* Start listening on the server socket. */
    void start();

    void log_thread_ids(std::ostream&) const;

  private:

    void create_listen_socket();

    /* Thread entry point for thread that will accept new socket
     * connections. */
    void accept_TEP();

  private:
    AdminServerSocket(const AdminServerSocket&);
    AdminServerSocket& operator=(const AdminServerSocket&);

  private:

    AdminInterfaceImpl* m_aii;
    int m_servfd;
    int m_threadid;
    pthread_t m_pthreadid;

//    qm::Thread m_acceptThr;
};

}

#endif
