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

#include "exio/Reactor.h"
#include "exio/Logger.h"
#include "exio/AdminInterface.h"
#include "exio/AdminInterfaceImpl.h"
#include "exio/utils.h"
#include "exio/Logger.h"

extern "C"
{
  #include <xlog/xlog.h>
}

#include <sstream>
#include <algorithm>
#include <map>

#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <errno.h>
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>
#include <sys/socket.h>

#include <sys/types.h>
#include <linux/unistd.h>
#include <sys/syscall.h>

namespace exio {


void eventstr(std::ostream& os, int e)
{
  const char* delim="";
  if (e bitand POLLIN)    { os << delim << "POLLIN"; delim="|"; }   // seen
  if (e bitand POLLPRI)   { os << delim << "POLLPRI"; delim="|"; }
  if (e bitand POLLOUT)   { os << delim << "POLLOUT"; delim="|"; }  // seen
  if (e bitand POLLRDHUP) { os << delim << "POLLRDHUP"; delim="|"; }
  if (e bitand POLLERR)   { os << delim << "POLLERR"; delim="|"; }
  if (e bitand POLLHUP)   { os << delim << "POLLHUP"; delim="|"; }
  if (e bitand POLLNVAL)  { os << delim << "POLLNVAL"; delim="|"; }  // seen
};

//----------------------------------------------------------------------
struct ReactorMsg
{
    /*
       eNoEvent: used when we want the reactor thread to come out of the
       internal notification Q.  Used when clients are to be added or deleted.

     */
    enum MsgType
    {
      eNoEvent    = 0,
      eClose,
      eAdd,
      eAttn,
      eTerminate   /* terminate reactor thread */
    };

    static const char* str(MsgType m)
    {
      switch(m)
      {
        case eNoEvent    : return "eNoEvent";
        case eClose      : return "eClose";
        case eAdd        : return "eAdd";
        case eAttn       : return "eAttn";
        case eTerminate  : return "eTerminate";
        default : return "unknown";
      }
    }

    ReactorMsg()          : type(eNoEvent), ptr(NULL) {}
    ReactorMsg(MsgType t, ReactorClient* __ptr = NULL) : type(t), ptr(__ptr) {}

    MsgType        type;
    ReactorClient* ptr;
};

//----------------------------------------------------------------------
class ReactorNotifQ
{
  public:
    ReactorNotifQ(int pipein, int pipeout)
      : m_pipein(pipein),
        m_pipeout(pipeout)
    {
    }

    // TODO: is there a race condition in here, ie. the condition pushing of a
    // eNoEvent?
    void push_msg(const ReactorMsg& m)
    {
      xlog_write("push_msg", __FILE__, __LINE__);
      xlog_write(ReactorMsg::str(m.type), __FILE__, __LINE__);
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

      // if there are queued items, then we don't need to push another
      // eNoEvent item.
      if ( (m.type == ReactorMsg::eNoEvent)
           and
           ((m_pending.empty() == false) or m_pending.size())
        )
      {
        return;
      }

      char c = 'x';
      m_pending.push_back(m);
      write(m_pipeout, &c, 1);
    }

    void pull(std::deque<ReactorMsg>& dest)
    {
      dest.clear();
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
      m_pending.swap( dest );

      // drain pipe
      char buf[1024];
      while (read(m_pipein, buf, sizeof(buf))>0) { }
    }

  private:
    std::deque<ReactorMsg> m_pending;
    cpp11::mutex           m_mutex;
    int                    m_pipein;
    int                    m_pipeout;
};

//----------------------------------------------------------------------

/* Constructor */
Reactor::Reactor(LogService* log)
  : m_log(log),
    m_is_stopping(false),
    m_notifq(NULL),
    m_io(NULL)
{
  xlog_config cfg;
  memset(&cfg, 0, sizeof(xlog_config));
  cfg.num_counters = 10;
  cfg.num_rows = 10000;
  xlog_init("/var/tmp/xlog.dat", cfg);

  m_pipefd[0]=-1;  // reader
  m_pipefd[1]=-1;  // writer

  // create our pipes
  if (pipe2(m_pipefd, O_NONBLOCK) != 0)
  {
    int err = errno;
    _WARN_(m_log,
           "pipe failed: "
           << utils::strerror(err) );
    m_pipefd[0]=-1;
    m_pipefd[1]=-1;

    throw std::runtime_error("cannot create pipe");
  }
  m_notifq = new ReactorNotifQ(m_pipefd[0], m_pipefd[1]);

  // Danger: creation of internal thread should be last action of constructor,
  // so that object construction is complete before accessed by thread.
  m_io = new cpp11::thread(&Reactor::reactor_io_TEP, this);
}

//----------------------------------------------------------------------
/* Destructor */
Reactor::~Reactor()
{
  // // wait until all clients removed - note, we are not doing anything clever
  // // like deleting the clients on demand, or setting a flag that the reactor
  // // is no invalid. It is up to the user application to ensure the reactor is
  // // deleted in a coordinated fashion.
  // {
  //   cpp11::unique_lock<cpp11::mutex> guard(m_clients.lock);
  //   while(m_clients.ptrs.empty()==false or m_clients.ptrs.size()>0)
  //   {
  //     m_clients.ptrs_empty.wait (guard);
  //   }
  // }

  /* Stop reactor thread */
  m_is_stopping = true;
  xlog_write("pushing termiante thread", __FILE__, __LINE__);
  m_notifq->push_msg(ReactorMsg(ReactorMsg::eTerminate));

  // Ensure our thread has been joined. Note: it is criticaly important that
  // internal threads are joined with before we continue to delete other data
  // members, because we want to avoid the common threading bug where internal
  // threads are left running after member data has been deleted; as soon as
  // one of those internal threads next comes alive and attempts to use the
  // deleted member data, a hard to identify error will occur!.
  m_io -> join();
  delete m_io;

  delete m_notifq;

  close(m_pipefd[0]);
  close(m_pipefd[1]);
}

//----------------------------------------------------------------------
void Reactor::reactor_io_TEP()
{
  while ( m_is_stopping == false)
  {
    try
    {
      reactor_main_loop();

      /* WARN: nothing should be done after main loop exit, because the object
       * is probably in the process of destructing. */
    }
    catch (const std::exception & e)
    {
      _ERROR_(m_log, "reactor: exception from event loop: " << e.what());
    }
    catch (...)
    {
      _ERROR_(m_log, "reactor: exception from event loop: unknown");
    }

    /* NOTE: we don't allow exceptions to cause the reactor thread to
     * terminate */
  }

  // final chance for clean up -- nothing sophisticated, just attend to the
  // clients one last time, then, force socket close.
  attend_clients();

//  cpp11::unique_lock<cpp11::mutex> guard(m_clients.lock);
  if (m_clients.ptrs.empty()==false or m_clients.ptrs.size()>0)
  {
    _WARN_(m_log, "reactor terminating, but clients still exist");
    for (std::vector<ReactorClient*>::iterator iter = m_clients.ptrs.begin();
         iter != m_clients.ptrs.end(); ++iter)
    {
      ReactorClient* client = *iter;
      if (client->io_open()) client->handle_close();

      // NOTE: we don't delete the client, because it might still be used by
      // the application.
    }
  }
}

//----------------------------------------------------------------------
void Reactor::reactor_main_loop()
{
  const int stdevents = POLLRDHUP|POLLERR|POLLHUP|POLLNVAL;

  std::vector< pollfd > fdset;
  std::map<int, ReactorClient*> fdmap;

  while (m_is_stopping == false)
  {
    fdset.clear();
    fdmap.clear();

    // add our internal pipe
    pollfd pfd;
    pfd.fd = m_pipefd[0];
    pfd.events = POLLIN bitor POLLHUP;
    fdset.push_back( pfd );

    // TODO: I should only need to build the fdset and fdmap when a client is
    // added or removed.  The only think I need to do each time is just call
    // events() to get the state of each client.
    {
//      cpp11::lock_guard<cpp11::mutex> guard(m_clients.lock);

      // build the array of pollfd, and companion array of client-id's
      for (std::vector<ReactorClient*>::iterator iter = m_clients.ptrs.begin();
           iter != m_clients.ptrs.end(); ++iter)
      {
        ReactorClient* client = *iter;
        if (client->io_open())
        {
          /* if we have called close() on the fd, we should not use it again
           * in the poll */
          pollfd pfd;
          memset(&pfd, 0, sizeof(pfd));
          pfd.fd = client->fd();
          pfd.events = client->events() |  stdevents;
          fdset.push_back( pfd );
          fdmap[ pfd.fd ] = client;
        }
      }
    }

    // TODO: move this logging to a separate function
    // {
    //   std::ostringstream osin;
    //   for (std::vector<pollfd>::const_iterator i = fdset.begin();
    //        i != fdset.end(); ++i)
    //   {
    //     if (i->events)
    //     {
    //       osin << "[";
    //       osin << "fd=" << i->fd << ", events=" << i->events << "(";
    //       eventstr(osin, i->events);
    //       osin << "),";
    //       osin << "]";
    //     }
    //   }
    //   _DEBUG_(m_log, "reactor: into poll, events: " << osin.str() );
    // }

    int timeout_msec = -1;

    xlog_write("poll-in", __FILE__, __LINE__);
    int nready = poll(&fdset[0], fdset.size(), timeout_msec);
    xlog_write("poll-out", __FILE__, __LINE__);


    // // Reactor is stopping.  This is our last chance to close any open sockets
    // // and delete client objects.  We generally don't want to delay exit of
    // // this reactor thread, because it will in turn delay the completion of
    // // the reactor destructor
    // if (m_is_stopping) return;
    // {
    //   cpp11::lock_guard<cpp11::mutex> guard(m_clients.lock);
    //   for (std::vector<ReactorClient*>::iterator it = m_clients.ptrs.begin();
    //        it != m_clients.ptrs.end(); ++it)
    //   {
    //     ReactorClient* client = *it;
    //     client->handle_close();
    //     if (client->m_release_count)
    //     {
    //       delete client;
    //     }
    //     else
    //     {
    //       // danger: we cannot delete the client, because a session still
    //       // appears to be using it. Best case problem, is we have a memory
    //       // leak.
    //       _ERROR_(m_log, "during reactor termination, unable to delete Client which is still in used");

    //     }
    //   }
    //   return;
    // }

    // TODO: move this logging to a separate function
    // {
    //   std::ostringstream osout;
    //   osout << "nready=" << nready << ", ";
    //   for (std::vector<pollfd>::const_iterator i = fdset.begin();
    //        i != fdset.end(); ++i)
    //   {
    //     if (i->revents)
    //     {
    //       osout << "[";
    //       osout << "fd=" << i->fd << ", revents=" << i->revents << "(";
    //       eventstr(osout, i->revents);
    //       osout << "),";
    //       osout << "]";
    //     }
    //   }
    //   _DEBUG_(m_log, "reactor: from poll, revents: " << osout.str() );
    // }


    if (nready < 0)
    {
      // TODO: handle error
    }
    else if (nready == 0)
    {
      // No FD ready.
    }
    else
    {
      for (std::vector<pollfd>::iterator iter = fdset.begin();
           iter != fdset.end(); ++iter)
      {
        if (iter->fd  == m_pipefd[0]) continue; // skip ctrl-msg events ... do later

        ReactorClient*   ptr       = fdmap[ iter->fd ];
        int              revents   = iter->revents;
        int              readagain = 0;

        if (revents bitand POLLIN)
        {
          if (ptr->io_open()) readagain = ptr->handle_input();
        }
        if (revents bitand POLLPRI)
        {
          // don't handle POLLPRI
        }
        if (revents bitand POLLOUT)
        {
          if (ptr->io_open()) ptr->handle_output();
        }
        if ( ((revents bitand POLLRDHUP) or
              (revents bitand POLLERR) or
              (revents bitand POLLHUP) or
              (revents bitand POLLNVAL)) and !readagain)
        {
          xlog_write("poll detected file close, calling ptr->handle_close()", __FILE__, __LINE__);
          ptr->handle_close();
        }
      }

      // Lets check if there has been an activity on the internal signal byte
      if (fdset[0].revents)
      {
        std::deque<ReactorMsg> nl;
        m_notifq->pull( nl );
        for (std::deque<ReactorMsg>::iterator n = nl.begin(); n != nl.end();++n)
        {
          handle_reactor_msg(*n);
        }
      }
    }


    // look for clients that we can safely delete
    // {
    //   cpp11::lock_guard<cpp11::mutex> guard(m_clients.lock);

    //   std::set<ReactorClient*> removedlist;
    //   for (std::vector<ReactorClient*>::iterator iter = m_clients.ptrs.begin();
    //        iter != m_clients.ptrs.end(); ++iter)
    //   {
    //     ReactorClient* client = *iter;

    //     if (client->m_release_count == 1)
    //     {
    //       // step 1: mark the release time, to introduce a saftey delay
    //       client->m_delete_time = time(NULL) + 10;
    //       client->m_release_count = 2;
    //     }
    //     else if (client->m_release_count == 2)
    //     {
    //       if (time(NULL) > client->m_delete_time)
    //       {
    //         removedlist.insert( client );
    //         client->m_release_count == 3;
    //       }
    //     }
    //   }

    //   if (!removedlist.empty())
    //   {
    //     std::vector<ReactorClient*> temp;
    //     for (std::vector<ReactorClient*>::iterator it = m_clients.ptrs.begin();
    //          it != m_clients.ptrs.end(); ++it)
    //     {
    //       if (removedlist.find(*it) == removedlist.end()) temp.push_back(*it);
    //     }
    //     m_clients.ptrs.swap( temp );
    //   }
    // }


  } // while


}

//----------------------------------------------------------------------

void Reactor::invalidate()
{
  // Danger here: this method is exposed to the user-application; they might
  // end up pushing lots of data onto the pipe.
  m_notifq->push_msg(ReactorMsg(ReactorMsg::eNoEvent));
}

//----------------------------------------------------------------------

// void Reactor::request_close(ReactorClient* client)
// {
//   xlog_write("request_close", __FILE__, __LINE__);
//   ReactorMsg msg(ReactorMsg::eClose, client);
//   m_notifq->push_msg(msg);
// }

// //----------------------------------------------------------------------

// void Reactor::request_shutdown(ReactorClient* client)
// {
//   // before we push the pointer (for later callback), check it has not expired
//   cpp11::lock_guard<cpp11::mutex> guard(m_clients.lock);
//   ReactorMsg msg(ReactorMsg::eShutdown, client);
//   m_notifq->push_msg(msg);
// }

//----------------------------------------------------------------------

// void Reactor::request_release(ReactorClient* client)
// {
//   ReactorMsg msg(ReactorMsg::eRelease, client);
//   m_notifq->push_msg(msg);
// }

//----------------------------------------------------------------------

void Reactor::add_client(ReactorClient* client)
{
  _DEBUG_(m_log, "reactor: request to add new client, fd " << client->fd());
  ReactorMsg msg(ReactorMsg::eAdd, client);
  m_notifq->push_msg(msg);
}

//----------------------------------------------------------------------

void Reactor::request_attn()
{
  ReactorMsg msg(ReactorMsg::eAttn);
  m_notifq->push_msg(msg);
}

//----------------------------------------------------------------------

void Reactor::handle_reactor_msg(const ReactorMsg& msg)
{
  xlog_write("ReactorMsg::handle_reactor_msg", __FILE__, __LINE__);
  switch(msg.type)
  {
    case ReactorMsg::eNoEvent  : break;
    // case ReactorMsg::eClose      :
    // {
    //   xlog_write("ReactorMsg::eClose", __FILE__, __LINE__);
    //   xlog_write("msg.ptr->handle_close()", __FILE__, __LINE__);
    //   msg.ptr->handle_close();
    //   break;
    // }
    // case ReactorMsg::eShutdown     :
    // {
    //   xlog_write("ReactorMsg::eShutdown", __FILE__, __LINE__);
    //   if (msg.ptr->io_open())
    //   {
    //     _DEBUG_(m_log, "reactor: shutdown(fd=" << msg.ptr->fd() << ", SHUT_WR)");

    //     // TODO: this must be moved into the client, and, protected, so that
    //     // shutdown cannot be called more than once.
    //     ::shutdown(msg.ptr->fd(), SHUT_WR);
    //   }
    //   break;
    // }
    case ReactorMsg::eAdd :
    {
      xlog_write("ReactorMsg::eAdd", __FILE__, __LINE__);
//      cpp11::lock_guard<cpp11::mutex> guard(m_clients.lock);
      m_clients.ptrs.push_back( msg.ptr );
      _DEBUG_(m_log, "reactor: added new client, fd " << msg.ptr->fd()
              << ", total clients " << m_clients.ptrs.size());
      break;
    }
    // case ReactorMsg::eRelease :
    // {
    //   xlog_write("ReactorMsg::eRelease", __FILE__, __LINE__);

    //   msg.ptr->m_release_count = 1; // mark as released

    //   cpp11::lock_guard<cpp11::mutex> guard(m_clients.lock);

    //   _DEBUG_(m_log, "reactor: destroying client, fd " << msg.ptr->fd());

    //   std::vector<ReactorClient*>::iterator it
    //     = std::find(m_clients.ptrs.begin(), m_clients.ptrs.end(), msg.ptr);
    //   if (it != m_clients.ptrs.end()) m_clients.ptrs.erase(it);
    //   delete msg.ptr;

    //   if (m_clients.ptrs.size() == 0) m_clients.ptrs_empty.notify_all();
    //   break;
    // }
    case ReactorMsg::eTerminate :
    {
      xlog_write("ReactorMsg::eTerminate", __FILE__, __LINE__);
      m_is_stopping = true; //normally already set
      break;
    }
    case ReactorMsg::eAttn :
    {
      xlog_write("ReactorMsg::eAttn", __FILE__, __LINE__);
      attend_clients();
      break;
    }
    default:
    {
      _ERROR_(m_log, "uknown message type on reactor pipe");
      break;
    }
  }
}

//----------------------------------------------------------------------

void Reactor::attend_clients()
{
  xlog_write("--> Reactor::attend_clients", __FILE__, __LINE__);
  std::set<ReactorClient*> removedlist;

  // TODO: can we reduce the scope of this lock?
//  cpp11::lock_guard<cpp11::mutex> guard(m_clients.lock);


  for (std::vector<ReactorClient*>::iterator it = m_clients.ptrs.begin();
       it != m_clients.ptrs.end(); ++it)
  {
    ReactorClient* client = *it;

    int attn_flags = client->attn_flag();
    if (attn_flags bitand ReactorClient::eWantShutdown)
    {
      if (client->io_open())
      {
        xlog_write("attend_clients -> Shutdown", __FILE__, __LINE__);
        client->handle_shutdown();
      }
    }
    if (attn_flags bitand ReactorClient::eWantClose)
    {
      if (client->io_open())
      {
        xlog_write("attend_clients -> close", __FILE__, __LINE__);
        client->handle_close();
      }
    }
    if (attn_flags bitand ReactorClient::eWantDelete)
    {
      xlog_write("attend_clients -> insert onto remove list", __FILE__, __LINE__);
      removedlist.insert( client );
    }
  }

  /* perform actual deletion */
  if (!removedlist.empty())
  {
    std::vector<ReactorClient*> temp;
    temp.reserve( m_clients.ptrs.size() - removedlist.size() );

    for (std::vector<ReactorClient*>::iterator it = m_clients.ptrs.begin();
         it != m_clients.ptrs.end(); ++it)
    {
      ReactorClient* client = *it;
      if (removedlist.find(client) != removedlist.end())
      {
        xlog_write("attend_clients -> handle_close before delete!!!", __FILE__, __LINE__);
        if (client->io_open()) client->handle_close();
        xlog_write("attend_clients -> deleting client!!!", __FILE__, __LINE__);
        delete client;
      }
      else
      {
        temp.push_back(*it);
      }
    }
    m_clients.ptrs.swap( temp );
  }

  xlog_write("<-- Reactor::attend_clients", __FILE__, __LINE__);
}

//----------------------------------------------------------------------
} // namespace exio
