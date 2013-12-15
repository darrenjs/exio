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
#include "exio/AdminServerSocket.h"
#include "exio/AdminInterfaceImpl.h"
#include "exio/Logger.h"
#include "exio/AppSvc.h"
#include "exio/utils.h"

#include <sstream>
#include <stdexcept>
#include <stdlib.h>

#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

// WARNING: before changing this value, consider that the housekeeping time
// interval & thread might be used somewhere within exio for other
// purposes. E.g., the thread could be driving heartbeating etc.
#define ACCEPT_TIMEOUT_SECS 10


//----------------------------------------------------------------------
/** Create an IPv4 TCP socket
 *
 * Returns a valid file descriptor, or throws if there was an error.
 */
int SocketIPv4()
{

  int socket_family = AF_INET;
  int socket_type = SOCK_STREAM;
  int protocol = 0;

  int fd = socket(socket_family, socket_type, protocol);
  int __errno = errno;

  if (fd < 0)
  {
    std::ostringstream err;
    err << "socket() failed: " << exio::utils::strerror(__errno);
    throw std::runtime_error(err.str());
  }

  return fd;
}

//----------------------------------------------------------------------
int Bind(int sockfd,
         const struct sockaddr *addr,
         socklen_t addrlen)
{
  if (bind(sockfd, addr, addrlen) < 0)
  {
    int __errno = errno;

    std::ostringstream err;
    err << "bind() failed: " << exio::utils::strerror(__errno);
    throw std::runtime_error(err.str());
  }

  return 0;
}

//----------------------------------------------------------------------
void Listen(int sockfd, int backlog)
{
  if (listen(sockfd, backlog) < 0)
  {
    int __errno = errno;
    std::ostringstream err;
    err << "listen() failed: " << exio::utils::strerror(__errno);
    throw std::runtime_error( err.str() );
  }
}
//----------------------------------------------------------------------
/*
 * Attempt to accept an incoming connection.  Will block for a configurable
 * amount of time.
 *
 * Return: on success, returns a non-negative integer, which is the descriptor
 * for the new connected socket.  On timeout, returns -1. On any error
 * condition will throw an exception.
 *
 * TODO: return to here, and make sure it is throwing exceptions for all error
 * situations.
 */
int Accept(int sockfd,
           struct sockaddr *addr,
           socklen_t *addrlen,
           exio::LogService * logsvc)
{
  int fd = -1;

  // set the server socket to non-blocking, because we are going to use poll
  int fdflags = fcntl(sockfd, F_GETFD);
  fcntl( sockfd, F_SETFD, fdflags bitor O_NONBLOCK);

  struct pollfd fds_array[1];
  nfds_t nfds = 1;

  int timeout = ACCEPT_TIMEOUT_SECS * 1000;  /* milliseconds */

  bool continue_to_poll = true;
  while ( continue_to_poll )
  {
    memset(&fds_array, 0, sizeof(fds_array));
    fds_array[0].fd     = sockfd;
    fds_array[0].events = POLLIN;

    // struct pollfd {
    //     int   fd;         /* file descriptor */
    //     short events;     /* requested events */
    //     short revents;    /* returned events */
    // };

    int n = poll(fds_array, nfds, timeout); // will block

    if (n)
    {
      // a positive 'n' is number of structures with nonzero revents fields
      if (fds_array[0].revents bitand POLLIN)
      {
        if ( (fd = accept(sockfd, addr, addrlen)) < 0)
        {
          int lasterrno = errno;
          throw std::runtime_error( exio::utils::strerror(lasterrno) );
        }
        continue_to_poll = false;
      }
      else
      {
        // TODO: error, we should never come here
      }
    }
    else if (n == 0)
    {
      /* timeout */
      continue_to_poll = false;
    }
    else
    {
      /* error condition */
      int _errno = errno;

      std::ostringstream os;
      os << "poll() failed: " << exio::utils::strerror(_errno);
      _WARN_(logsvc, os.str() );
      throw std::runtime_error( os.str() );
    }

  } // while

  return fd;
}

namespace exio
{


//----------------------------------------------------------------------
AdminServerSocket::AdminServerSocket(AdminInterfaceImpl* ai)
  : m_aii(ai),
    m_threadid(0)
{
  /* CAUTION: don't try to use the m_ai parameter in here, because that object
   * itself it likely to still be under initialisation. */
}

//----------------------------------------------------------------------
void AdminServerSocket::create_listen_socket()
{
  /* Create an IPv4 socket */
  m_servfd = SocketIPv4();

  /* Specify socket address */
  sockaddr_in sockaddr;
  bzero(&sockaddr, sizeof(sockaddr));
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  sockaddr.sin_port  = htons(m_aii->appsvc().conf().server_port );

  int optval = 1;
  setsockopt(m_servfd, SOL_SOCKET, SO_REUSEADDR, (&optval), sizeof(optval));

  /* Bind our socket to its a address */
  Bind(m_servfd,(struct sockaddr*) &sockaddr, sizeof(sockaddr) );

  /* Mark as a passive socket */
  Listen(m_servfd, 100);
}
//----------------------------------------------------------------------
void AdminServerSocket::accept_TEP()
{
  bool enable_server = (m_aii->appsvc().conf().server_port != EXIO_NO_SERVER);

  if (enable_server)
  {
    create_listen_socket();
    _INFO_(m_aii->appsvc().log(),
           "Listening on port " << m_aii->appsvc().conf().server_port);
  }

  m_threadid  = syscall(SYS_gettid);
  m_pthreadid = pthread_self();

  int conn_count = 0;

  time_t last_house_keep = ::time(NULL);
  time_t conncount_on_last_house = conn_count;

  // TODO: need a way to singal this loop to stop.
  while (true)
  {
    if (!enable_server)  {  /* non-server mode*/  sleep(ACCEPT_TIMEOUT_SECS);  }
    else
    {
      sockaddr_in clientaddr;
      socklen_t addrlen = sizeof(clientaddr);
      memset(&clientaddr, 0, sizeof(clientaddr));

      int connfd = -1;

      /* attempt to accept a new connection */
      try
      {
        connfd = Accept(m_servfd, (struct sockaddr*) &clientaddr, &addrlen,
                        m_aii->appsvc().log());
        conn_count++;
      }
      catch (const std::exception& e)
      {
        _ERROR_(m_aii->appsvc().log(),
                "accept() failed: " << e.what() );
      }
      catch (...)
      {
        _ERROR_(m_aii->appsvc().log(),
                "accept() failed: unknown exception" );
      }

      if ( connfd < 0)
      {
        /* timeout or exception */
      }
      else
      {
        bool session_created_ok = false;
        try
        {

#ifdef SO_KEEPALIVE
          /* Set keepalives on the socket to detect dropped connections. */
          {
            int keepalive = 1;
            if (setsockopt (connfd, SOL_SOCKET, SO_KEEPALIVE,
                            (char *) &keepalive, sizeof (keepalive)) < 0)
            {
              int __errno = errno;
              _WARN_(m_aii->appsvc().log(),"setsockopt (SO_KEEPALIVE): "
                     << exio::utils::strerror(__errno));
            }
          }
#endif

          m_aii->createNewSession( connfd );
          session_created_ok = true;
        }
        catch (const std::exception& e)
        {
          _ERROR_(m_aii->appsvc().log(),
                  "createNewSession() failed: " << e.what() );
        }
        catch (...)
        {
          _ERROR_(m_aii->appsvc().log(),
                  "createNewSession() failed: unknown exception" );
        }

        if (!session_created_ok)
        {
          _WARN_(m_aii->appsvc().log(), "closing socket " << connfd
                 << " because session creation failed");
          ::close( connfd );
        }
      }
    }

    // housekeeping - ensure that housekeeping is calling at least once during
    // some fixed interval, or at least once when a number of connections have
    // been accepted.  Doing it this way (rather than on the accept timeout)
    // ensures that housekeeping will always get called, even during period of
    // high frequency socket accepts.

    if ( ( (::time(NULL) - last_house_keep) >= ACCEPT_TIMEOUT_SECS) or
         (conn_count - conncount_on_last_house > 10)
      )
    {
      //_INFO_(m_aii->appsvc().log(), "housekeeping");
      try {  m_aii->housekeeping();  } catch (...) {}
      last_house_keep = ::time(NULL);
      conncount_on_last_house = conn_count;
    }

    // Saftey mechanism.  To prevent excessive activity of this loop, lets
    // sleep between accept attempts.  This protects us from a malicious
    // remote application which is make repeated connection attempts.  It also
    // protects us if, do to some temporary error condition of the OS, the
    // socket poll is failing.  The sleep is at the end of the while-loop so
    // that we don't sleep at module startup; ie, as soon as this class is
    // instantiated, we are listening for incoming connections without delay.
    //
    // Another reason we need this is to prevent the accrual of too many
    // threads, since each thread takes up a lot of stack memory (eg 8MB).
    //
    // The sleep value is now chosen to a few tens of ms.  This should offer
    // protection against rapid access, and, is not to noticable for normal
    // user interaction.

    usleep( 50000 );
  } // while


  _INFO_(m_aii->appsvc().log(), "server socket thread exiting" );
}

//----------------------------------------------------------------------

void AdminServerSocket::start()
{
  cpp11::thread thr(&AdminServerSocket::accept_TEP, this);
  thr.detach();
}

//----------------------------------------------------------------------
std::pair<pthread_t, int> AdminServerSocket::thread_ids() const
{
  std::pair<pthread_t, int> t;
  t.first  = m_pthreadid;
  t.second = m_threadid;
  return t;
}

//----------------------------------------------------------------------

}
