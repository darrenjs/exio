#ifndef EXIO_ADMINSESSION_H
#define EXIO_ADMINSESSION_H

#include "exio/AdminIO.h"
#include "exio/AdminSessionID.h"

#include <list>
#include <iostream>

namespace sam
{
  class txMessage;
}

namespace exio
{

class AdminInterface;


class AdminSession : public AdminIO::Listener
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
                 Listener* l);
    ~AdminSession();

    /* Error code indicates success.  Zero/false means no error.  Other
     * returns value indicates encoding or IO problem. */
    bool enqueueToSend(const sam::txMessage&);

    void close_io();

    bool is_open() const { return m_session_valid; }

    bool safe_to_delete() const;

    const SID& id() const { return m_id; }

    const std::string & peer_serviceid() const { return m_serviceid; }


    void wants_autoclose(bool b) { m_autoclose = b; }
    bool wants_autoclose() const { return m_autoclose; }


    virtual void io_onmsg(const sam::txMessage& src) ;
    virtual void io_closed();

    void housekeeping();

    const std::string& username() const { return m_username; }
    const std::string& peeraddr() const { return m_peeraddr; }

    bool logon_recevied() const { return m_logon_received; }

    void log_thread_ids(std::ostream&) const;

    // IO stats
    time_t        start_time() const { return m_start; }
    time_t        last_write() const { return (m_io)? m_io->last_write():0; }
    unsigned long bytes_out()  const { return (m_io)? m_io->bytes_out():0; }
    unsigned long bytes_in()   const { return (m_io)? m_io->bytes_in():0; }

  private:

    void notify_of_close();

  private:
    AdminSession(const AdminSession &);  // no copy
    AdminSession & operator=(const AdminSession &); // no assignment

    AppSvc& m_appsvc;
    SID  m_id;
    std::string m_serviceid;  // what peer has provided via Logon
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

    time_t    m_start;

    AdminIO * m_io;

};

  std::ostream & operator<<(std::ostream&, const SID & id);

} // qm

#endif
