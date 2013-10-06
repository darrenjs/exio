#include "exio/ReactorReadBuffer.h"

#include <sstream>
#include <stdexcept>

namespace exio {

//----------------------------------------------------------------------
void ReactorReadBuffer::tostream(std::ostream& os)
{
  os << "Buffer: cap=" << m_buf_cap
     << ", avail=" << m_bytesavail
     << ", space=" << space_remain()
     << ", rp=" << m_rp;
}

//----------------------------------------------------------------------
size_t ReactorReadBuffer::grow()
{
  signed long long extra = m_buf_cap*0.5;
  if (extra == 0) extra = 1;

  signed long long new_cap = m_buf_cap + extra;  // could overflow

  // TODO: need to test growth failure
  // if (new_cap > 100)
  //   throw std::runtime_error("ReactorReadBuffer test exception");

  if (new_cap < m_buf_cap || new_cap <= 0)
  {
    std::ostringstream os;
    os << "Failed to allocate extra " << extra << " bytes for socket buffer growth. Current capacity=" << m_buf_cap;
    throw std::runtime_error(os.str().c_str());
  }

  char* new_buf = NULL;

  try
  {
    new_buf = new char[new_cap];
  }
  catch (const std::bad_alloc& e)
  {
    new_buf = NULL;
  }

  if (new_buf == NULL)
  {
    std::ostringstream os;
    os << "Failed to allocate " << new_cap << " bytes for socket buffer growth";
    throw std::runtime_error(os.str().c_str());
  }

  memset(new_buf, 0, new_cap);
  memcpy(new_buf, m_buf, m_buf_cap);

  delete [] m_buf;
  m_buf     = new_buf;
  m_buf_cap = new_cap;

  return extra;
}

} // namespace exio
