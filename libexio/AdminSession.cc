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


#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

namespace exio {


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
  os << m_unqiue_id << "." << m_fd;
  return os.str();
}

SID SID::fromString(const std::string& s)
{
  std::vector<std::string> parts = utils::tokenize(s.c_str(), '.',false);

  if (parts.size() == 2)
  {
    SID retval;
    retval.m_unqiue_id = atoll(parts[0].c_str());
    retval.m_fd = atoi(parts[1].c_str());
    return retval;
  }
  else
  {
    return no_session;
  }
}

//----------------------------------------------------------------------
SID::SID()
  : m_unqiue_id( 0 ),
    m_fd ( 0 )
{
  // TODO: I want to make the ID to be even safer.  So add some extra fields:  port and time
}

SID::SID(unsigned long long id,
         int fd)
  : m_unqiue_id( id ),
    m_fd ( fd )
{
}

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
                           AdminSession::Listener* l)
  : m_appsvc( appsvc),
    m_id(AdminSessionIdGenerator::next_admin_sessionid(), fd),
    m_logon_received(false),
    m_session_valid(true),
    m_peeraddr( sock_descr(fd) ),
    m_listener( l ),  // need store listener before IO started
    m_autoclose(false),
    m_hb_intvl(60),
    m_start(time(NULL)),
    m_io( new AdminIO(m_appsvc, fd, this ))
{
}
//----------------------------------------------------------------------
AdminSession::~AdminSession()
{
  if ( not m_io->safe_to_delete() )
  {
    _WARN_(m_appsvc.log(),
           "Deleting session IO object, but it is not yet safe to delete!");
  }

  delete m_io;
}
//----------------------------------------------------------------------
void AdminSession::log_thread_ids(std::ostream& os) const
{
  os << "reader (LWP " << m_io->reader_lwp()
     << " / pthread " << m_io->reader_pth()  << ") "
     << "writer (LWP " << m_io->writer_lwp()
     << " / pthread " << m_io->writer_pth() << ")";
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

  if (m_io)
  {
    QueuedItem qi;
    sam::SAMProtocol protocol;
    try
    {
      qi.size = protocol.encodeMsg(msg, qi.buf(), qi.capacity());

      /*
      size_t sz = protocol.calc_encoded_size(msg);
      _INFO_(m_appsvc.log(), "estimated " << sz << ", actual " << qi.size);
      */

      m_io->enqueue( qi );
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
  }
  else
  {
    _ERROR_(m_appsvc.log(), "Failed to encode/enqueue "
            << msg.type() <<  " message because IO object not available");

  }

  return true; // failure
}

//----------------------------------------------------------------------

void AdminSession::close()
{
  m_io->request_stop();
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

  if (m_listener) m_listener->session_msg_received( msg, *this);
}

//----------------------------------------------------------------------

void AdminSession::io_closed()
{
  m_session_valid = false;

  m_listener->session_closed( *this );

  // lets forget about our listener
  m_listener = NULL;
}

//----------------------------------------------------------------------

bool AdminSession::safe_to_delete() const
{
  return (m_session_valid==false and m_io->safe_to_delete());
}

//----------------------------------------------------------------------
void AdminSession::housekeeping()
{
  if ((!m_io) or (m_hb_intvl==0)) return;

  time_t lastactivity = m_io->last_write();
  time_t now = time(NULL);

  // note, 'm_last_send' is more or less 'current time'
  if (abs(now - lastactivity) >= m_hb_intvl)
  {
    enqueueToSend( sam::txMessage(id::heartbeat) );
  }
}

//----------------------------------------------------------------------


} // namespace qm
