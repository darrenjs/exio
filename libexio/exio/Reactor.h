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

#ifndef EXIO_REACTOR_H
#define EXIO_REACTOR_H

#include "exio/Client.h"

#include "thread.h"
#include "atomic.h"

#include <set>

namespace exio {

  class LogService;
  class ReactorMsg;
  class ReactorNotifQ;


class Reactor
{
  public:
    Reactor(LogService*, int nworkers=2);
    ~Reactor();

    void add_client(ReactorClient*);

//    void request_close(ReactorClient*);
//    void request_shutdown(ReactorClient*);
//    void request_release(ReactorClient*);
    void request_attn();

    void invalidate();


  private:
    Reactor(const Reactor&); // no copy
    Reactor& operator=(const Reactor&); // no assignment

    void reactor_main_loop();
    void reactor_io_TEP();

    void worker_TEP();
    bool worker_TEP_impl();

    void handle_reactor_msg(const ReactorMsg&);
    void attend_clients();

    LogService* m_log;
    cpp11::atomic_bool m_is_stopping;

    int m_pipefd[2]; // TODO: move into the NotifQ


    std::vector<ReactorClient*> m_clients;
    std::set<ReactorClient*>    m_destroying;
    time_t m_last_cleanup;

    ReactorNotifQ * m_notifq;
    cpp11::thread * m_io;


    struct RunQ
    {
        cpp11::mutex                  mutex;
        std::list <ReactorClient* >  items;
        size_t                        itemcount;
        cpp11::condition_variable     cond;
        RunQ () : itemcount(0) {}
    } m_runq;


    std::vector<cpp11::thread*> m_workers;
};

} // namespace exio

#endif

