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
#ifndef EXIO_CLIENT_H
#define EXIO_CLIENT_H

#include "thread.h"
#include "mutex.h"
#include "atomic.h"
#include "condition_variable.h"

#include "exio/ReactorReadBuffer.h"

#include <queue>
#include <deque>
#include <list>

#include <stdint.h>

namespace exio {


  class LogService;
  class Reactor;
  class RawMsg;

#define EXIO_CLIENT_CHUNK_SIZE 1024  // TODO: move to using dynamic memory


class ReactorClient
{
  public:
    enum IOState { IO_default = 0,
                   IO_close,
                   IO_read_again };
  public:

    ReactorClient(Reactor* r, int fd);

    virtual IOState handle_input() = 0;
    virtual IOState handle_output() = 0;

    virtual void handle_close() = 0;

    virtual int  events() = 0;
    virtual bool has_work() = 0;

    int      fd()       const { return m_fd; }

    Reactor* reactor() { return m_reactor; }

    bool    io_open() const { return m_io_closed==false; }

    uint64_t bytes_out()  const { return m_bytes_out; }
    uint64_t bytes_in()   const { return m_bytes_in; }
    time_t  last_write() const { return m_last_write; }
    time_t  last_read()  const { return m_last_read; }

    void update_run_state_for_worker(char& oldstate, char& newstate);
    void update_run_state_for_reactor(char& oldstate, char& newstate);
    void set_run_state();
    char get_run_state();


    virtual void do_work() = 0;

    enum AttnBits
    {
      //eWantClose    = 0x01,
      //eWantShutdown = 0x02,
      eWantDelete   = 0x04,
      eShutdownDone = 0x08
    };

    int attn_flag() const;

  protected:
    virtual ~ReactorClient(){}

  private:
    ReactorClient(const ReactorClient&);
    ReactorClient& operator=(const ReactorClient&);
    int destroy_cycle(time_t);

    Reactor* m_reactor;
    int      m_fd;


  protected:
    cpp11::atomic_bool m_io_closed;

    uint64_t m_bytes_out;
    uint64_t m_bytes_in;
    time_t   m_last_write;
    time_t   m_last_read;

  protected:
    int                   m_attn_flags;
    mutable cpp11::mutex  m_attn_mutex;

    time_t   m_tdestroy;

    char m_runstate;
    cpp11::mutex m_runstate_mutex;


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

    /* Queue data to send  / close socket */
    int queue(const char*, size_t, bool request_close = false);

    size_t pending_out() const;

    // TODO: add pending_in() method.  Little more tricky, because pending
    // bytes are on both the queue and the memory buffer, so, need to ready
    // from two locations.

    virtual void do_work() ;

  protected:

    ~Client();

  private:
    Client(const Client&); // no copy
    Client& operator=(const Client&); // no assignment

    int  process_inbound_bytes(ReactorReadBuffer&);


    /* Reactor callbacks */

    virtual ReactorClient::IOState handle_input();
    virtual ReactorClient::IOState handle_output();
    virtual bool has_work() ;

    void handle_close();

    int  events();
    void do_shutdown_SHUT_WR();

    void shutdown_outq();

    LogService* m_logsvc;


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


    cpp11::recursive_mutex m_cb_mtx;
    ClientCallback *       m_cb; // null => callbacks not allowed

    size_t m_out_pend_max;
    ReactorReadBuffer m_buf;

  protected:


    friend class Reactor;
};

} // namespace exio

#endif
