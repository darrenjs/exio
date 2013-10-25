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
#include "exio/AdminSession.h"
#include "exio/AdminInterface.h"
#include "exio/Logger.h"
#include "exio/sam.h"
#include "exio/MsgIDs.h"
#include "exio/utils.h"
#include "exio/SamBuffer.h"
#include "exio/Reactor.h"

extern "C"
{
#include <xlog/xlog.h>
}

#include "mutex.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

namespace exio {


struct QueuedItem
{
    /* number of bytes actually used */
    size_t   size;

    enum Flags
    {
      eDeleteAfterUse = 0x01, // not supported
      eThreadKill     = 0x02

    };
    unsigned int flags;

    const char* buf() const;

    QueuedItem();

    void release();

    exio::SamBuffer * sb() { return &m_sb; }

  private:
    exio:: DynamicSamBuffer m_sb;
};


SID SID::no_session = SID();


struct AdminSessionIdGenerator
{
    static cpp11::mutex admin_sessionid_lock;
    static unsigned long long admin_sessionid;

    static unsigned long long next_admin_sessionid()
    {
      cpp11::lock_guard< cpp11::mutex > guard( admin_sessionid_lock );

      return admin_sessionid++;
    }

};

cpp11::mutex AdminSessionIdGenerator::admin_sessionid_lock;
unsigned long long AdminSessionIdGenerator::admin_sessionid = 1;

//----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const SID& id)
{
  os << id.toString();
  return os;
}

std::string SID::toString() const
{
  std::ostringstream os;
  os << m_unqiue_id;
//  os << m_unqiue_id << "." << m_fd;
  return os.str();
}

SID SID::fromString(const std::string& s)
{
  SID retval;
  retval.m_unqiue_id = atoi( s.c_str() );

  // std::vector<std::string> parts = utils::tokenize(s.c_str(), '.',false);

  // if (parts.size() == 2)
  // {
  //   SID retval;
  //   retval.m_unqiue_id = atoll(parts[0].c_str());
  //   retval.m_fd = atoi(parts[1].c_str());
  //   return retval;
  // }
  // else
  // {
  //   return no_session;
  // }

  return retval;
}

//----------------------------------------------------------------------
SID::SID()
  : m_unqiue_id( 0 )
{
}

SID::SID(size_t id)
  : m_unqiue_id( id )
{
}

// SID::SID()
//   : m_unqiue_id( 0 ),
//     m_fd ( 0 )
// {
// }

// SID::SID(unsigned long long id,
//          int fd)
//   : m_unqiue_id( id ),
//     m_fd ( fd )
// {
// }

/*
 * Taken from Unix Network Programming, section 3.8.  Look up sock_ntop.c for
 * full implementation of this function.
 */
std::string sock_ntop(const struct sockaddr * sa,
                      socklen_t salen)
{

  char str[128];

  switch (sa->sa_family)
  {
    case AF_INET:
    {
      struct sockaddr_in * sin = (struct sockaddr_in*) sa;
      if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
        return "";

      std::stringstream os;
      os << str << ":" << ntohs(sin->sin_port);
      return os.str();
    }
    default:
      return "";
  }

}

// TODO: provide better signalling to caller if hostname lookup fails
std::string sock_descr(int fd)
{
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);
  memset(&addr,0,sizeof(addr));

  int __e =  getpeername(fd, (struct sockaddr *) &addr, &addrlen);

  if (!__e)
    return sock_ntop((const struct sockaddr *)&addr, addrlen);
  else
    return "unknown";
}

//----------------------------------------------------------------------
AdminSession::AdminSession(AppSvc& appsvc,
                           int fd,
                           AdminSession::Listener* l,
                           size_t id)
  : m_appsvc( appsvc),
    m_id(id),
    m_fd(fd),
//    m_id(AdminSessionIdGenerator::next_admin_sessionid(), fd),
    m_logon_received(false),
    m_session_valid(true),
    m_peeraddr( sock_descr(fd) ),
    m_listener( l ),  // need store listener before IO started
    m_autoclose(false),
    m_hb_intvl(30),
    m_hb_last(time(NULL)),
    m_start( m_hb_last ),
    m_samp(m_appsvc),
    m_io_handle(NULL)
{
}

//----------------------------------------------------------------------
void AdminSession::init(Reactor* reactor)
{
  // once the client is constructed, we can immediately send data and also
  // receive callbacks (if there is data on the socket).

  // note, we do this in init(), rather than in the constructor, is so that
  // the user of AdminSession can better control when data starts arriving off
  // the socket, and so also when callbacks start going into the rest of the
  // program.

  m_io_handle = new Client(reactor, m_fd, m_appsvc.log(), this);
  reactor->add_client( m_io_handle );
}

//----------------------------------------------------------------------
AdminSession::~AdminSession()
{
  xlog_write1("AdminSession::~AdminSession", __FILE__, __LINE__);

  // TODO: need mutex protection around m_io_handle.  Well, probably is ok,
  // since the write is done in the destructor, ie, should be no more method
  // calls after this.
  if (m_io_handle)
  {
    m_io_handle->release(); // disables callbacks, etc.
    m_io_handle = NULL;
  }
}
//----------------------------------------------------------------------
void AdminSession::log_thread_ids(std::ostream& os) const
{
  if ( m_io_handle )
  {
    os << "task, " << m_io_handle->task_lwp()
       << ", " <<  m_io_handle->task_tid();
  }
}
//----------------------------------------------------------------------

bool AdminSession::enqueueToSend(const sam::txMessage& msg)
{
  if (!m_session_valid) return true; // ignore request if session not valid

  // TODO: reject message if session is closed.
  // TODO: what happens if msg too large to encode?


  // {
  //   sam::MessageFormatter msgfmt;
  //   std::ostringstream os;
  //   msgfmt.format(msg, os);

  //   _INFO_(m_appsvc.log(), "Sending: " << os.str() );
  // }

  QueuedItem qi;
  sam::SAMProtocol protocol(m_appsvc);

  try
  {
    qi.size = protocol.encodeMsg(msg, qi.sb());

    int result;
    if (m_io_handle) result = m_io_handle->queue(qi.buf(), qi.size, false);

    if (result == -1)
    {
      _ERROR_(m_appsvc.log(), "Dropping slow consumer session " << m_id);
      return true;
    }
    /*
      size_t sz = protocol.calc_encoded_size(msg);
      _INFO_(m_appsvc.log(), "estimated " << sz << ", actual " << qi.size);
    */

    return false;  // success
  }
  catch (sam::OverFlow& err)
  {
    // If we arrive here, the message could not be encoded.
    size_t sz = protocol.calc_encoded_size(msg);
    _ERROR_(m_appsvc.log(), "Failed to send "
            << msg.type() <<  " message due to encode exception: "
            << err.what()
            << " (expected size was " << sz << ")");

    // // TODO experimental
    // size_t n = protocol.calcEncodeSize(msg);
    // _INFO_(m_appsvc.log(), "Estimate size required: " << n);

    // size_t tmpsz = (size_t) (n * 1.10);
    // char * tmp = new char[ tmpsz ];
    // memset(tmp, 0, tmpsz);

    // try
    // {
    //   size_t len = protocol.encodeMsg(msg, tmp, tmpsz);
    //   _INFO_(m_appsvc.log(), "Encoding works, used a length of " << len
    //          << ": " << tmp)
    // }
    // catch (...)
    // {
    //   _ERROR_(m_appsvc.log(), "Encoding still did not work");
    // }
    // delete [] tmp;
  }
  catch (std::exception& err)
  {
    _ERROR_(m_appsvc.log(), "Failed to encode/enqueue "
            << msg.type() <<  " message due to exception: "
            << err.what());
  }
  catch (...)
  {
    _ERROR_(m_appsvc.log(), "Failed to encode/enqueue "
            << msg.type() <<  " message due to unknown exception");
  }

  return true; // failure
}

//----------------------------------------------------------------------

void AdminSession::close()
{
  xlog_write1("AdminSession::close", __FILE__, __LINE__);
  if (m_io_handle) m_io_handle->queue(0,0,true);
}

//----------------------------------------------------------------------
/**
 * A SAM message has been read from the client socket.  It represents a client
 * request. So here we start the process of routing it to possibly a client
 * callback.
 */
void AdminSession::io_onmsg(const sam::txMessage& msg)
{
  if (!m_session_valid) return; // ignore request if session not valid

  // We can handle some messages within the session

  // Identify the message
  if (msg.type() == id::msg_logon)
  {
    if (m_logon_received)
    {
      std::ostringstream os;
      os << "Ignoring logon message for session " << m_id
         << ". Logon already occurred.";
      _WARN_(m_appsvc.log(), os.str());
    }
    else
    {
      m_logon_received = true;

      const sam::txField* fsvcid = msg.root().find_field(id::QN_serviceid);
      const sam::txField* fuser  = msg.root().find_field(id::QN_head_user);

      if (!fsvcid)
      {
        _WARN_(m_appsvc.log(), "Logon for session " << m_id
               << " is missing field " << id::QN_serviceid);
      }
      else
      {
        m_serviceid = fsvcid->value();
      }
      if (!fuser)
      {
        _WARN_(m_appsvc.log(), "Logon for session " << m_id
               << " is missing field " << id::QN_head_user);
      }
      else
      {
        m_username = fuser->value();
      }

      std::ostringstream os;
      os << "Received logon for session "
         << m_id << ", service-id '" << m_serviceid << "', "
         << "username '" << m_username << "'";
      _INFO_(m_appsvc.log(), os.str());
    }

    /* NOTE: we allow logon to continue further up the stack */
  }

  if (m_listener)
  {
    m_listener->session_msg_received( msg, *this);
  }
}

//----------------------------------------------------------------------

void AdminSession::process_close(Client*)  // callback, from IO
{
  _INFO_(m_appsvc.log(), "AdminSession:process_close");
  m_session_valid = false;

  m_listener->session_closed( *this );

  // lets forget about our listener
  m_listener = NULL;
}

//----------------------------------------------------------------------

bool AdminSession::safe_to_delete() const
{
  /* we only consider safe to delete once the io has been closed */
  return (m_session_valid==false and
          ((m_io_handle and m_io_handle->io_open() == false) or (m_io_handle==NULL))
    );
}

//----------------------------------------------------------------------
void AdminSession::housekeeping()
{
  if ((m_hb_intvl==0) or
      (m_io_handle==NULL) or
      (not m_io_handle->io_open())) return;

  time_t now = time(NULL);

  // Note: we now send heartbeats periodically, irrespective of outbound
  // activity on the session.  We are doing this because our heartbeats also
  // serve as tests-requests, so the peer will take our heartbeat and reply to
  // it.
  if (abs(now - m_hb_last) >= m_hb_intvl)
  {
    // we are sending an unsolicited heartbeat, so, also include the
    // testrequest flag to request a reply from the peer.
    sam::txMessage hbmsg(id::heartbeat);
    hbmsg.root().put_field(id::QN_testrequest, id::True );
    enqueueToSend( hbmsg );
    m_hb_last = now;
  }
}
//----------------------------------------------------------------------
time_t AdminSession::start_time() const
{
  return m_start;
}
//----------------------------------------------------------------------
time_t  AdminSession::last_write() const
{
  return (m_io_handle)? m_io_handle->last_write():0;
}
//----------------------------------------------------------------------
uint64_t AdminSession::bytes_out()  const
{
  return (m_io_handle)? m_io_handle->bytes_out():0;
}
//----------------------------------------------------------------------
uint64_t AdminSession::bytes_in()   const
{
  return (m_io_handle)? m_io_handle->bytes_in():0;
}
//----------------------------------------------------------------------
uint64_t AdminSession::bytes_pend() const
{
  return (m_io_handle)? m_io_handle->pending_out():0;
}
//----------------------------------------------------------------------
int AdminSession::fd() const
{
  return (m_io_handle)? m_io_handle->fd():0;
}
//----------------------------------------------------------------------
size_t AdminSession::process_input(Client*, const char* src, int size)  // callback, from IO
{
  sam::txMessage msg;
  size_t consumed = 0;

  /* TODO: need to carefully review this method */

  try
  {
    // TODO: this could either work, or, fail, or partly fail.  For the partly
    // fail, maybe we should make sure we can decode the initial successfully decoded message?

    consumed = m_samp.decodeMsg(msg, src, size);
  }
  catch (const std::exception& e)
  {


    _ERROR_(m_appsvc.log(),
            "Closing connection due to protocol error: " << e.what());

    // send error reply
    sam::txMessage badmsg("badmsg"); // TODO: would be nice to include an error string?
    badmsg.root().put_field("msg", e.what());

    // TODO: this sending of a badmsg reply has not yet been tested
    try
    {
      enqueueToSend( badmsg );
    }
    catch (...) {  }

    if (m_io_handle) m_io_handle->queue(0, 0, true); // this will close the IO
  }

  /* raw data has been decoded, so now pass to the client */
  if (consumed)
  {
    try
    {
      this->io_onmsg( msg );
    }
    catch(const std::exception& err)
    {
      _ERROR_(m_appsvc.log(), "exception when processing message: " << err.what());
    }
    catch(...)
    {
      _ERROR_(m_appsvc.log(), "unknown exception when processing message");
    }
  }

  return consumed;
}
//----------------------------------------------------------------------

} // namespace qm
