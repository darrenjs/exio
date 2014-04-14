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

/* Feature test, to safely use POLLRDHUP */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "exio/Reactor.h"
#include "exio/Logger.h"
#include "exio/AdminInterface.h"
#include "exio/AdminInterfaceImpl.h"
#include "exio/utils.h"
#include "exio/Logger.h"

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

/* If POLLRDHUP is not defined, then lets define it so that its later usage
 * does nothing. */
#ifndef POLLRDHUP
#define POLLRDHUP 0x00
#endif

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
      eDelete,
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
        case eDelete     : return "eDelete";
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

    void push_msg(const ReactorMsg& m)
    {
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

      // write to pipe -- TODO: should handle error?
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
Reactor::Reactor(LogService* log, int nworkers)
  : m_log(log),
    m_is_stopping(false),
    m_last_cleanup(0),
    m_notifq(NULL),
    m_io(NULL),
    m_thr_ids(1+nworkers)
{

  m_pipefd[0]=-1;  // reader
  m_pipefd[1]=-1;  // writer

  //  In Linux versions before 2.6.11, the capacity of a pipe was the same as
  //  the system page size (e.g., 4096 bytes on i386).  Since Linux 2.6.11,
  //  the pipe capacity is 65536 bytes.

  // create our pipes
  if (pipe(m_pipefd) != 0)
  {
    int err = errno;
    _WARN_(m_log,
           "pipe failed: "
           << utils::strerror(err) );
    m_pipefd[0]=-1;
    m_pipefd[1]=-1;

    throw std::runtime_error("cannot create pipe");
  }

  // set internal pipe to non-blocking
  bool fcntlerr = false;
  int fl;
  fl = ::fcntl(m_pipefd[0], F_GETFL); if (fl < 0) fcntlerr = true;
  if (::fcntl(m_pipefd[0], F_SETFL, fl | O_NONBLOCK) < 0)  fcntlerr = true;
  fl = ::fcntl(m_pipefd[1], F_GETFL);  if (fl < 0) fcntlerr = true;
  if (::fcntl(m_pipefd[1], F_SETFL, fl | O_NONBLOCK) < 0) fcntlerr = true;
  if (fcntlerr) throw std::runtime_error("cannot set pipe to O_NONBLOCK");

  m_notifq = new ReactorNotifQ(m_pipefd[0], m_pipefd[1]);

  /* create internal threads last as last step of object construction */

  for (int i = 0; i < nworkers; i++)
  {
    m_workers.push_back( new cpp11::thread(&Reactor::worker_TEP, this, i)  );
  }

  m_io = new cpp11::thread(&Reactor::reactor_io_TEP, this);
}

//----------------------------------------------------------------------
/* Destructor */
Reactor::~Reactor()
{
  /* Stop reactor thread */
  m_is_stopping = true;
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

  // I have decided to shut down workers after the reactor. This is because
  // the reactor is a source of input for the workers, so once the reactor has
  // shutdown, the workers cannot get anymore work.

  /* shutdown worker threads */
  {
    cpp11::lock_guard<cpp11::mutex> guard( m_runq.mutex );
    m_runq.items.push_back(NULL);
    m_runq.itemcount++;
    m_runq.cond.notify_all();
  }
  for (std::vector<cpp11::thread*>::iterator it = m_workers.begin();
       it!=m_workers.end(); it++)
  {
    cpp11::thread * thr = *it;
    thr->join();
    delete thr;
    *it = NULL;
  }

  close(m_pipefd[0]);
  close(m_pipefd[1]);

  /* final attempt to close sockets */
  for (std::vector<ReactorClient*>::iterator iter = m_clients.begin();
       iter != m_clients.end(); ++iter)
  {
    ReactorClient* client = *iter;
    // client->handle_shutdown();
    client->handle_close();
    delete client;
  }

}

//----------------------------------------------------------------------
void Reactor::reactor_io_TEP()
{
  m_thr_ids[0].first  = pthread_self();
  m_thr_ids[0].second = syscall(SYS_gettid);

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
    // added or removed.  The only thing I need to do each time is just call
    // events() to get the state of each client.
    {
//      cpp11::lock_guard<cpp11::mutex> guard(m_clients.lock);

      // build the array of pollfd, and companion array of client-id's
      for (std::vector<ReactorClient*>::iterator iter = m_clients.begin();
           iter != m_clients.end(); ++iter)
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

    // Are there client objects that are going through their closure
    // death-cycle .. if so, need a timeout. Choose a 1 second interval.
    int timeout = (!m_destroying.empty() || m_destroying.size())? 1000:-1;

    int nready = ::poll(&fdset[0], fdset.size(), timeout);

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
        int              iost      = ReactorClient::IO_default;


        // TODO: bug in here.  iost can be overrwritten by a POLLOUT event,
        // after the POLLIN event has been called.

        if (revents bitand POLLIN)
        {
          if (ptr->io_open()) iost or_eq ptr->handle_input();
        }

        if (revents bitand POLLPRI) { /* don't handle POLLPRI */ }

        if (revents bitand POLLOUT)
        {
          if (ptr->io_open()) iost or_eq ptr->handle_output();
        }

        if ( (((revents bitand POLLRDHUP) or
               (revents bitand POLLERR)   or
               (revents bitand POLLHUP)   or
               (revents bitand POLLNVAL))
              and
              ((iost bitand ReactorClient::IO_read_again) == 0))
//              iost != ReactorClient::IO_read_again)
             or (iost bitand ReactorClient::IO_close) )
        {
          ptr->handle_close();
        }
      }

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

    /* search for clients with work to do */
    // TODO: this can be made more efficient, based on the earlier IO
    for (std::vector<ReactorClient*>::iterator it = m_clients.begin();
         it != m_clients.end(); ++it)
    {
      ReactorClient* client = *it;

      char oldstate;
      char newstate;

      client->update_run_state_for_reactor(oldstate, newstate);

      if (oldstate=='N' and newstate=='Q')
      {
        cpp11::lock_guard<cpp11::mutex> guard( m_runq.mutex );
        m_runq.items.push_back(client);
        m_runq.itemcount++;

        m_runq.cond.notify_one();
      }
    }

    /* Destruction cycle */
    time_t now = ::time(NULL);
    std::set<ReactorClient*> deletenow;
    if (!m_destroying.empty() and
        (   ((now > m_last_cleanup) and ((now-m_last_cleanup)>2))
         or (now < m_last_cleanup) ))
    {
      m_last_cleanup = now;

      for (std::set<ReactorClient*>::iterator it = m_destroying.begin();
           it != m_destroying.end(); ++it)
      {
        ReactorClient* client = *it;
        if (client->destroy_cycle(now))
        {

          // TODO: note: before we mark a client for deletion, we must ensure
          // it is not queued.  Also, maybe, we should set the client to have
          // a state, ie, K could be its state which means, ready to be
          // killed ????
          if (client->get_run_state() == 'N')
          {
            deletenow.insert(client);
          }
        }
      }
    }

    if (!deletenow.empty())
    {
      /* rebuild the client list */
      std::vector<ReactorClient*> temp;
      for (std::vector<ReactorClient*>::iterator it = m_clients.begin();
           it != m_clients.end(); ++it)
      {
        if (deletenow.find(*it) == deletenow.end()) temp.push_back(*it);
      }
      m_clients.swap( temp );

      /* perform actual deletion */
      for (std::set<ReactorClient*>::iterator it = deletenow.begin();
           it != deletenow.end(); ++it)
      {
        ReactorClient* client = *it;
        m_destroying.erase(*it);
        delete client;
      }
    }

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
  switch(msg.type)
  {
    case ReactorMsg::eNoEvent  : break;
    // case ReactorMsg::eClose      :
    // {
    //   msg.ptr->handle_close();
    //   break;
    // }
    // case ReactorMsg::eShutdown     :
    // {
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
      m_clients.push_back( msg.ptr );
      break;
    }
    case ReactorMsg::eTerminate :
    {
      m_is_stopping = true; //normally already set
      break;
    }
    case ReactorMsg::eAttn :
    {
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

  for (std::vector<ReactorClient*>::iterator it = m_clients.begin();
       it != m_clients.end(); ++it)
  {
    ReactorClient* client = *it;

    int const attn = client->attn_flag();

    // if ( (attn & (ReactorClient::eWantShutdown|ReactorClient::eShutdownDone))
    //      == ReactorClient::eWantShutdown)
    // {
    //   if (client->io_open())
    //   {
    //     client->handle_shutdown();
    //   }
    // }

    if (attn bitand ReactorClient::eWantDelete)
    {
      // TODO: need to raise another flag, on the client, to prevent coming
      // here again and again and again
      m_destroying.insert( client );
    }
  }
}

//----------------------------------------------------------------------
void Reactor::worker_TEP(int index)
{
  m_thr_ids[index+1].first  = pthread_self();
  m_thr_ids[index+1].second = syscall(SYS_gettid);

  while(true)
  {
    try
    {
     bool exitnow = worker_TEP_impl();
     if (exitnow) return;
    }
    catch(const std::exception& e)
    {
      _WARN_(m_log, "exception from worker_TEP_impl(): " <<e.what());
    }
    catch(...)
    {
      _WARN_(m_log, "exception from worker_TEP_impl(): unknown");
    }
  }
}
//----------------------------------------------------------------------
bool Reactor::worker_TEP_impl()
{
  ReactorClient* client = NULL;
  {
    /* Note the requirement for unique_lock here.  This is the type expected
       by the condition variable wait() method, because the condition
       variable will need to release and acquire the lock as wait() is
       entered and exited.
    */
    cpp11::unique_lock<cpp11::mutex> lock( m_runq.mutex );

    /* Loop/condition predicate here is a separate boolean flag.  Initially
     * the predicate was simply a call to -- m_q.items.empty() -- however,
     * strangely, that did not always work. There was some race-condition,
     * which was fairly well repeatable. It seems that somehow, when using
     * empty(), it was possible to add an item to the queue and for empty()
     * to still be true, and this even when the call to push(), and the call
     * to empty(), were pretected with mutexes.  Replaced the empty() with
     * size()!=0 fixed the problem.  Because I didn't understand why empty()
     * failed and size() worked, here I've gone for additional robustness,
     * but using an boolean approach.
     *
     * NOTE: comment came from a problem found in AdminIO.cc code.
     */
    while (m_runq.itemcount == 0)
    {
      m_runq.cond.wait( lock );
    }

    // take the top item requiring work
    client = m_runq.items.front();

    if (client == NULL)
    {
      // A null pointer signifies "end thread", so we return.  To allow other
      // threads to also return, we leave the item on the queue.
      return true;
    }

    m_runq.items.pop_front();
    m_runq.itemcount--;
  }

  // work on the client while it has pending inbound data
  client->set_run_state();
  while (true)
  {
    try
    {
      client->do_work();
    }
    catch(const std::exception& e)
    {
      _WARN_(m_log, "exception processing socket data: " <<e.what());
    }
    catch(...)
    {
      _WARN_(m_log, "exception processing socket data: unknown" );
    }

    char oldstate;
    char newstate;
    client->update_run_state_for_worker(oldstate, newstate);

    if (newstate != 'R') break;
  }

  return false;
}

//----------------------------------------------------------------------

const std::vector< std::pair<pthread_t,int> >& Reactor::thread_ids() const
{
  return m_thr_ids;
}

//----------------------------------------------------------------------

} // namespace exio
