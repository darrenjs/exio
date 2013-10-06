#include "exio/Client.h"
#include "exio/utils.h"
#include "exio/Logger.h"
#include "exio/AppSvc.h"
#include "exio/Reactor.h"
#include "exio/ReactorReadBuffer.h"

// extern "C"
// {
// #include <xlog/xlog.h>
// }

#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/syscall.h>

namespace exio {

/*

TODO: what I should have learnt

NOTE: aside from these ponts, scan throught ht erest of this file, and the
Reactor, for other things I've learnt about threading and IO

- two patterns for a threading server

- how to do the close

- handling massive messages

- test case: looking for mem leaks

- there are two kinds of shutdown: normal-controlled, and
  emergency-uncontrolled.  this is the pattern used in EXIO.  Thre are two
  because there are two considerations: saftey and correctness.  Saftey is the
  main concern; that is about preventing core dumps.  Correctness is about
  ensuring race-conditions are handled etc.



*/

// TODO: can I usnify thte buffer types?

// TODO: also, need to test the code for memory leaks
struct RawMsg
{
  private:

    // memory region, used for the later delete []
    char* mem;

  public:

    // these store the raw data remaining to be processed
    char*  ptr;
    size_t size;

    RawMsg()
      : mem(0),
        ptr(0),
        size(0)
    {
    }

    RawMsg(const char* src, size_t srclen)
      : mem( new char[srclen]),
        ptr(mem),
        size(srclen)
    {
      memcpy(mem, src, srclen);
    }

    ~RawMsg()
    {
      /* note: no automatic release of the memory */
    }

    bool operator==(const RawMsg&rhs)
    {
      return this->mem == rhs.mem and this->ptr==rhs.ptr and this->size==rhs.size;
    }

    void freemem()
    {
      delete [] mem;
      mem=0;
    }

    void eat(size_t s)
    {
      ptr  += s;
      size -= s;
    }
};

// class RawMsgGuard
// {
//   public:
//     RawMsgGuard() : m_buf(0) { }
//     RawMsgGuard(RawMsg * b) : m_buf(b) { }
//     ~RawMsgGuard() { if (m_buf) m_buf->freemem(); }


//     RawMsg* buf() { return m_buf; }

//     void reset(RawMsg* b) { m_buf = b; }

//   private:
//     RawMsgGuard(RawMsgGuard&);
//     RawMsgGuard& operator=(RawMsgGuard&);
//     RawMsg* m_buf;
// };



  Client::DataFifo::DataFifo()
    : itemcount(0),
      pending(0)
  {
  }

  Client::OutboundQueue::OutboundQueue()
    : itemcount(0),
      acceptmore(true),
      pending(0)
  {
  }


//----------------------------------------------------------------------
/* Constructor */
Client::Client(Reactor* reactor,
               int fd,
               LogService* log,
               ClientCallback* cb)
  : ReactorClient(reactor, fd),
    m_logsvc(log),
    m_io_closed(false),
    m_log_io_events(true),
    m_task_thread(NULL),
    m_cb(cb)
{

  // Danger: creation of internal thread should be last action of constructor,
  // so that object construction is complete before accessed by thread.
  m_task_thread = new cpp11::thread(&Client::task_thread_TEP, this);
}

//----------------------------------------------------------------------
/* Destructor */
Client::~Client()   /* REACTOR THREAD */
{
  // TODO: we have a design problem here: what if self object is destructed on
  // the TASK thread!  This suggests a new design.

  // TODO: note down this principle: we want to ensure safety, not
  // correctness. If the user application destroys a client thread without
  // ensuring pending data has been written, then the framework will not
  // compensate. Outbound data might be lost.

  // NOTE: derived classes should already have set this before coming in
  // here. This is one of the earliest checks we need to perform during
  // destruction, to prevent callbacks into the dervied-class which has
  // already destructed!
  {
    cpp11::lock_guard<cpp11::mutex> guard( m_cb_mtx );
    if (m_cb)
    {
      m_cb = NULL;
      _WARN_(m_logsvc, "Client destructing but callbacks are still enabled!");
    }
  }

  /* stop the internal thread */
  request_task_thread_stop();
  m_task_thread -> join();
  delete m_task_thread;

  /* clear up any memory */
  shutdown_outq();
  m_datafifo.items.clear();

//  xlog_write("Client::~Client", __FILE__, __LINE__);
}
//----------------------------------------------------------------------
void Client::shutdown_outq()
{
  cpp11::lock_guard<cpp11::mutex> guard( m_out_q.mutex );
  m_out_q.acceptmore = false;

  for (std::list<RawMsg>::iterator i = m_out_q.items.begin();
       i != m_out_q.items.end(); ++i)
  {
    i->freemem();
  }
  m_out_q.items.clear();

  m_out_q.itemcount= 0;
  m_out_q.pending = 0;
}
//----------------------------------------------------------------------
void Client::task_thread_TEP()
{
  m_task_tid = pthread_self();
  m_task_lwp = syscall(SYS_gettid);

//  xlog_write("task_thread_TEP", __FILE__, __LINE__);

  // This is long lived; must not be destructed, because that would cause
  // pending bytes to be lost.  Normally would put this input the
  // process_inbound_bytes() method, however that is able to exit.
  ReactorReadBuffer buf(4096);
  bool should_exit = false;
  while (!should_exit)
  {
    try
    {
      if (process_inbound_bytes(buf)) should_exit = true;
    }
    catch(const std::exception& e)
    {
      _WARN_(m_logsvc, "exception processing socket data: " <<e.what());
    }
    catch(...)
    {
      _WARN_(m_logsvc, "exception processing socket data: unknown" );
    }
  }

  {
    cpp11::lock_guard<cpp11::mutex> guard( m_cb_mtx );
    if (m_cb) m_cb->process_close(this);
  }

  shutdown_outq();
}
//----------------------------------------------------------------------
int Client::process_inbound_bytes(ReactorReadBuffer&buf)
{
  bool should_exit = false;
  do
  {
    {
      /* Note the requirement for unique_lock here.  This is the type expected
         by the condition variable wait() method, because the condition
         variable will need to release and acquire the lock as wait() is
         entered and exited.
       */
      cpp11::unique_lock<cpp11::mutex> lock( m_datafifo.mutex );

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
      while (m_datafifo.itemcount == 0)
      {
        m_datafifo.cond.wait( lock );
      }



/*

hmmm .... might have STL::list problem here again

#0  0x00007f4ba4e2b6c2 in std::_List_const_iterator<exio::Client::DataChunk>::operator++ (this=0x7f4ba356d9b0) at /usr/include/c++/4.7/bits/stl_list.h:236
#1  0x00007f4ba4e2b38d in std::__distance<std::_List_const_iterator<exio::Client::DataChunk> > (__first=..., __last=...) at /usr/include/c++/4.7/bits/stl_iterator_base_funcs.h:82
#2  0x00007f4ba4e2aecc in std::distance<std::_List_const_iterator<exio::Client::DataChunk> > (__first=..., __last=...) at /usr/include/c++/4.7/bits/stl_iterator_base_funcs.h:118
#3  0x00007f4ba4e2a999 in std::list<exio::Client::DataChunk, std::allocator<exio::Client::DataChunk> >::size (this=0x171c418) at /usr/include/c++/4.7/bits/stl_list.h:855


TODO: hmmm .... maybe instead, change the logic ... use a swap to extract all
of the data quickly, and then use an iterator..  Should be better that
modifying the list all the time.
*/
      // copy all pending data into our inbound-buffer

      while (m_datafifo.itemcount)
      {
        DataChunk& front = m_datafifo.items.front();

        if (front.size == 0) // got sentinel object
        {
          // don't process any more pending inbound chunks after receiving the
          // sentinel object (however, for data already in the inbound-buffer,
          // we need to pass that back to the client).
          should_exit = true;
          break;
        }
        else
        {
          while (buf.space_remain() < front.size)
          {
            // TODO: need to handle throw on growth error
            _DEBUG_(m_logsvc, "client: growing buffer, currently at " << buf.capacity());
            buf.grow(); // throws on failure
          }

          // The cancopy amount is just front.size(). We can sure of this due
          // to the preceeding growth. Ie, we don't need to compare it to the
          // space remaining in the buffer.
          size_t cancopy = front.size;
          if (cancopy)
          {
            // good, readbuffer can accept more data
            memcpy(buf.wp(), &(front.buf[front.start]), cancopy);
            buf.incr_bytesavail( cancopy );

            front.size  -= cancopy;
            front.start += cancopy;
          }
        }

        m_datafifo.itemcount--;
        m_datafifo.items.pop_front();  // no need to check if front has been used up
      }

    } //   -*-*-*-*- end of IO lock -*-*-*-*


    // We have released the lock, which will allow the IO thread to
    // proceed. Now pass our bytes to the derived method.
    while ( buf.bytesavail() )
    {
      // TODO: what happens if an exception is thrown?
      size_t consumed = 0;

      {
        cpp11::lock_guard<cpp11::mutex> guard( m_cb_mtx );
        if (m_cb)
        {
          try
          {
            consumed = m_cb->process_input(this, buf.rp(), buf.bytesavail());
          }
          catch (...)
          {

            // TODO: what should we do here?
            std::string leading(buf.rp(), 20);
            _WARN_(m_logsvc, "*** EXCEPTION: [" << leading << "]");

          }
        }
      }

      if (consumed>0)
        buf.consume( consumed );
      else
        break;
    }

    buf.shift_pending_to_array_start();

  } while (should_exit == false);

  return 1;  // return >0 to imply thread exit
}
//----------------------------------------------------------------------
void Client::handle_input()  /* REACTOR THREAD */
{
  /* called by the reactor thread */

  if ( !iovalid() ) return;

  // TODO: here, we should check how much memory is pending? Either add that
  // in, or, move to using a synchronized read/write buffer.

  DataChunk chunk;

  ssize_t n = read(fd(), chunk.buf, EXIO_CLIENT_CHUNK_SIZE);
  int const _err = errno;

  if (m_log_io_events)
  {
    std::ostringstream os;
    os << "read() " << n
       << ", errno " << _err
       << " (" << utils::strerror(_err) << ")";

    if (n > 0)
    {
      std::string raw(chunk.buf, n);
      os << ", bytes '" << raw << "'";
    }
    _DEBUG_(m_logsvc, os.str());
  }

  if (n == 0) // zero indicates end of file
  {
    _DEBUG_(m_logsvc, "eof when reading fd " << fd());

    // request a controlled shutdown
    if (reactor()) reactor()->request_close(this);
  }
  else if (n < 0)
  {
    _INFO_(m_logsvc, "socket read failed: " << utils::strerror(_err) );

    // request a controlled shutdown
    if (reactor()) reactor()->request_close(this);
  }
  else if (n > 0 )
  {
    // if we have reached here, then we have successfully read new bytes from
    // the socket, and placed them into the read buffer
    chunk.size = n;
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_datafifo.mutex );
      m_datafifo.items.push_back( chunk );
      m_datafifo.itemcount++;
      m_datafifo.cond.notify_all();
    }
  }
}
//----------------------------------------------------------------------
int Client::events()  /* REACTOR THREAD */
{
  int events = 0;

  if (not m_io_closed)
  {
    events = POLLIN;
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_out_q.mutex );

      if ( m_out_q.itemcount        ||
           (!m_out_q.items.empty()) ||
           (m_out_q.items.size()>0)
        ) events |= POLLOUT;
    }
  }

  return events;
}

//----------------------------------------------------------------------
void Client::queue(const char* buf, size_t size, bool request_close)
{
  bool invalidate_reactor = false;

  {
    cpp11::lock_guard<cpp11::mutex> guard( m_out_q.mutex );

    if (m_out_q.acceptmore == false) return;

    if (buf and size)
    {
      // TODO: could throw due to out of memory
      m_out_q.items.push_back( RawMsg(buf, size) );
      m_out_q.itemcount++;
      m_out_q.pending += size;
      invalidate_reactor = true;
    }

    if (request_close)
    {
      // TODO: could throw due to out of memory
      // push an empty item to indicate that IO thread should then perform an io
      // close after any earlier data has been written.
      m_out_q.items.push_back( RawMsg() );
      m_out_q.itemcount++;
      m_out_q.acceptmore = false;
      invalidate_reactor = true;
    }

  }

  if (invalidate_reactor) reactor()->invalidate();
}
//----------------------------------------------------------------------
void Client::close()
{
  queue(NULL, 0, true);
}
//----------------------------------------------------------------------
size_t Client::pending_out() const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_out_q.mutex );
  return m_out_q.pending;
}
//----------------------------------------------------------------------
void Client::handle_output() /* REACTOR THREAD */
{
  /* we can now write bytes to the socket, without blocking  */
  while (true)
  {
    RawMsg * next = NULL;

    // get the next data item to write
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_out_q.mutex );

      if (m_out_q.itemcount or !m_out_q.items.empty() or m_out_q.items.size())
      {
        next = &(m_out_q.items.front());
      }
      else
      {
        m_out_q.itemcount = 0;
        return;
      }
    }

    // handle close-io sentinel object
    if (*next == RawMsg())
    {
      if (reactor()) reactor()->request_close(this); // request controlled shutdown
      return;
    }

    /* Note: now using send() instead of write(), so that we can prevent
     * SIGPIPE from being raised */
    const int flags = MSG_DONTWAIT bitor MSG_NOSIGNAL;

    size_t const written = next->size;
    ssize_t n = send(fd(), next->ptr, written , flags);
    int const _err = errno;

    try {
      if (m_log_io_events)
      {
        std::ostringstream os;
        os << "send(fd"<<fd()<<","<<written<<")=" << n
           << ", errno " << _err
           << " (" << utils::strerror(_err) << ")";
        _DEBUG_(m_logsvc, os.str());
      }
    } catch (...){}

    if (n == -1)
    {
      if (_err == EAGAIN or _err == EWOULDBLOCK)
      {
        // TODO: what to do here?
        //std::cout << "EAGAIN on socket " << fd;  // Add to logger
      }
      else
      {
        _WARN_(m_logsvc,
               "socket write failed, fd " << fd() << ": "
               << utils::strerror(_err) );

        if (reactor()) reactor()->request_close(this); // request controlled shutdown
        return;
      }
    }

    if (n>0)
    {
      next->eat(n);

      cpp11::lock_guard<cpp11::mutex> guard( m_out_q.mutex );
      m_out_q.pending = (m_out_q.pending>(size_t)n)? (m_out_q.pending-n) : 0;

      // if all bytes written, pop from the queue
      if (next->size == 0)
      {
        next->freemem();
        m_out_q.items.erase( m_out_q.items.begin() );
        m_out_q.itemcount--;
      }
    }

    // Return to the reactor, so that other events can be serviced.
    // get the next data item to write
    return;
  } // while
}
//----------------------------------------------------------------------
void Client::handle_close()   /* REACTOR THREAD */
{
  /* this is the controlled-shutdown-sequence */

  if (not m_io_closed)
  {
    _DEBUG_(m_logsvc, "close(fd" << fd() <<")");

    // Note: important that we only make one call to close.
    m_io_closed = true;
    ::close(fd());

    // request stop of the task thread (which invokes the derived-class
    // callback to notify of closure).
    _DEBUG_(m_logsvc, "*** client: request task thread to stop");
    request_task_thread_stop();
  }
}
//----------------------------------------------------------------------
void Client::request_task_thread_stop()
{
  cpp11::lock_guard<cpp11::mutex> guard( m_datafifo.mutex );

  m_datafifo.items.push_back( DataChunk() );
  m_datafifo.itemcount++;
  m_datafifo.cond.notify_all();
}
//----------------------------------------------------------------------
void Client::release()
{
  // TODO: consider using a recursive mutex, instead of checking thread id

  // Very first action is to prevent further callbacks into the client.  The
  // lock is taken to ensure there is no callback in progress.
  if (pthread_self() == m_task_tid)
  {
    m_cb = NULL;
  }
  else
  {
    cpp11::lock_guard<cpp11::mutex> guard( m_cb_mtx );
    m_cb = NULL;
  }

  // request a close, just in case user-application forgot
  if (reactor()) reactor()->request_close(this);
  if (reactor()) reactor()->request_delete(this);
}
//----------------------------------------------------------------------
} // namespace exio

/*

  THOUGHTS  THOUGHTS  THOUGHTS  THOUGHTS


THOUGHTS

  >>>> ALSO: get xlog working again. NEXT: specify filename

Problem 1.
I think the "new" problem, with the admin "help" not working, is due to the
invalidate & close events (now) being processed together.  Something I have
wanted before, and maybe will fix this, is ability for UA to pass data and
then signifiy "close after all written".  Maybe make that the default
behaviour? Also, I would like to unify the close() and release().  But, for
now, maybe make it so that one of the final checks the handle_output does is
to see if the Client has been released, and if so, requests a close and then a
delete provided there is no data pending a write.

  ---> BUT; is it still "right" to process pending events in the notifq all at
       the same time?


Problem 2.  Serious memory leak.  Then seeks to have started happening after
the introduction of the std::list for the notifcation queue.  Perhaps look at
a simple way for doing it?

Problem 3. Bad messages being seen at the received application.  Still have no
idea about this one.

Problem 4.  Admin commands are freezing (on T41).

Problem 5.  Why are admin processing taking up so much CPU? (on  T41)

OTHER:

ubuntu: enable gdb

ubuntu: enable xterm accept (-nolisten option)


*/
