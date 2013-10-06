#ifndef EXIO_REACTORREADBUFFER_H
#define EXIO_REACTORREADBUFFER_H

#include <string.h>
#include <sstream>

namespace exio {

class ReactorReadBuffer
{
  public:

    ReactorReadBuffer(size_t cap = 1024)
      : m_buf_cap(cap /* initial size */),
        m_buf(new char[m_buf_cap])
    {
      zero();
    }

    ~ReactorReadBuffer()
    {
      delete [] m_buf;
    }

    void zero()
    {
      memset(m_buf, 0, m_buf_cap);
      m_bytesavail = 0;
      m_rp         = 0;
    }

    size_t capacity() const { return m_buf_cap; }

    char* wp() const { return m_buf + m_bytesavail; }
    void incr_bytesavail(size_t t) { m_bytesavail += t; }

    size_t space_remain() const { return m_buf_cap - m_bytesavail; }

    size_t bytesavail() const { return m_bytesavail; }

    size_t grow();

    /* Any used bytes in the buffer are moved to the start of the array. */
    void shift_pending_to_array_start()
    {
      if (m_bytesavail > 0 and m_rp) memmove(m_buf, m_buf+m_rp, m_bytesavail);
      m_rp = 0;
    }

    const char* rp() const { return m_buf+m_rp; }

    void consume(size_t n) { m_rp += n;  m_bytesavail -= n; }

    void tostream(std::ostream& os); // debug info

  private:
    ReactorReadBuffer(const ReactorReadBuffer&);
    ReactorReadBuffer& operator=(const ReactorReadBuffer&);

    signed long long m_buf_cap;
    char*  m_buf;
    size_t m_bytesavail;
    size_t m_rp;
};

} // namespace exio

#endif
