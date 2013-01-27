#include "exio/AdminInterfaceImpl.h"
#include "exio/AdminInterface.h"
#include "exio/AdminCommand.h"
#include "exio/Logger.h"
#include "exio/MsgIDs.h"
#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>




#define MIN_SECS   (60)
#define HOUR_SECS  (60 * 60)
#define DAY_SECS   (24 * 60 * 60)

namespace exio {



//----------------------------------------------------------------------

AdminInterfaceImpl::AdminInterfaceImpl(AdminInterface * ai)
  : m_appsvc(ai->appsvc()),
    m_logsvc( ai->appsvc().log() ),
    m_svcid(ai->appsvc().conf().serviceid),
    m_serverSocket(this),
    m_monitor(this),
    m_start_time( ::time(NULL) )
{
  /*
   * NOTE: design principle here: we should not add admins which modify table
   * data (eg, clear_table).  Management of table data is the responsibility
   * of the application code.
   */


  std::map<std::string,std::string> adminattrs;
  adminattrs[ "hidden" ] = "false";

  /* Register some admin capabilities of an admin interface */
  admin_add( AdminCommand("tables",
                          "list tables", "",
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
}

//----------------------------------------------------------------------

void AdminInterfaceImpl::session_closed(AdminSession& session)
{
  // Remove the session from the active list
  {
    cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

    std::map< SID, AdminSession* >::iterator i =
      m_sessions.items.find( session.id() );

    if ( i != m_sessions.items.end() )
    {
      m_sessions.items.erase( i );
    }
    else
    {
      /* oh dear, we failed to find out session based on lookup. now lets try
       * searching for its address */
      for (i=m_sessions.items.begin(); i != m_sessions.items.end(); ++i)
      {
        if (i->second == &session)
        {
          m_sessions.items.erase( i );
        }
      }
    }
  }

  // Now add the session to the expired list
  {
    cpp11::lock_guard<cpp11::mutex> guard(m_expired_sessions.lock);
    m_expired_sessions.items[ session.id() ] = &session;
  }

  // TODO:Now remove this session from any table subscriptions it might have held
  m_monitor.unsubscribe_all( session.id() );
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::session_cleanup()
{
  std::map< SID, AdminSession* > delete_later;

  cpp11::lock_guard<cpp11::mutex> guard(m_expired_sessions.lock);

  for (std::map< SID, AdminSession* >::iterator i =
         m_expired_sessions.items.begin();
       i != m_expired_sessions.items.end(); ++i)
  {
    try
    {
      if ( i->second->safe_to_delete() )
      {
//        _INFO_(m_logsvc, "Deleting session " << i->second->id() );
        delete i->second;

        // LESSON: I used to reset the iterator to begin(), but sometimes
        // failed...
      }
      else
        delete_later[ i->first ] = i->second;
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
  return m_sessions.items.size();
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::send_one(const sam::txMessage& msg,
                                  const SID& id)
{
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  std::map<SID, AdminSession*>::iterator iter;

  // NOTE: use this logging section when debugging admin sessions that
  // disconnected but we are still trying sending messages to

  // for (iter = m_sessions.items.begin();
  //      iter != m_sessions.items.end(); ++iter)
  // {
  //   _INFO_( "Active session: " << iter->first );
  // }

  iter = m_sessions.items.find(id);

  if (iter != m_sessions.items.end())
  {
    AdminSession * session = iter->second;

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
    _INFO_(m_logsvc, "no session found " << id );
  }
}
//----------------------------------------------------------------------
void AdminInterfaceImpl::send_all(const sam::txMessage& msg)
{
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  for (std::map<SID, AdminSession*>::iterator it = m_sessions.items.begin();
       it != m_sessions.items.end(); ++it)
  {
    if (it->second->is_open())
    {
      it->second->enqueueToSend( msg );
    }
  }
}

//----------------------------------------------------------------------
bool AdminInterfaceImpl::session_exists(const SID& id) const
{
  // Grab the lock, so that other threads cannot update the collection of
  // sessions while we searching.
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  return m_sessions.items.find( id ) != m_sessions.items.end();
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::session_list(std::list< SID > & l) const
{
  // Grab the lock, so that other threads cannot update the collection of
  // sessions while we are iterating through it.
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  for (std::map< SID, AdminSession*>::iterator iter
         = m_sessions.items.begin();
       iter != m_sessions.items.end(); ++iter)
  {
    AdminSession* s = iter->second;
    l.push_back( s->id() );
  }
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::session_stop_one(const SID& id)
{
  // Grab the sessions lock.  This will prevent any other thread from running
  // the session cleanup routine. This is important because we don't want to
  // get a handle to a session which is simultaneously being deleted by
  // another thread.
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  std::map< SID, AdminSession* >::iterator iter =
    m_sessions.items.find( id );

  // TODO: call session close method here
//  if (iter != m_sessions.items.end()) iter->second->close_safe();
}

//----------------------------------------------------------------------
void AdminInterfaceImpl::session_stop_all()
{
  // Grab the sessions lock.  This will prevent any other thread from running
  // the session cleanup routine. This is important because we don't want to
  // get a handle to a session which is simultaneously being deleted by
  // another thread.
  cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

  for (std::map<SID, AdminSession*>::iterator iter = m_sessions.items.begin();
       iter != m_sessions.items.end(); ++iter)
  {
//    AdminSession* s = iter->second;

    // TODO: call a session close method here
//    s->close_safe();
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
void AdminInterfaceImpl::handle_admin_request(const sam::txMessage& reqmsg,
                                              AdminSession& session)
{
  static sam::qname QN_body_command = QNAME(id::body, id::command);

  std::string reqseqno;
  const sam::txField* f_reqseqno = reqmsg.root().find_field(id::QN_head_reqseqno);
  if (f_reqseqno) reqseqno = f_reqseqno->value();

  AdminResponse resp(reqseqno);

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
      // TODO: need to send back a reject message here

      _ERROR_(m_logsvc, "Cannot identify command name of the request");
      return;  /* ignore empty message for now : TODO: throw exception again? */
      // throw std::runtime_error("request did not specify an admin
    }

    // try to find a matching admin
    AdminCommand * admin = this->admin_find( command->value() );
    if (admin == NULL)
      throw AdminError(id::err_admin_not_found, "command not recognised");

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

    // TODO: need to build out the request object
    AdminRequest req( reqmsg,  session.id() );
    resp = admin->invoke(req);

  }
  catch (AdminError & e)
  {
    // TODO: need to generalise the error codes
    std::cout << "admin failed: " << e.what() << "\n"; // TODO: remove?
    resp = AdminResponse::error(reqseqno, e.code(), e.what());
  }
  catch (std::runtime_error & e)
  {
    // TODO: need to generalise the error codes
    std::cout << "admin failed: " << e.what() << "\n"; // TODO: remove?
    resp = AdminResponse::error(reqseqno, id::err_admin_not_found,e.what());
  }
  catch (...)
  {
    // TODO: need to use
    resp = AdminResponse::error(reqseqno, id::err_unknown,
                                "unknown admin error");
  }

  if (resp.send)
  {
    resp.msg.root().put_field(id::QN_msgtype, id::msg_response);
    session.enqueueToSend( resp.msg );
  }

  // Optionally close the session if there is no pending replies and if the
  // sessions wants auto-close.
  if ( !exio::has_pending(resp.msg) and session.wants_autoclose() )
  {
    _INFO_(m_logsvc, "Session will be closed: " << session.id().addr());
    session.close_io();
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
}

//----------------------------------------------------------------------

/*
(gdb) where
#0  0x00007ffff7ba1ad0 in exio::AdminInterfaceImpl::createNewSession () from /home/darrens/work/dev/src/c++/demo/install/lib/libexio.so.0
#1  0x00007ffff7ba8f8c in exio::AdminServerSocket::accept_TEP (this=0x63bbe0) at ../../exio/libexio/AdminServerSocket.cc:261
#2  0x00007ffff7bcc89a in cpp11::execute_native_thread_routine (__p=0x63d870) at ../../exio/libcpp11/thread.cc:21
#3  0x00007ffff745fe9a in start_thread () from /lib/x86_64-linux-gnu/libpthread.so.0
#4  0x00007ffff718ccbd in clone () from /lib/x86_64-linux-gnu/libc.so.6
#5  0x0000000000000000 in ?? ()
*/
void AdminInterfaceImpl::createNewSession(int fd)
{
  // TODO perform these steps individually, and catch and throw exception.
  // TODO: basically, if the second step fails, adding to the list, we
  // must close the session straigth away -- but, don't throw is closing a
  // valid session, because we don't want the called to think the socket
  // needs to be closed.
  AdminSession* session = new AdminSession(m_appsvc, fd, this);

  {
    cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);

    // TODO: here I need to check that the ID is unique. If not, I could try
    // creating a new unique ID, or, reject the connection.

    m_sessions.items[ session->id() ] = session;
  }


  // Send a logon message to the new connection.

  // TODO: once the ximon is able to send logon kmessages first, then we won't
  // do this until a logon has been received from the remote.
  sam::txMessage logon(id::msg_logon);

  logon.root().put_field(id::QN_serviceid, m_appsvc.conf().serviceid);
  session->enqueueToSend( logon );

  // TODO: what is the best way to modularise code for building logon message?
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
  std::list<std::string> admins;

  AdminResponse resp(request.reqseqno);


  {
     cpp11::unique_lock<cpp11::mutex> guard(m_admins.lock);

        for (std::map<std::string,AdminCommand>::iterator i =
               m_admins.items.begin();
             i != m_admins.items.end(); ++i)
        {
          admins.push_back( i->first );
        }
  }

  exio::add_rescode(resp.msg, 0);
  exio::set_pending(resp.msg, false);
  exio::formatreply_simplelist(resp.body(), admins, "admin");

  return resp;
}

//----------------------------------------------------------------------
AdminResponse AdminInterfaceImpl::admincmd_snapshot(
  AdminRequest& request)
{
  m_monitor.broadcast_snapshot();
  return AdminResponse::success(request.reqseqno);
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



  // username
  char username[256];
  memset(username, 0, sizeof(username));
  if (getlogin_r(username, sizeof(username)) != 0)
    strcpy(username, "unknown");
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
    values.push_back( s->addr() );  // "addr"

    std::string serviceid="";
    {
      cpp11::lock_guard<cpp11::mutex> guard(m_sessions.lock);
      std::map<SID, AdminSession*>::iterator sesit = m_sessions.items.find(*s);
      if (sesit != m_sessions.items.end())
        serviceid = sesit->second->peer_serviceid();
    };
    values.push_back( s->toString() ); // "sessionid"
    values.push_back( serviceid ); // "serviceid"

    // note: using the IP end-point as the rowkey
    builder.add_row(s->addr(), values );
  }

  return resp;
}

//----------------------------------------------------------------------

AdminResponse AdminInterfaceImpl::admincmd_list_tables(
  AdminRequest& r)
{
  std::list< std::string > tables = m_monitor.tables();

  AdminResponse resp(r.reqseqno);

  exio::add_rescode(resp.msg, 0);
  exio::set_pending(resp.msg, false);

  exio::formatreply_simplelist(resp.body(), tables, "table");

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
void AdminInterfaceImpl::clear_table(const std::string& table_name)
{
  m_monitor.clear_table(table_name);
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

    //_INFO_(m_logsvc, "Setting session to autoclose");
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
                                       const std::string& alert_type)
{
  sam::txMessage msg(id::msg_alert);
  msg.root().put_field(id::QN_msgtype, id::msg_alert);

  sam::txContainer& body = msg.root().put_child(id::body);
  body.put_field(id::source, source);
  body.put_field(id::source_type, source_type);
  body.put_field(id::error, error_str);
  body.put_field(id::alert_type, alert_type);

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


} // namespace exio