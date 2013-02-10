#ifndef EXIO_ADMINIO_H
#define EXIO_ADMINIO_H

#include "exio/sam.h"

#include "thread.h"

#include "condition_variable.h"
#include <queue>

namespace exio
{
class AppSvc;

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

    size_t capacity() const { return sam::MAX_MSG_LEN; }

    char*       buf()       { return m_buf; }
    const char* buf() const { return m_buf; }

    QueuedItem()
      :  size(0),
         flags(0)
    {}

    void release()  // better to just use the constructor?
    {
      size  = 0;
      flags = 0;
    }

  private:
    char   m_buf[sam::MAX_MSG_LEN];  // TODO: better to use dynamic memory
};




/* make this look like C++ */
class AtomicInt
{
    int m_value;
    mutable cpp11::mutex m_mutex;

  public:

    explicit AtomicInt(int i = 0) : m_value(i) {}

    int value() const
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
      return  m_value;
    }

    void value( int n )
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
      m_value = n;
    }

    void incr()
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
      m_value++;
    }

  private:
    AtomicInt(const AtomicInt&);
    AtomicInt& operator=(const AtomicInt&);
};


/* make this look like C++ */
class AtomicBool
{
    bool m_value;
    mutable cpp11::mutex m_mutex;

  public:

    explicit AtomicBool(bool b) : m_value(b) {}

    bool operator=(bool __i)
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
      m_value = __i;
      return m_value;
    }

    operator bool() const
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
      return  m_value;
    }

  private:
    AtomicBool(const AtomicBool&);
    AtomicBool& operator=(const AtomicBool&);
};


class AdminIO
{
  public:
    class Listener
    {
      public:
        virtual void io_onmsg(const sam::txMessage& src) = 0;
        virtual void io_closed() = 0;
        virtual ~Listener() {}
    };

  public:
    explicit AdminIO(AppSvc& appsvc,
                     int fd,
                     Listener *);
    ~AdminIO();

    void enqueue(const QueuedItem& i);

    bool stopping() const;
    void request_stop();


    bool safe_to_delete() const;

    time_t last_write() const { return m_last_write; }

  private:
    AdminIO(const AdminIO&); // no copy
    AdminIO& operator=(const AdminIO&); // no assignment

    void socket_read_TEP();
    void read_from_socket();

    void socket_write_TEP();
    void socket_write();
    void wait_and_pop(QueuedItem& value);

    void push_item(const QueuedItem& i);

    AppSvc& m_appsvc;
    Listener * m_listener;
    int m_fd;
    bool m_session_valid;  // TODO: make atomic

    AtomicBool m_is_stopping;  // true if stopping


    struct ProtectedQueue
    {
        mutable cpp11::mutex mutex;
        std::queue<QueuedItem> items;
        cpp11::condition_variable convar;
    } m_q;

    AtomicInt m_threads_complete;

    AtomicBool m_have_data;

    // always place member threads last, so that the get started after other
    // data is ready
    enum { NumberInternalThreads = 2 };
    cpp11::thread * m_read_thread;
    cpp11::thread * m_write_thread;

    bool m_log_io_events; // for debugging

    // IO stats
    time_t m_last_write;
    size_t m_total_out;
};

} // namespace qm

#endif
