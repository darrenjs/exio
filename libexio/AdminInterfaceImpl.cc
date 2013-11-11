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
#include "exio/AdminInterfaceImpl.h"
#include "exio/AdminInterface.h"
#include "exio/AdminCommand.h"
#include "exio/Logger.h"
#include "exio/MsgIDs.h"
#include "exio/utils.h"
#include "exio/Reactor.h"

#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define MIN_SECS   (60)
#define HOUR_SECS  (60 * 60)
#define DAY_SECS   (24 * 60 * 60)

namespace exio {

//----------------------------------------------------------------------
// comparison, not case sensitive.
bool compare_nocase(std::string lhs, std::string rhs)
{
  return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
}

//----------------------------------------------------------------------

AdminInterfaceImpl::AdminInterfaceImpl(AdminInterface * ai)
  : m_appsvc(ai->appsvc()),
    m_logsvc( ai->appsvc().log() ),
    m_svcid(ai->appsvc().conf().serviceid),
    m_serverSocket(this),
    m_monitor(this),
    m_start_time( ::time(NULL) ),
    m_ai(ai),
    m_reactor( new Reactor(ai->appsvc().log()))
{
  m_sessions.createdCount = 0;
  m_sessions.next = 1;

  /* Reset the session registry */
  for (size_t i = 0; i < SESSION_REG_SIZE; ++i)
  {
    SessionReg::reset(&(m_sessions.reg[i]));
  }


  /*
   * NOTE: design principle here: we should not add admins which modify table
   * data (eg, clear_table).  Management of table data is the responsibility
   * of the application code.
   */


  std::map<std::string,std::string> adminattrs;
  adminattrs[ "hidden" ] = "false";

  // TODO: to these admins ever get deleted?

  /* Register some admin capabilities of an admin interface */
  admin_add( AdminCommand("table",
                          "query table information",
                          "table [list|show] [tablename]",
                          &AdminInterfaceImpl::admincmd_list_tables, this,
                          adminattrs) );

  // admin_add( AdminCommand("sub",
  //                         "subscribe", "",
  //                         &AdminInterfaceIm
  //                         pl::admincmd_subscribe, this,
  //                         adminattrs) );

  admin_add( AdminCommand("sessions",
                          "list active sessions", "",
                          &AdminInterfaceImpl::admincmd_sessions, this,
                          adminattrs) );

  admin_add( AdminCommand("help",
                          "list admins available", "",
                          &AdminInterfaceImpl::admincmd_help, this,
                          adminattrs) );

  admin_add( AdminCommand("info",
                          "list server information", "",
                          &AdminInterfaceImpl::admincmd_info, this,
                          adminattrs) );

  admin_add( AdminCommand("snapshot",
                          "broadcast a table snapshot", "",
                          &AdminInterfaceImpl::admincmd_snapshot, this,
                          adminattrs) );

  admin_add( AdminCommand("diags",
                          "dump exio diagnostics",
                          "diags [sessions|threads|tables]",
                          &AdminInterfaceImpl::admincmd_diags, this,
                          adminattrs) );

  // admin_add( AdminCommand("table_subs",
  //                         "list subscribers for each table", "",
  //                         &AdminInterfaceImpl::admincmd_table_subs, this,
  //                         adminattrs) );

  admin_add( AdminCommand("drop_session",
                          "force a session disconnect", "",
                          &AdminInterfaceImpl::admincmd_del_session, this,
                          adminattrs) );

}

//----------------------------------------------------------------------

AdminInterfaceImpl::~AdminInterfaceImpl()
{
  delete m_reactor;
}

//----------------------------------------------------------------------

void AdminInterfaceImpl::session_closed(AdminSession& session)
{
  /* Note in here we first remove any usage of the session, e.g., removing it
   * from any subscriptions, before actually removing the sessions from the
   * active list.  This ordering is important.  If we do it the other way, ie,
   * remove from the active-list following by removing usage/subscriptions,
   * the session can still be attempted to be used (eg used for sending table
   * updates), which will lead to lots of warnings about session not found
   * etc. */

  // Remove session from any table subscriptions
  m_monitor.unsubscribe_all( session.id() );

  // Remove the session from the active list
  {
    cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

    // free the slot
    SessionReg::reset( &( m_sessions.reg[session.id().unique_id()]) );

    // std::map< SID, AdminSession* >::iterator i =
    //   m_sessions.items.find( session.id() );

    // if ( i != m_sessions.items.end() )
    // {
    //   m_sessions.items.erase( i );
    // }
    // else
    // {
    //   /* oh dear, we failed to find our session based on lookup. now lets try
    //    * searching for its address */
    //   for (i=m_sessions.items.begin(); i != m_sessions.items.end(); ++i)
    //   {
    //     if (i->second == &session)
    //     {
    //       m_sessions.items.erase( i );
    //       break;
    //     }
    //   }
    // }
  }

  // Now add the session to the expired list. First take a copy of the
  // session-id of the session being closed, because we don't know for how
  // long the session object itself will remain valid, i.e., it could be
  // deleted immediately following the exit of the following critical section.
  SID sessionidclosed = session.id();

  {
    cpp11::lock_guard<cpp11::mutex> guard(m_expired_sessions.lock);
    m_expired_sessions.items.push_back( &session );
  }

  _INFO_(m_logsvc,"Session " << sessionidclosed << " closed");
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::session_cleanup()
{
  std::list< AdminSession* > delete_later;

  cpp11::lock_guard<cpp11::mutex> guard(m_expired_sessions.lock);

  for (std::list< AdminSession* >::iterator i =
         m_expired_sessions.items.begin();
       i != m_expired_sessions.items.end(); ++i)
  {
    AdminSession* s = *i;
    try
    {
      if ( s->safe_to_delete())
      {
//        _INFO_(m_logsvc, "Deleting session " << i->second->id() );
        delete s;

        // LESSON: I used to reset the iterator to begin(), but sometimes
        // failed...
      }
      else
        delete_later.push_back( s );
    }
    catch (...)
    {
      _WARN_(m_logsvc,"ignoring exception thrown from AdminSession destructor");
    }
  }

  m_expired_sessions.items = delete_later;

  if ( m_expired_sessions.items.size() > 0)
  {
    _WARN_(m_logsvc,"Expired sessions not ready to delete, n=" << m_expired_sessions.items.size() );
  }
}

//----------------------------------------------------------------------
size_t AdminInterfaceImpl::session_count() const
{
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  size_t count = 0;

  for (size_t i = 0; i < SESSION_REG_SIZE; ++i)
  {
    if (m_sessions.reg[i].used()) count++;
  }

  return count;
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::send_one(const sam::txMessage& msg,
                                  const SID& id)
{
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  SessionReg& sreg = m_sessions.reg[ id.unique_id() ];

  if (sreg.used())
  {
    AdminSession * session = sreg.ptr;

    // Note: it is better to perform the session enqueue inside the lock so
    // that we avoid the race condition where a session is being removed
    // (and deleted) and we are trying to use the pointer to it! So as long
    // as we are holding the sessions lock, the session pointer will be
    // valid.

    // TODO: regarding comment above, make a note about this, because this is
    // a pattern that I have considered before, ie, to reduce the lock scope,
    // I have taken a copy of the protected data, and operated on the data
    // outside of the lock.  Sometimes it can be unsafe to do that.
    if ( session->is_open() ) session->enqueueToSend( msg );
  }
  else
  {
    _WARN_(m_logsvc, "session not found " << id
           << ", unable to send message: "
           << msg.type());

    // TODO: create a method for building a string of current sessions IDs
//    // build a string of all the sessions
//    std::ostringstream os;
//    for (iter=m_sessions.items.begin(); iter != m_sessions.items.end(); ++iter)
//    {
//      if (iter != m_sessions.items.begin()) os << ", ";
//      os << iter->first;
//    }
//    _INFO_(m_logsvc, "Current sessions: " << os.str());
  }
}
//----------------------------------------------------------------------
void AdminInterfaceImpl::send_one(const std::list<sam::txMessage>& msgs,
                                  const SID& id)
{
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  SessionReg& sreg = m_sessions.reg[ id.unique_id() ];

  if (sreg.used())
  {
    AdminSession * session = sreg.ptr;

    // Note: it is better to perform the session enqueue inside the lock so
    // that we avoid the race condition where a session is being removed
    // (and deleted) and we are trying to use the pointer to it! So as long
    // as we are holding the sessions lock, the session pointer will be
    // valid.

    // TODO: regarding comment above, make a note about this, because this is
    // a pattern that I have considered before, ie, to reduce the lock scope,
    // I have taken a copy of the protected data, and operated on the data
    // outside of the lock.  Sometimes it can be unsafe to do that.
    if ( session->is_open() )
    {
      for (std::list<sam::txMessage>::const_iterator it = msgs.begin();
           it != msgs.end(); ++it)
        session->enqueueToSend( *it);
    }
  }
  else
  {
    _WARN_(m_logsvc, "session not found " << id
           << ", unable to send message list");

    // TODO: see above TODO, above a method for building a session list
  //  // build a string of all the sessions
  //   std::ostringstream os;
  //   for (iter=m_sessions.items.begin(); iter != m_sessions.items.end(); ++iter)
  //   {
  //     if (iter != m_sessions.items.begin()) os << ", ";
  //     os << iter->first;
  //   }
  //   _INFO_(m_logsvc, "Current sessions: " << os.str());
  }
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::send_all(const sam::txMessage& msg)
{
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  for (size_t i = 0; i < SESSION_REG_SIZE; ++i)
  {
    if ( m_sessions.reg[i].used() and
         m_sessions.reg[i].ptr->is_open())
    {
      m_sessions.reg[i].ptr->enqueueToSend( msg );
    }
  }

  // for (std::map<SID, AdminSession*>::iterator it = m_sessions.items.begin();
  //      it != m_sessions.items.end(); ++it)
  // {
  //   if (it->second->is_open())
  //   {
  //     it->second->enqueueToSend( msg );
  //   }
  // }
}

//----------------------------------------------------------------------
bool AdminInterfaceImpl::session_exists(const SID& id) const
{
  // Grab the lock, so that other threads cannot update the collection of
  // sessions while we searching.
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  size_t i = id.unique_id();
  return m_sessions.reg[ i ].used();
}

//----------------------------------------------------------------------
bool AdminInterfaceImpl::session_open(const SID& id) const
{
  // Grab the lock, so that other threads cannot update the collection of
  // sessions while we searching.
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  size_t i = id.unique_id();
  return (m_sessions.reg[i].used() and
          m_sessions.reg[i].ptr->is_open());

//  return m_sessions.items.find( id ) != m_sessions.items.end();
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::session_list(std::list< SID > & l) const
{
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  for (size_t i = 0; i < SESSION_REG_SIZE; ++i)
  {
    if (m_sessions.reg[i].used())
      l.push_back( m_sessions.reg[i].ptr->id() );
  }

  // for (std::map< SID, AdminSession*>::iterator iter
  //        = m_sessions.items.begin();
  //      iter != m_sessions.items.end(); ++iter)
  // {
  //   AdminSession* s = iter->second;
  //   l.push_back( s->id() );
  // }
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::session_stop_one(const SID& id)
{
  // Grab the sessions lock.  This will prevent any other thread from running
  // the session cleanup routine. This is important because we don't want to
  // get a handle to a session which is simultaneously being deleted by
  // another thread.
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);


  size_t index = id.unique_id();

  if (m_sessions.reg[index].used())
  {
    _INFO_(m_logsvc, "closing session " << id);
    m_sessions.reg[index].ptr->close();
  }

  // std::map< SID, AdminSession* >::iterator iter =
  //   m_sessions.items.find( id );

  // if (iter != m_sessions.items.end())
  // {
  //   _INFO_(m_logsvc, "closing session " << id);
  //   iter->second->close();
  // }
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::session_stop_all()
{
  // Grab the sessions lock.  This will prevent any other thread from running
  // the session cleanup routine. This is important because we don't want to
  // get a handle to a session which is simultaneously being deleted by
  // another thread.
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  for (size_t i = 0; i < SESSION_REG_SIZE; ++i)
  {
    if (m_sessions.reg[i].used())
      m_sessions.reg[i].ptr->close();
  }
}

//----------------------------------------------------------------------

/**
 * Invoked by a session when it has just received a new SAM message from its
 * peer.
 */
void AdminInterfaceImpl::session_msg_received(const sam::txMessage& reqmsg,
                                              AdminSession& session)
{

  // {
  //   sam::MessageFormatter formatter;
  //   std::ostringstream os;
  //   os << "Received msg: ";
  //   formatter.format(reqmsg, os);
  //   _INFO_(m_logsvc,  os.str() );
  // }


  if (reqmsg.type() == id::msg_logon)
  {
    handle_logon_msg(reqmsg, session);
    return;
  }
  else if (reqmsg.type() == id::msg_request)
  {
    handle_admin_request(reqmsg, session);
    return;
  }
}

//----------------------------------------------------------------------

void AdminInterfaceImpl::helper_session_descr(std::ostream& os,
                                              AdminSession& session,
                                              const std::string& user)
{
  os << "session " << session.id()
     << " (service-id '" << session.peer_serviceid() << "', "
     << "username '" << user << "', "
     << "peer " << session.peeraddr() << ", "
     << "fd " << session.fd() << ")";
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::handle_admin_request(const sam::txMessage& reqmsg,
                                              AdminSession& session)
{
  static sam::qname QN_body_command = QNAME(id::body, id::command);

  std::string reqseqno;
  const sam::txField* f_reqseqno = reqmsg.root().find_field(id::QN_head_reqseqno);
  if (f_reqseqno) reqseqno = f_reqseqno->value();

  AdminResponse resp(reqseqno);

  // we need to associate a username with this request - TODO: later I need
  // to embeed this into the AdminRequest object etc and compare the
  // username of both the sessions and the command
  std::string user = session.username();

  try
  {
    // Locate the command name - this should be in head.command, however, to
    // compatible with the legacy ximon.gui, we need to also expect it in
    // body.command (if we first don't find it in head.command)

    const sam::txField* command = reqmsg.root().find_field( id::QN_command );

    if (!command or command->value().empty() )
    {
      command = reqmsg.root().find_field( QN_body_command );
    }

    if (!command or command->value().empty() )
    {
      std::ostringstream os;
      os << "Invalid admin requested by ";
      helper_session_descr(os, session, user);
      _WARN_(m_logsvc, os.str());

      throw AdminError(id::err_admin_not_found);
    }

    // try to find a matching admin
    AdminCommand * admin = this->admin_find( command->value() );
    if (admin == NULL)
    {
      std::ostringstream os;
      os << "Unknown admin '" << command->value() << "' requested by ";
      helper_session_descr(os, session, user);
      _WARN_(m_logsvc, os.str());

      throw AdminError(id::err_admin_not_found);
    }

    /*

    What do we need to do here?

    -> we need to match the raw request to an admin command.

    -> if cant match, we can reply in the negative.

    -> but, if we find an admin (via the registry), then we can go ahead an execute.

    -> three modes:

    immediate response.
    delayed reponse.
    later replies.

    */


    AdminRequest req( reqmsg,  session.id(),  m_ai);

    {
      std::ostringstream os;
      os << "Admin '" << command->value() << "' with args [";

      std::vector< std::string >::const_iterator argiter;
      for (argiter=req.args().begin(); argiter!=req.args().end(); ++argiter)
      {
        if (argiter!=req.args().begin()) os << ", ";
        os << "'" << *argiter<<"'";
      }

      os << "] requested by ";
      helper_session_descr(os, session, user);
      _INFO_(m_logsvc, os.str());
    }

    resp = admin->invoke(req);

  }
  catch (AdminError & e)
  {
    _WARN_(m_logsvc, "admin failed: " << e.what());
    resp = AdminResponse::error(reqseqno, e.code(), e.what());
  }
  catch (std::exception & e)
  {
    _WARN_(m_logsvc, "admin failed: " << e.what());
    resp = AdminResponse::error(reqseqno, id::err_admin_not_found,e.what());
  }
  catch (...)
  {
    _WARN_(m_logsvc, "admin failed: unknown exception caught");
    resp = AdminResponse::error(reqseqno, id::err_unknown,
                                "unknown admin error");
  }

  if (resp.send)
  {
    resp.msg.root().put_field(id::QN_msgtype, id::msg_response);
    if( session.enqueueToSend( resp.msg ) )
    {
      /* we failed to encode/send ... might have been an internal error, so,
         lets try to send a stock error message */
      exio::AdminResponse  err_resp = AdminResponse::error(reqseqno,
                                                           id::err_unknown,
                                                           "enqueue failed");
      session.enqueueToSend(err_resp.msg);
    }
  }

  // Optionally close the session if there is no pending replies and if the
  // sessions wants auto-close.
  if ( !exio::has_pending(resp.msg) and session.wants_autoclose() )
  {
    session.close();
  }
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::admin_add(AdminCommand ac)
{
  std::pair<std::string, AdminCommand> mypair(ac.name(), ac);

  cpp11::unique_lock<cpp11::mutex> guard(m_admins.lock);
  m_admins.items.insert( mypair );
}

//----------------------------------------------------------------------

AdminCommand* AdminInterfaceImpl::admin_find(const std::string& name)
{
  cpp11::unique_lock<cpp11::mutex> guard(m_admins.lock);



  std::map<std::string, AdminCommand>::iterator iter =
    m_admins.items.find( name );




  if (iter ==  m_admins.items.end() )
    return NULL;

  return &(iter->second);
}


//----------------------------------------------------------------------
// AdminResponse AdminInterfaceImpl::admincmd_uptime(AdminRequest& r)
// {
//   static const int MIN_SECS = 60;
//   static const int HOUR_SECS = 60 * 60;
//   static const int DAY_SECS = 24 * 60 * 60;


//   time_t now = time(NULL);
//   unsigned long elapsed = now - m_start_time;

//   size_t days = elapsed / DAY_SECS;
//   elapsed -= days * DAY_SECS;

//   size_t hours = elapsed / HOUR_SECS;
//   elapsed -= hours * HOUR_SECS;

//   size_t min = elapsed / MIN_SECS;
//   elapsed -= min * MIN_SECS;

//   size_t sec = elapsed;

//   std::ostringstream os;
//   os << days  << " days, "
//      << hours << " hours, "
//      << min   << " mins, "
//      << sec   << " secs";

//   return AdminResponse::success(r.reqseqno, os.str());
// }

//----------------------------------------------------------------------
void AdminInterfaceImpl::housekeeping()
{
  session_cleanup();

  // Now heartbeat on each session
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);


  for (size_t i = 0; i < SESSION_REG_SIZE; ++i)
  {
    if (m_sessions.reg[i].used() and
        m_sessions.reg[i].ptr->is_open())
    {
      m_sessions.reg[i].ptr->housekeeping();
    }
  }

  // for (std::map<SID, AdminSession*>::iterator it = m_sessions.items.begin();
  //      it != m_sessions.items.end(); ++it)
  // {
  //   if (it->second->is_open()) it->second->housekeeping();
  // }
}

//----------------------------------------------------------------------

/*
(gdb) where
exio::AdminInterfaceImpl::createNewSession () from /home/darrens/work/dev/src/c++/demo/install/lib/libexio.so.0
exio::AdminServerSocket::accept_TEP (this=0x63bbe0) at ../../exio/libexio/AdminServerSocket.cc:261
cpp11::execute_native_thread_routine (__p=0x63d870) at ../../exio/libcpp11/thread.cc:21
start_thread () from /lib/x86_64-linux-gnu/libpthread.so.0
*/
void AdminInterfaceImpl::createNewSession(int fd)
{
  // Serialise access to the create new session operation.  We do this so that
  // we avoid the situation there a free slot on the session registry array is
  // not simultaneously claimed more than once.
  cpp11::lock_guard<cpp11::mutex> guard(m_create_session_lock);

  // TODO perform these steps individually, and catch and throw exception.
  // TODO: basically, if the second step fails, adding to the list, we
  // must close the session straigth away -- but, don't throw is closing a
  // valid session, because we don't want the called to think the socket
  // needs to be closed.

  // Attempt to find a spare session row
  size_t session_index = 0;
  {
    cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);
    for (size_t i = 0; i < SESSION_REG_SIZE and session_index == 0; ++i)
    {
      // Is 'next' available?
      if (m_sessions.next != 0 and
          not m_sessions.reg[m_sessions.next].used())
      {
         session_index = m_sessions.next;
      }

      // move on
      if (++m_sessions.next > MAX_SESSIONS) m_sessions.next=0;
    }
  }

  if (session_index == 0) throw std::runtime_error("no more sessions allowed");

  AdminSession* session = NULL;

  {
    cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

    // Note that we create the session, and register it in the
    // session-registry witin a lock scope.  This is teo prevent the race
    // condition whereby a message is received from the remote process before
    // the session ID has been registered; that can result in session ID
    // lookups failing, meaning that replies to such race-condition messages
    // go unsent.
    session = new AdminSession(m_appsvc, fd, this, session_index);

    // TODO: here I need to check that the ID is unique. If not, I could try
    // creating a new unique ID, or, reject the connection.

    m_sessions.reg[ session_index ].ptr = session;

    // m_sessions.items[ session->id() ] = session;


    m_sessions.createdCount++;
  }

  // add client to the reactor quite early, so the IO events can begin
  session->init(m_reactor);

  _INFO_(m_appsvc.log(), "Connection from "
         << session->peeraddr()
         << ", fd " << fd
         << ", created session " << session->id());

  // std::ostringstream os;
  // os << "IO threads for session " << session->id() << ": ";
  // session->log_thread_ids(os);
  // _INFO_(m_appsvc.log(),  os.str() );

  // Send a logon message to the new connection. Note that this is an
  // unconditional event, ie we don't have to wait for thje client to send a
  // logon message first.  This is okay, because all this serves to do is send
  // some details about the server to the clienht.
  sam::txMessage logon(id::msg_logon);

  logon.root().put_field(id::QN_serviceid, m_appsvc.conf().serviceid);
  session->enqueueToSend( logon );

}

//----------------------------------------------------------------------

AdminResponse AdminInterfaceImpl::admincmd_subscribe(AdminRequest& req)
{
  AdminResponse resp(req.reqseqno);

  exio::add_rescode(resp.msg, 0);

  m_monitor.subscribe_all( req.id );
  return resp;
}

//----------------------------------------------------------------------
AdminResponse AdminInterfaceImpl::admincmd_help(
  AdminRequest& request)
{
  cpp11::unique_lock<cpp11::mutex> guard(m_admins.lock);

  if (request.args().empty())
  {
    // no args, so just list all admins
    std::list<std::string> admins;
    AdminResponse resp(request.reqseqno);

    for (std::map<std::string,AdminCommand>::iterator i =
           m_admins.items.begin();
         i != m_admins.items.end(); ++i)
    {
      admins.push_back( i->first );
    }

    admins.sort( compare_nocase );

    exio::add_rescode(resp.msg, 0);
    exio::set_pending(resp.msg, false);
    exio::formatreply_simplelist(resp.body(), admins, "admin");
    return resp;
  }
  else
  {
    std::map<std::string,AdminCommand>::iterator i = m_admins.items.find( request.args()[0] );
    if (i != m_admins.items.end())
    {
      std::ostringstream os;
      os << i->second.name();

      if (i->second.shorthelp().empty()==false)
        os << " - " << i->second.shorthelp();

      if (i->second.longhelp().empty()==false)
      {
        os << "\n\n" << i->second.longhelp();
      }

      return AdminResponse::success(request.reqseqno,os.str());
    }
    else
    {
      return AdminResponse::error(request.reqseqno,
                                  id::err_admin_not_found,
                                  "admin not found");
    }
  }
}

//----------------------------------------------------------------------
AdminResponse AdminInterfaceImpl::admincmd_snapshot(
  AdminRequest& request)
{
  m_monitor.broadcast_snapshot();
  return AdminResponse::success(request.reqseqno, "snapshot broadcast");
}
//----------------------------------------------------------------------
AdminResponse AdminInterfaceImpl::admincmd_info(AdminRequest& request)
{
  AdminResponse resp(request.reqseqno);

  /* Generate the uptime string */
  time_t now = time(NULL);
  unsigned long elapsed = now - m_start_time;

  size_t days = elapsed / DAY_SECS;
  elapsed -= days * DAY_SECS;

  size_t hours = elapsed / HOUR_SECS;
  elapsed -= hours * HOUR_SECS;

  size_t min = elapsed / MIN_SECS;
  elapsed -= min * MIN_SECS;

  size_t sec = elapsed;
  std::ostringstream uptime;
  uptime << days  << " days, "
         << hours << " hours, "
         << min   << " mins, "
         << sec   << " secs";

  std::ostringstream os;
  os << "serviceid: " << m_svcid << "\n";
  os << "arch: " << sizeof(long)*8 << "\n";
  os << "uptime: " << uptime.str() << "\n";
  os << "pid: " << getpid() << "\n";

  // username - use cuserid because that is user ID of the process
  char username[256];
  memset(username, 0, sizeof(username));
  cuserid(username);
  username[sizeof(username)-1] = '\0';
  os << "user: " << username << "\n";

  // hostname
  char hostname[256];
  memset(hostname, 0, sizeof(hostname));
  if (gethostname(hostname, sizeof(hostname)) != 0)
    strcpy(hostname, "unknown");
  hostname[sizeof(hostname)-1] = '\0';
  os << "host: " << hostname << "\n";

  // exio api version
  os << "exio-version: " << PACKAGE_VERSION;

  exio::add_rescode(resp.msg, 0);
  exio::set_pending(resp.msg, false);

  exio::formatreply_string(resp.body(), os.str());

  return resp;
}
//----------------------------------------------------------------------
AdminResponse AdminInterfaceImpl::admincmd_sessions(
  AdminRequest& request)
{
  AdminResponse resp(request.reqseqno);

  exio::add_rescode(resp.msg, 0);
  exio::set_pending(resp.msg, false);

  TableBuilder builder( resp.body() );

  std::vector<std::string> columns;
  columns.push_back("addr");
  columns.push_back("sessionid");
  columns.push_back("serviceid");

  builder.set_columns(columns);

  std::list< SID > sessions;
  this->session_list( sessions );

  for (std::list< SID >::iterator s = sessions.begin();
       s != sessions.end(); ++s)
  {
    std::vector<std::string> values;

    sid_desc sessioninfo; bool found;
    session_info(*s, sessioninfo, found);
    values.push_back( sessioninfo.peeraddr );  // "addr"

    std::string serviceid="";
    {
      cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

      if (m_sessions.reg[ s->unique_id() ].used())
      {
        serviceid = m_sessions.reg[s->unique_id()].ptr->peer_serviceid();

//        std::map<SID, AdminSession*>::iterator sesit = m_sessions.items.find(*s);
//        if (sesit != m_sessions.items.end())
//          serviceid = sesit->second->peer_serviceid();
      };
    }
    values.push_back( s->toString() ); // "sessionid"
    values.push_back( serviceid ); // "serviceid"

    // note: using the IP end-point as the rowkey
    builder.add_row(sessioninfo.peeraddr, values );
  }

  return resp;
}

//----------------------------------------------------------------------

AdminResponse AdminInterfaceImpl::admincmd_list_tables(AdminRequest& req)
{
  AdminResponse resp(req.reqseqno);
  exio::add_rescode(resp.msg, 0);
  exio::set_pending(resp.msg, false);

  if (req.args().empty()==false)
  {
    if (req.args()[0] == "list")
    {
      std::list< std::string > tables = m_monitor.tables();
      exio::formatreply_simplelist(resp.body(), tables, "table");
    }
    else if (req.args()[0] == "show")
    {
      if (req.args().size()>1)
      {
        if (m_monitor.has_table( req.args()[1]) == false)
        {
          return AdminResponse::error(req.reqseqno,
                                      id::err_no_table,
                                      "table not found");
        }

        AdminInterface::Table copy;

        std::vector<std::string>  cols;
        std::vector< std::vector <std::string> > rows;

         m_monitor.copy_table2(req.args()[1],cols,rows);


         TableBuilder builder( resp.body() );
         builder.set_columns(cols);

         // TODO: here we see the design problem of the Table and Row
         // classes. We have to assume the same field ID is used for a rowkey.
         // But also, need to review whether a row key is always needed.
         for (std::vector< std::vector <std::string> >::iterator r = rows.begin();
              r != rows.end(); ++r)
         {
           builder.add_row(id::row_key, *r);
        }
         exio::add_rescode(resp.msg, 0);
      }
      else
      {
        return AdminResponse::error(req.reqseqno,
                                    id::err_bad_command,
                                    "missing tablename");
      }
    }
    else
    {
      return AdminResponse::error(req.reqseqno,
                                  id::err_bad_command,
                                  "unknown table command");
    }
  }
  else
  {
    return AdminResponse::error(req.reqseqno,
                                id::err_bad_command,
                                "missing table command");
  }


  return resp;
}

//----------------------------------------------------------------------

void AdminInterfaceImpl::table_column_attr(const std::string& tablename,
                                           const std::string& column,
                                           const sam::txContainer& attribute)
{
  m_monitor.table_column_attr(tablename, column, attribute);
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::clear_table(const std::string& tablename)
{
  m_monitor.clear_table(tablename);
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::delete_row(const std::string& tablename, const std::string& rowkey)
{
  m_monitor.delete_row(tablename, rowkey);
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::monitor_update(
  const std::string& table_name,
  const std::string& rowkey,
  const std::map<std::string, std::string> & fields)
{
  m_monitor.update_table(table_name, rowkey, fields);
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::monitor_update_meta(const std::string& table_name,
                                             const std::string& rowkey,
                                             const std::string& column,
                                             const sam::txContainer& meta)
{
  m_monitor.update_meta(table_name, rowkey, column, meta);
}
//----------------------------------------------------------------------

void AdminInterfaceImpl::add_columns(const std::string& table_name,
                                     const std::list<std::string>& cols)
{
  m_monitor.add_columns(table_name, cols);
}


//----------------------------------------------------------------------

void AdminInterfaceImpl::handle_logon_msg(const sam::txMessage& logonmsg,
                                          AdminSession& session)
{
  // TODO: for now we will automatically subscribe to all tables, unless the
  // QN_noautosub field is present. This should be changed so that a table
  // subscription is only made by the subscribe message
  if ( logonmsg.root().check_field(id::QN_noautosub, id::True) == false)
  {
    m_monitor.subscribe_all( session.id() );
  }

  /* For compatibity with legacy GUI, we only set our sessions to
   * automatically close if the logon message indicates it wants that
   * functionality. */
  if ( logonmsg.root().check_field(QNAME(id::head, id::autoclose), id::True))
  {
    session.wants_autoclose(true);
  }

  // for now, we will also send a full description of our admins to the new
  // session.
  sam::txMessage admindescr;
  serialise_admins(admindescr);
  send_one(admindescr, session.id());
}
//----------------------------------------------------------------------

void AdminInterfaceImpl::start()
{
  m_serverSocket.start();
}

//----------------------------------------------------------------------

bool AdminInterfaceImpl::has_row(const std::string& tablename,
                             const std::string& rowkey) const
{
  return m_monitor.has_row(tablename, rowkey);
}

//----------------------------------------------------------------------

bool AdminInterfaceImpl::has_table(const std::string& tablename) const
{
  return m_monitor.has_table(tablename);
}
//----------------------------------------------------------------------
void AdminInterfaceImpl::copy_table(const std::string& tablename,
                                AdminInterface::Table& dest) const
{
  m_monitor.copy_table(tablename, dest);
}
//----------------------------------------------------------------------
void AdminInterfaceImpl::copy_table2(const std::string& tablename,
                                     std::vector<std::string> & cols,
                                     std::vector< std::vector <std::string> >& rows) const
{
  m_monitor.copy_table2(tablename, cols, rows);
}
//----------------------------------------------------------------------
void AdminInterfaceImpl::copy_row(const std::string& tablename,
                              const std::string& rowkey,
                              AdminInterface::Row& dest) const
{
  m_monitor.copy_row(tablename, rowkey, dest);
}
//----------------------------------------------------------------------
void AdminInterfaceImpl::monitor_alert(const std::string& source,
                                       const std::string& source_type,
                                       const std::string& error_str,
                                       const std::string& alert_type,
                                       const std::string& alert_id,
                                       const std::string& severity)
{
  sam::txMessage msg(id::msg_alert);
  msg.root().put_field(id::QN_msgtype, id::msg_alert);

  sam::txContainer& body = msg.root().put_child(id::body);
  body.put_field(id::source, source);
  body.put_field(id::source_type, source_type);
  body.put_field(id::error, error_str);
  body.put_field(id::alert_type, alert_type);
  body.put_field(id::alert_id, alert_id);
  body.put_field(id::severity, severity);

  send_all(msg);
}
//----------------------------------------------------------------------
void AdminInterfaceImpl::serialise_admins(sam::txMessage& msg) const
{
  // TODO: here is another case where we are construction a SAM message,
  // without care for its since.  I.e., can we be sure that the message we are
  // constructing, in the txMessage / txContainer etc, without care for the
  // eventual size of the message, and whether it can fit within a SAM
  // container.


   /*
     Some notes on admin commands (eventually this should be moved to the
     AdminCommand header file. These items are admin-attributes, and

     "hidden"  - if set to "true", the admin will not appear in the default
                 admin-panel (the admin-panel that open from the Connections
                 pane)

     "short"   - short help

     "long"    - long help help

     "defargs" - default arguments to place onto an interactive command
                 line.

   */

  cpp11::unique_lock<cpp11::mutex> guard(m_admins.lock);

  msg.type(id::admindescr);
  msg.root().put_field(id::QN_msgtype, id::admindescr);

  sam::txContainer& body     = msg.root().put_child(id::body);
  sam::txContainer& adminset = body.put_child(id::adminset);

  // loop over each of the admins
  int nadmin = 0;
  typedef std::map<std::string, AdminCommand>::const_iterator Iter;
  for (Iter i = m_admins.items.begin(); i != m_admins.items.end(); ++i)
  {
     const AdminCommand& ac = i->second;

     std::ostringstream os;
     os << id::admin_prefix << nadmin++;
     sam::txContainer& adminbox = adminset.put_child(os.str());

     adminbox.put_field(id::name,      ac.name());
     adminbox.put_field(id::shorthelp, ac.shorthelp());
     adminbox.put_field(id::longhelp,  ac.longhelp());

     // Next, serialise the attributes
     const std::map<std::string,std::string>& attrs = ac.attrs_();
     for (std::map<std::string,std::string>::const_iterator ait=attrs.begin();
          ait != attrs.end(); ++ait)
     {
       adminbox.put_field(ait->first, ait->second);
     }
  }

  std::ostringstream os;
  os << nadmin;
  adminset.put_field(id::admincount, os.str());
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::session_info(SID sid,
                                      sid_desc& sd,
                                      bool& found) const
{
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  const SessionReg& sreg = m_sessions.reg[ sid.unique_id() ];

  if ( sreg.used() )
  {

    found = true;
    sd.username = sreg.ptr->username();
    sd.peeraddr = sreg.ptr->peeraddr();
  }
  else
  {
    // session not found
    found = false;
  }
}
//----------------------------------------------------------------------
AdminResponse AdminInterfaceImpl::admincmd_diags(AdminRequest& request)
{
  AdminResponse resp(request.reqseqno);

  std::ostringstream os;

  std::set<std::string> sections;
  for (AdminRequest::Args::const_iterator i = request.args().begin();
       i != request.args().end(); ++i)
  {
    sections.insert(*i);
  }

// or (sections.count("sections")=1))
  if (sections.empty())
  {
    os << "Sessions\n";
    os << "--------\n";
  }
  {
    cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

    if (sections.empty())
    {
      os << "Total ever created: " << m_sessions.createdCount << "\n";

      size_t count = 0;
      for (size_t i = 0; i < SESSION_REG_SIZE; ++i)
        if (m_sessions.reg[i].used()) count++;

      os << "Active: " <<  count << "\n";
    }

    if (sections.empty() or (sections.count("sessions")==1))
    {
      os << "SessionID, fd, PeerAddr, PeerServiceID, User, Logon, Start, LastOut, BytesOut, BytesIn, QueueOut\n";

      for (size_t i = 0; i < SESSION_REG_SIZE; ++i)
      {
        if (not m_sessions.reg[i].used()) continue;
        AdminSession* sptr = m_sessions.reg[i].ptr;

        os << sptr->id() << ", ";
        os << sptr->fd() << ", ";
        os << sptr->peeraddr() << ", ";
        os << sptr->peer_serviceid() << ", ";
        os << sptr->username() << ", ";
        os << sptr->logon_recevied() << ", ";
        os << utils::datetimestamp(sptr->start_time()) << ", ";
        os << utils::datetimestamp(sptr->last_write()) << ", ";
        os << sptr->bytes_out() << ", ";
        os << sptr->bytes_in() << ", ";
        os << sptr->bytes_pend() << ", ";
        os << "\n";
      }
    }
  }

  if (sections.empty())
  {
    os << "\nexio threads\n------------\n";
  }

  if (sections.empty() or (sections.count("threads")==1))
  {
    os << "thread, lwp, pthread\n";

    std::pair<pthread_t, int> serverthr = m_serverSocket.thread_ids();
    os << "server_socket_accept, "
       << serverthr.second << std::dec << ", 0x"
       << std::hex << serverthr.first << std::dec << "\n";

    const std::vector< std::pair<pthread_t,int> > & rthreads
      = m_reactor->thread_ids();
    os << "reactor_io, "
       << rthreads[0].second << ", 0x"
       << std::hex << rthreads[0].first << std::dec << "\n";
    for (size_t n = 1; n < rthreads.size(); n++)
    {
      if (n != 1) os << "\n";
      os << "reactor_inbound_"<<n<<", "
         << rthreads[n].second << ", 0x"
         << std::hex << rthreads[n].first << std::dec;
    }
  }

  std::list<SID> sids;


  if (sections.empty())
  {
    os << "\n\ntables\n------\n";
  }

  if (sections.empty() or (sections.count("tables")==1))
  {
    std::list< std::string > tables = m_monitor.tables();
    for (std::list<std::string>::iterator t = tables.begin();
         t != tables.end(); ++t)
    {
      sids.clear();
      m_monitor.table_subscribers(*t, sids);
      os << *t;
      os << ": subs=[";
      for (std::list<SID>::iterator i = sids.begin();
           i != sids.end(); ++i)
      {
        if (i != sids.begin()) os << ", ";
        os << *i;
      }
      os << "], size=" << m_monitor.table_size(*t);
      os << "\n";
    }
  }


  exio::add_rescode(resp.msg, 0);
  exio::set_pending(resp.msg, false);

  exio::formatreply_string(resp.body(), os.str());

  return resp;
}

//----------------------------------------------------------------------
AdminResponse AdminInterfaceImpl::admincmd_table_subs(AdminRequest& request)
{
  AdminResponse resp(request.reqseqno);

  std::ostringstream os;

  std::list< std::string > tables = m_monitor.tables();
  std::list< SID > subs;

  for (std::list<std::string>::iterator i=tables.begin(); i!=tables.end(); ++i)
  {
    if (i != tables.begin()) os << "\n";
    os << *i << ": ";

    subs.clear();
    m_monitor.table_subscribers(*i, subs);

    for (std::list< SID >::iterator s = subs.begin(); s != subs.end(); ++s)
    {
      if (s != subs.begin()) os << ", ";
      os << *s;
    }
  }

  exio::add_rescode(resp.msg, 0);
  exio::set_pending(resp.msg, false);
  exio::formatreply_string(resp.body(), os.str());

  return resp;
}

//----------------------------------------------------------------------
AdminResponse AdminInterfaceImpl::admincmd_del_session(AdminRequest& req)
{
  AdminResponse resp(req.reqseqno);

  if (req.args().size() == 0) throw AdminError(id::err_bad_command);


  std::list<SID> sessions_stopped;

  AdminRequest::Args::const_iterator iter;
  for (iter = req.args().begin(); iter != req.args().end(); ++iter)
  {
    SID s  = SID::fromString( *iter );
    if (s != SID::no_session and session_exists(s))
    {
      sessions_stopped.push_back(s);
      session_stop_one( s );
    }
  }

  std::ostringstream os;
  os << "sessions stopping: ";
  for (std::list<SID>::iterator i = sessions_stopped.begin();
       i != sessions_stopped.end(); ++i)
  {
    if (i != sessions_stopped.begin()) os << ", ";
    os << *i;
  }

  exio::add_rescode(resp.msg, 0);
  exio::set_pending(resp.msg, false);
  exio::formatreply_string(resp.body(), os.str());

  return resp;
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::monitor_snapshot(const std::string& tablename)
{
  m_monitor.broadcast_snapshot(tablename);
}
//----------------------------------------------------------------------
void AdminInterfaceImpl::monitor_snapshot()
{
  m_monitor.broadcast_snapshot();
}
//----------------------------------------------------------------------
bool AdminInterfaceImpl::copy_field(const std::string& tablename,
                                    const std::string& rowkey,
                                    const std::string& field,
                                    std::string& dest)
{
  return m_monitor.copy_field(tablename,
                              rowkey,
                              field,
                              dest);


}
//----------------------------------------------------------------------
void AdminInterfaceImpl::copy_rowkeys(const std::string& tablename,
                                      std::list< std::string >& dest) const
{
  m_monitor.copy_rowkeys(tablename, dest);
}
//----------------------------------------------------------------------


AdminSession* AdminInterfaceImpl::get_session(SID id)
{
  AdminSession * ptr = NULL;

  {
    cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

    SessionReg& sreg = m_sessions.reg[ id.unique_id() ];
    if (sreg.used())
    {
      ptr =  sreg.ptr;
    }
  }

  return ptr;
}

//----------------------------------------------------------------------


} // namespace exio
