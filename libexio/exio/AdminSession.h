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
#ifndef EXIO_ADMINSESSION_H
#define EXIO_ADMINSESSION_H

#include "exio/AdminSessionID.h"
#include "exio/Client.h"
#include "exio/sam.h"

#include <list>
#include <iostream>

#include <stdint.h>

namespace sam
{
  class txMessage;
}

namespace exio
{

class AdminInterface;
class AppSvc;
class Reactor;


  class AdminSession : public ClientCallback
{
  public:

    /* Warning: the callback methods will be invoked by the AdminSession's
     * socket reader thread. Don't use that thread, during one of the
     * callbacks, to delete the actual AdminSession self instance! E.g. on a
     * closed() callback, it is tempting to call delete on the session (if it
     * was allocated on the heap).  However, don't do it!
     */
    class Listener
    {
      public:
        virtual ~Listener() {};
        virtual void session_msg_received(const sam::txMessage&,
                                          AdminSession&) = 0;
        virtual void session_closed(AdminSession& session) = 0;
    };

  public:
    AdminSession(AppSvc&,
                 int fd,
                 Listener* l,
                 size_t id);
    ~AdminSession();

    void init(Reactor* reactor);

    /* Error code indicates success.  Zero/false means no error.  Other
     * returns value indicates encoding or IO problem. */
    bool enqueueToSend(const sam::txMessage&);

    /* Request session to close */
    void close();

    bool is_open() const { return m_session_valid; }

    bool safe_to_delete() const;

    const SID& id() const { return m_id; }

    const std::string & peer_serviceid() const { return m_serviceid; }


    void wants_autoclose(bool b) { m_autoclose = b; }
    bool wants_autoclose() const { return m_autoclose; }


    virtual void io_onmsg(const sam::txMessage& src) ;

    void housekeeping();

    const std::string& username() const { return m_username; }
    const std::string& peeraddr() const { return m_peeraddr; }

    bool logon_recevied() const { return m_logon_received; }

    void log_thread_ids(std::ostream&) const;

    int fd() const;

    // IO stats
    time_t start_time() const;
    time_t last_write() const;
    uint64_t bytes_out()  const;
    uint64_t bytes_in()   const;
    uint64_t bytes_pend() const;

  protected:
    virtual size_t process_input(Client*, const char*, int);
    virtual void   process_close(Client*);

  private:

    void notify_of_close();

  private:
    AdminSession(const AdminSession &);  // no copy
    AdminSession & operator=(const AdminSession &); // no assignment

    AppSvc& m_appsvc;
    SID  m_id;
    std::string m_serviceid;  // what peer has provided via Logon
    int m_fd;

    bool m_logon_received;
    bool m_session_valid;

    std::string m_username;
    std::string m_peeraddr;

    Listener* m_listener;

    bool m_autoclose;

    /* Interval, in secs, at which to send heartbeats. Values below 30 seconds
     * might not be too reliable, because the underlying housekeeping timer
     * has around a 20 second precision. */
    int m_hb_intvl;
    time_t m_hb_last;

    time_t    m_start;

    sam::SAMProtocol m_samp;

    Client*  m_io_handle;

};

  std::ostream & operator<<(std::ostream&, const SID & id);

}

#endif
