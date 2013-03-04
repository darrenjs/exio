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
    volatile int m_value;
    mutable cpp11::mutex m_mutex;

  public:

    explicit AtomicInt(int i = 0) : m_value(i) {}

    int value() const
    {
      cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
      return m_value;
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

    // IO stats
    time_t        last_write() const { return m_last_write; }
    unsigned long bytes_out()  const { return m_bytesout; }
    unsigned long bytes_in()   const { return m_bytesin; }


    int reader_lwp() const { return m_lwp.reader; }
    int writer_lwp() const { return m_lwp.writer; }

  private:
    AdminIO(const AdminIO&); // no copy
    AdminIO& operator=(const AdminIO&); // no assignment

    void socket_read_TEP();
    void read_from_socket();

    void socket_write_TEP();
    void socket_write();
    void wait_and_pop(QueuedItem& value);

    void push_item(const QueuedItem& i);

    // Thread IDs
    struct ThreadIDs
    {
        int reader;
        int writer;
        AtomicInt count;
        ThreadIDs():reader(0), writer(0), count(0) {}
    } m_lwp;

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

    // IO stats
    unsigned long m_bytesout;
    unsigned long m_bytesin;
    time_t m_last_write;

    bool m_log_io_events; // for debugging

    // always place member threads last, so that the get started after other
    // data is ready
    enum { NumberInternalThreads = 2 };
    cpp11::thread * m_read_thread;
    cpp11::thread * m_write_thread;

};

} // namespace qm

#endif
