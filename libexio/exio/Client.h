#ifndef EXIO_CLIENT_H
#define EXIO_CLIENT_H

#include "thread.h"
#include "mutex.h"
#include "atomic.h"
#include "condition_variable.h"

#include <queue>
#include <deque>
#include <list>

namespace exio {


  class LogService;
  class Reactor;
  class RawMsg;

#define EXIO_CLIENT_CHUNK_SIZE 1024  // TODO: move to using dynamic memory


class ReactorClient
{
  public:

    ReactorClient(Reactor* r, int fd);

    virtual int  handle_input() = 0;
    virtual void handle_output() = 0;
    virtual void handle_close() = 0;
    virtual int  events() = 0;

    int      fd()       const { return m_fd; }

    Reactor* reactor() { return m_reactor; }

    bool    io_open() const { return m_io_open; }

    size_t  bytes_out()  const { return m_bytes_out; }
    size_t  bytes_in()   const { return m_bytes_in; }
    time_t  last_write() const { return m_last_write; }
    time_t  last_read()  const { return m_last_read; }

  protected:
    virtual ~ReactorClient(){}

    void     set_io_closed()       { m_io_open=false; }

  private:
    ReactorClient(const ReactorClient&);
    ReactorClient& operator=(const ReactorClient&);

    Reactor* m_reactor;
    int      m_fd;
    bool     m_io_open;

  protected:
    size_t   m_bytes_out;
    size_t   m_bytes_in;
    time_t   m_last_write;
    time_t   m_last_read;

    friend class Reactor;
};


class Client;
class ReactorReadBuffer;

class ClientCallback
{
  public:
    virtual ~ClientCallback(){}


    /** Data has arrived on the the underlying IO session */
    virtual size_t process_input(Client*, const char*, int) = 0;

    /** Underlying IO session has closed */
    virtual void   process_close(Client*) = 0;

};

/* TODO: rename this to BufferedClient */
class Client : public ReactorClient
{
  public:
    Client(Reactor*,
           int fd,
           LogService*,
           ClientCallback*);  // TODO: Calback should be beofre log svc

    void release();

    /* Queue data to send */
    int queue(const char*, size_t, bool request_close = false);
    void close();

    /* Has peer socket been closed? */
    bool iovalid() const { return !m_io_closed; }


    int       task_lwp() const { return m_task_lwp; }
    pthread_t task_tid() const { return m_task_tid; }


    size_t pending_out() const;

    // TODO: add pending_in() method.  Little more tricky, because pending
    // bytes are on both the queue and the memory buffer, so, need to ready
    // from two locations.


  protected:

    ~Client();

  private:
    Client(const Client&); // no copy
    Client& operator=(const Client&); // no assignment

    void task_thread_TEP();
    int  process_inbound_bytes(ReactorReadBuffer&);

    void request_task_thread_stop();

    /* Reactor callbacks */
    int handle_input();
    void handle_output();
    void handle_close();
    int  events();

    void shutdown_outq();

    LogService* m_logsvc;

    cpp11::atomic_bool m_io_closed;

    bool m_log_io_events;

    struct DataChunk
    {
        char   buf[EXIO_CLIENT_CHUNK_SIZE];
        size_t size;
        size_t start;

        DataChunk() : size(0), start(0) {}
    };

    struct DataFifo
    {
        cpp11::mutex              mutex;
        std::list < DataChunk >   items;   // TODO: deque better?
        size_t                    itemcount;
        cpp11::condition_variable cond;
        size_t                    pending;  // total data size in queue
        DataFifo();
    } m_datafifo;

    struct OutboundQueue
    {
        mutable cpp11::mutex mutex;
        std::list<RawMsg>    items;  // TODO: deque better?
        size_t               itemcount;
        bool                 acceptmore;
        size_t               pending;  // total data size in queue
        OutboundQueue();
    } m_out_q;

    cpp11::thread *    m_task_thread;
    pthread_t          m_task_tid;
    int                m_task_lwp;

    cpp11::mutex     m_cb_mtx; // callback mutex
    ClientCallback * m_cb;       // null => callbacks no longer allowed

    size_t m_out_pend_max;

  protected:


    friend class Reactor;
};

} // namespace exio

#endif
