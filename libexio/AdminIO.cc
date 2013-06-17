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
#include "exio/AdminIO.h"
#include "exio/AdminIOListener.h"
#include "exio/AdminInterface.h"
#include "exio/Logger.h"
#include "exio/sam.h"
#include "exio/utils.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>

#define READ_BUF_SIZE sam::MAX_MSG_LEN

namespace exio {

  struct io_error
  {
      int  _errno;
      bool _eof;

      io_error(): _errno(0), _eof(true) {}
      io_error(int err): _errno(err), _eof(false) {}
  };

// std::string string_error(int e)
// {
//   std::string retval;

//   char errbuf[256];
//   memset(errbuf, 0, sizeof(errbuf));

// /*

// TODO: here I should be using the proper feature tests for the XSI
// implementation of strerror_r .  See man page.

//   (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE

// */

// #ifdef _GNU_SOURCE
//   return strerror_r(e, errbuf, sizeof(errbuf)-1);
// #else
//   // XSI implementation
//   if (strerror_r(e, errbuf, sizeof(errbuf)-1) == 0)


// #endif

//   return "unknown";
// }

//----------------------------------------------------------------------
AdminIO::ThreadIDs::ThreadIDs()
  : reader(0),
    writer(0),
    reader_pthread(0),
    writer_pthread(0),
    count(0)
{
}

//----------------------------------------------------------------------
/* Constructor */
AdminIO::AdminIO(AppSvc& appsvc,int fd, AdminIOListener * l)
  :  m_appsvc(appsvc),
     m_listener( l ),
     m_fd(fd),
     m_is_stopping( false ),
     m_threads_complete(0),
     m_have_data( false ),
     m_bytesout(0),
     m_bytesin(0),
     m_last_write(0),
     m_log_io_events(false),
     m_read_thread( new cpp11::thread(&AdminIO::socket_read_TEP, this)),
     m_write_thread( new cpp11::thread(&AdminIO::socket_write_TEP, this))
{
  // before we return, lets try to wait a little while so that the thread IDs
  // of the socket reader & writer threads get assigned
  int loops = 0;
  while (m_lwp.count.value() != 2 and loops++ < 10)
  {
    usleep(10000);
  }
}

//----------------------------------------------------------------------
/* Destructor */
AdminIO::~AdminIO()
{
  // Ensure our thread has been joined. Note: it is criticaly important that
  // internal threads are joined with before we continue to delete other data
  // members, because we want to avoid the common threading bug where internal
  // threads are left running after member data has been deleted; as soon as
  // one of those internal threads next comes alive and attempts to use the
  // deleted member data, a hard to identify error will occur!.
  m_read_thread->join();
  delete m_read_thread;

  m_write_thread->join();
  delete m_write_thread;
}

//----------------------------------------------------------------------

bool read_timeo(int fd, int sec, int usec)
{
  /*
    This function is more or less taken verbatim from UNIX Network
    Programming, Stevens et al.
  */

  fd_set readset;
  FD_ZERO(&readset);
  FD_SET(fd, &readset);

  struct timeval tv;
  tv.tv_sec  = sec;
  tv.tv_usec = usec;

  // prepare params
  int nfds = fd + 1;  // nfds is the highest-numbered file descriptor, +1
  fd_set *readfds = &readset;
  fd_set *writefds = NULL;  /* not watching write events */
  fd_set *exceptfds = NULL; /* not watching for exceptions */
  struct timeval * timeout = &tv;

  // return the number of file descriptors now ready, or -1 if error. 0
  // indicates timeout

  // TODO: select is not the modern approach to socket IO.  Investigate epoll
  int nready = select(nfds,
                      readfds,
                      writefds,
                      exceptfds,
                      timeout);

  return nready > 0;
}
//----------------------------------------------------------------------
void AdminIO::socket_read_TEP()
{
  m_lwp.reader = syscall(SYS_gettid);
  m_lwp.reader_pthread = pthread_self();
  m_lwp.count.incr();

  bool call_request_stop = true;
  try
  {
    // If read_from_socket returns, it means the write-thread detected the
    // socket close, and notified the read thread to terminate normally.
    // However, if the read thread detects the need to close the IO, then it
    // will throw, and so the IO shutdown will be triggered from here.
    read_from_socket();
    call_request_stop = false;
  }
  catch (const io_error& ioerr)
  {
    if (not ioerr._eof)
    {
      _WARN_(m_appsvc.log(),
              "socket read failed: "
             << utils::strerror(ioerr._errno) );
    }
  }
  catch (std::runtime_error& e)
  {
    _WARN_(m_appsvc.log(), "socket read failed: " << e.what());
  }
  catch (...)
  {
    /* Prevent unknown exceptions from causing uncontrolled stack unwind.  I
       have observed that the only exception that gets to here is one that
       arised when the peer has closed the socket. */

  }

  // termiante the write thread
  if (call_request_stop)  request_stop();

  m_threads_complete.incr(); /* LAST INSTRUCTION OF THREAD */
}
//----------------------------------------------------------------------
void AdminIO::read_from_socket()
{
  // Read from the socket
  char buf[READ_BUF_SIZE];
  bzero(buf, READ_BUF_SIZE);

  sam::SAMProtocol samp;
  sam::txMessage msg;

  // number of bytes in buf[] waiting to be processed
  size_t bytesavail = 0;

  while (true)

    // TODO: maybe make it configurable to perform blocking read?
//  while ( ( buflen = read(m_fd, p, (READ_BUF_SIZE-buflen)) ) > 0)

  {
    // check the socket for data, but timeout after a short while to allow
    // this thread to exit if the session its being programmatically terminated
    while (! m_is_stopping and !read_timeo(m_fd, 1, 0)) {};
    if ( m_is_stopping ) return;

    if (bytesavail == READ_BUF_SIZE)
    {
      //_INFO_(m_appsvc.log(), "bytesavail  " << bytesavail << " - buffer full");
      throw std::runtime_error("input buffer full");
    }

    char* wp = buf + bytesavail; // write pointer
    ssize_t n = read( m_fd, wp, (READ_BUF_SIZE - bytesavail) );
    int const _err = errno;

    if (n>0) m_bytesin += n;

    if (m_log_io_events)
    {
      std::ostringstream os;
      os << "read() " << n
         << ", errno " << _err
         << " (" << utils::strerror(_err) << ")";
      _INFO_(m_appsvc.log(), os.str());
    }

    if ( m_is_stopping ) return;

    if (n == 0) // zero indicates end of file
    {
      _INFO_(m_appsvc.log(), "eof when reading fd " << m_fd);
      throw io_error(); // empty imples eof
    }
    else if (n < 0)
    {
      throw io_error( _err );
    }

    bytesavail += n; // increase count of waiting bytes

    char* rp = buf;  // read pointer

    while ( true )
    {
      size_t consumed = 0;
      try
      {
        consumed = samp.decodeMsg(msg, rp, rp+bytesavail);
      }
      catch (const std::exception& e)
      {
        _ERROR_(m_appsvc.log(), "Received bad data. Closing connection.");

        // send error reply
        sam::txMessage msg("badmsg");
        sam::SAMProtocol protocol;

        QueuedItem qi;
        try
        {
          qi.size = protocol.encodeMsg(msg, qi.buf(), qi.capacity());
          enqueue( qi );
        }
        catch (sam::OverFlow& err)
        {
          _ERROR_(m_appsvc.log(), "Failed to encode message: " << err.what());
        }

        throw;
      }

      if (consumed > 0)
      {
        if (m_listener)
        {
          try {
            m_listener->io_onmsg( msg );
          }
          catch (...)
          {
            /* ignore exception thrown by client

            this was added (10/02/13) when I observed exceptions being thrown
            during the processes of encoding a response, were finding their
            way up to here.
            */
          }
        }
        // consume the data from our buffer
        rp         += consumed;
        bytesavail -= consumed;
      }

      // Only break out of loop if no bytes were consumed. If we processed at
      // least one message, then attempt to decode again, because there might
      // be another complete message in the buffer.
      if (consumed == 0) break;
    }

    /* Any used bytes in the buffer are moved to the start of the array. */
    if (bytesavail > 0 and rp > buf) memmove(buf, rp, bytesavail);
    bzero(buf+bytesavail, READ_BUF_SIZE - bytesavail);
  }
}
//----------------------------------------------------------------------
void AdminIO::enqueue(const QueuedItem &i)
{
  push_item(i);
}
//----------------------------------------------------------------------
void AdminIO::push_item(const QueuedItem &i)
{
  {
    cpp11::lock_guard<cpp11::mutex> guard( m_q.mutex );
    m_q.bytes_pending += i.size;
    m_q.items.push( i );
    m_have_data = true;
    m_q.convar.notify_one();
  }
}
//----------------------------------------------------------------------
unsigned long AdminIO::bytes_pending() const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_q.mutex );
  return m_q.bytes_pending;
}
//----------------------------------------------------------------------
void AdminIO::wait_and_pop(QueuedItem& value)
{
//  _INFO_(">>> AdminIO::wait_and_pop");

  /* Note the requirement for unique_lock here.  This is the type expected
   * by the condition variable wait() method, because the condition
   * variable will need to release and acquire the lock as wait() is
   * entered and exited.
   */

  cpp11::unique_lock<cpp11::mutex> lock( m_q.mutex );

  /* Loop/condition predicate here is a separate boolean flag.  Initially the
   * predicate was simply a call to -- m_q.items.empty() -- however,
   * strangely, that did not always work. There was some race-condition, which
   * was fairly well repeatable. It seems that somehow, when using empty(), it
   * was possible to add an item to the queue and for empty() to still be
   * true, and this even when the cal to push(), and the call to empty(), were
   * pretected with mutexes.  Replaced the empty() with size()!=0 fixed the
   * problem.  Because I didn't understand why empty() failed and size()
   * worked, here I've gone for additional robustness, but using an atomic
   * boolean approach, such that the code within the predicate is (1) simple
   * and (2) has extra synchronization around it.
   */
  while ( not m_have_data )
  {
//    _INFO_("writer giong into wait" );

    m_q.convar.wait(lock);
//    _INFO_("writer awake");
  }

  value = m_q.items.front();
  m_q.items.pop();
  m_q.bytes_pending -= value.size;

  m_have_data = (m_q.items.size() > 0)? true : false;

//  _INFO_("<<< AdminIO::wait_and_pop");
}
//----------------------------------------------------------------------
void AdminIO::socket_write_TEP()
{
  m_lwp.writer = syscall(SYS_gettid);
  m_lwp.writer_pthread = pthread_self();
  m_lwp.count.incr();

  try
  {
    socket_write();
  }
  catch (...)
  {
    /* catch all exceptions to prevent uncontrolled unwind / termination */
  }

  m_threads_complete.incr(); /* LAST INSTRUCTION OF THREAD */
}
//----------------------------------------------------------------------
void AdminIO::socket_write()
{
  enum
  {
    Unknown = 0,
    QIKILL,
    FLAG_BEFORE_READ,
    WRITE_ERR,
    WHILE
  } exit_reason = Unknown;

  bool socket_was_closed = false;
  try
  {

    while ( not m_is_stopping )
    {
      exit_reason = Unknown;

      QueuedItem item_to_write;
      wait_and_pop( item_to_write );

      // Note: here we can't be sure of the blocking/non-blocking state of the
      // socket. TODO: why not?
      size_t bytes_done = 0;

      // Note: reason we check m_is_stopping in the while condition is so that
      // we make a check of it before the socket write.  A common way for this
      // write thread to terminate is for the read thread to put a kill item
      // on queue and /or set the m_is_stopping flag.  So before a socket
      // write, always test for validity, other we might hit the socket-alias
      // race condition (although, we have not fully elliminated the race
      // condition here).
      while ( not m_is_stopping and bytes_done < item_to_write.size)
      {
        int fd = m_fd;

        /* Note: now using send() instead of write(), so that we can prevent
         * SIGPIPE from being raised */
        const int flags = MSG_DONTWAIT bitor MSG_NOSIGNAL;
        char* buf       = item_to_write.buf()+bytes_done;
        size_t len      = item_to_write.size-bytes_done;

        // TODO:slow_consumer - need this send() call not to block, so that we
        // can timeouts in the case of a slow consumer.
        ssize_t n = send(fd, buf, len, flags);
//        ssize_t n = write(fd, qi.buf()+bytes_done, qi.size-bytes_done);
        int const _err = errno;

        if (m_log_io_events)
        {
          std::ostringstream os;
          os << "send() " << n
             << ", errno " << _err
             << " (" << utils::strerror(_err) << ")";
          _INFO_(m_appsvc.log(), os.str());
        }

        if (n == -1)
        {
          if (_err == EAGAIN or _err == EWOULDBLOCK)
          {
            //std::cout << "EAGAIN on socket " << fd;  // Add to logger
            usleep(100000);  // wait for a litte while
          }
          else
          {
            exit_reason = WRITE_ERR;
            _WARN_(m_appsvc.log(),
                   "socket write failed, fd " << m_fd << ": "
                   << utils::strerror(_err) );
            m_is_stopping = true;
          }
        }
        else
        {
          bytes_done += n;
          m_last_write = time(NULL);
          if (n>0) m_bytesout+=n;
        }
      }

      if (item_to_write.flags bitand QueuedItem::eThreadKill)
      {
        /* we have been signalled to actively close the connection */

        m_is_stopping = true;  // will timeout the read thread

        _INFO_(m_appsvc.log(), "closing socket fd " << m_fd);

        struct linger linger;
        linger.l_onoff  = 1;
        linger.l_linger = 5;  /* seconds to wait */
        setsockopt(m_fd,
                   SOL_SOCKET, SO_LINGER,
                   (const char *) &linger,
                   sizeof(linger));

        // TODO: here, before the close, is there a way to check the socket is
        // still what we think it is?  I.e., test it is valid, and get its
        // address. I.e., to remove the socket alias close bug, where we
        // accidentally close a different connection.
        close( m_fd );
        socket_was_closed = true;

        if (exit_reason == Unknown) exit_reason = QIKILL;
      }

      item_to_write.release();
      if (exit_reason == Unknown)  exit_reason = WHILE;
    }
  }
  catch(...)
  {
    /* Ignore exceptions.  This block exists just to prevent exceptions from
     * terminating this function. */
  }

  // Just in case an exception was thrown ... or a loop break was inserted at
  // a later date.
  m_is_stopping = true;  // will timeout the read thread
  if (not socket_was_closed) close( m_fd ); // only close if not previously closed

  // _INFO_("write-thread stopping, reason " << exit_reason
  //        << " is_stopping: " << m_is_stopping
  //        << ", pthread_self " << pthread_self() );

  if (m_listener) m_listener->io_closed();  // exceptions caught in TEP

  // TODO: if we move to holding a list of memory-pointers, that list will
  // need to be manually cleared here (ie each element will need to have its
  // cleanup method called), and all done in a critical section.

}

//----------------------------------------------------------------------

bool AdminIO::safe_to_delete() const
{
  return (m_is_stopping
          and m_threads_complete.value() == AdminIO::NumberInternalThreads );
}

//----------------------------------------------------------------------
bool AdminIO::stopping() const
{
  return m_is_stopping;
}

//----------------------------------------------------------------------
void AdminIO::request_stop()
{
  QueuedItem qi;
  qi.flags = qi.flags bitor QueuedItem::eThreadKill;
  push_item( qi );
}

//----------------------------------------------------------------------
}  // namespace qm
