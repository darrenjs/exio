#include "exio/SamBuffer.h"
#include "exio/sam.h"

#include <iostream>
#include <string.h>
#include <stdio.h>

#define RESERVE_FOR_SAM_HEADER 100

namespace exio {

//----------------------------------------------------------------------

// FixedSamBuffer::FixedSamBuffer()
//   : m_msg_start(m_buf),
//     m_write_ptr(m_buf),
//     m_end(m_buf + sam::MAX_MSG_LEN)
// {
//   // reserve initial space for writing header information

//   // TODO: throw if len is too small
//   m_msg_start += RESERVE_FOR_SAM_HEADER;
//   m_write_ptr  = m_msg_start;
// }
// //----------------------------------------------------------------------
// FixedSamBuffer::FixedSamBuffer(const FixedSamBuffer& src)
// {
//   memcpy(this->m_buf, src.m_buf, sam::MAX_MSG_LEN);
//   this->m_msg_start = this->m_buf + (src.m_msg_start-src.m_buf);
//   this->m_write_ptr = this->m_buf + (src.m_write_ptr-src.m_buf);
//   this->m_end = this->m_buf + (src.m_end-src.m_buf);
// }
// //----------------------------------------------------------------------
// FixedSamBuffer& FixedSamBuffer::operator=(const FixedSamBuffer& src)
// {
//   memcpy(this->m_buf, src.m_buf, sam::MAX_MSG_LEN);
//   this->m_msg_start = this->m_buf + (src.m_msg_start-src.m_buf);
//   this->m_write_ptr = this->m_buf + (src.m_write_ptr-src.m_buf);
//   this->m_end = this->m_buf + (src.m_end-src.m_buf);

//   return *this;
// }
// //----------------------------------------------------------------------
// void FixedSamBuffer::check_space(size_t n)
// {
//   if (n >= (size_t)(m_end-m_write_ptr))
//   {
//     throw sam::OverFlow(sam::OverFlow::eEncodingBufferTooSmall);
//   }
// }

// //----------------------------------------------------------------------
// void FixedSamBuffer::append(const char* src, size_t srclen)
// {
//   check_space( srclen );

//   memcpy(m_write_ptr, src, srclen);
//   m_write_ptr += srclen;
// }

// //----------------------------------------------------------------------
// void FixedSamBuffer::append(char src)
// {
//   check_space( 1 );

//   *m_write_ptr = src;
//   m_write_ptr += 1;
// }

// //----------------------------------------------------------------------
// size_t FixedSamBuffer::msg_size() const
// {
//   std::ptrdiff_t d = m_write_ptr - m_msg_start;
//   return d;
// }
// const char*  FixedSamBuffer::msg_start() const { return m_msg_start; }

// //----------------------------------------------------------------------
// void FixedSamBuffer::encode_header()
// {
//   size_t msglen = this->msg_size();

//   size_t headlen = 1 + 7 + 1; // MSG_START + SAM_HDR + META_DELIM
//   msglen += headlen;

//   const size_t practical_MAX_MSG_LEN = sam::MAX_MSG_LEN - 100;
//   if ( headlen <= practical_MAX_MSG_LEN)
//   {
//     // We have a small message, ie, less than MAX_MSG_LEN, so we will encode
//     // using the legacy SAM0100 format.

//     // SAM0100 always reserves 5 bytes for the msglen section
//     msglen  += 5;
//     headlen += 5;

//     // TODO: need to check here the overall message size
//     // TODO: check the headlen is within the reserve len

//     m_msg_start -= headlen;

//     // build the header
//     char * p = m_msg_start;

//     *p = sam::MSG_START; p++;
//     memcpy(p, sam::SAM0100, 7); p += 7;
//     *p = sam::META_DELIM; p++;

//     char lenbuf[6] = {'0','0','0','0','0','\0'}; // strlen=5
//     snprintf(lenbuf, sizeof(lenbuf), "%05zu", msglen);
//     ::memcpy(p, lenbuf, sizeof(lenbuf)-1);
//   }
//   else
//   {
//     // TODO: throw unsupported SAM protocol
//   }

// }

//----------------------------------------------------------------------
/* Constructor */
DynamicSamBuffer::DynamicSamBuffer(size_t reserve)
  : m_msg_start(RESERVE_FOR_SAM_HEADER),
    m_wptr(RESERVE_FOR_SAM_HEADER)
{
  if (reserve < RESERVE_FOR_SAM_HEADER) reserve += RESERVE_FOR_SAM_HEADER;
  m_buf.resize(reserve, '\0');
}
//----------------------------------------------------------------------
/* Destructor */
DynamicSamBuffer::~DynamicSamBuffer()
{
}
//----------------------------------------------------------------------
void DynamicSamBuffer::check_space(size_t n)
{
  // ensure current buffer has enough size
  const size_t remain = m_buf.size() - m_wptr;

  if (n > remain)
  {
    size_t newsize = std::max(m_buf.size() + n - remain, 2*m_buf.size()) ;

    if (newsize < m_buf.size())
    {
      // oveflow?
      throw std::runtime_error("failed to resize encoding buffer");
    }

    //std::cout << "resizing from " <<  m_buf.size() << " to " << newsize << "\n";

    // If the resize fails, std::bad_alloc is thrown and will be caught in
    // higher levels
    m_buf.resize( newsize, '\0' );
  }
}
//----------------------------------------------------------------------
void DynamicSamBuffer::append(const char* src, size_t srclen)
{
  check_space(srclen);

  char* dest = &(m_buf[ m_wptr ]);
  memcpy(dest, src, srclen);
  m_wptr += srclen;
}
//----------------------------------------------------------------------
void DynamicSamBuffer::append(char src)
{
  check_space(1);
  m_buf[ m_wptr++ ] = src;
}

//----------------------------------------------------------------------
size_t DynamicSamBuffer::msg_size() const
{
  return m_wptr - m_msg_start;
}
//----------------------------------------------------------------------
const char* DynamicSamBuffer::msg_start() const
{
  return &(m_buf[m_msg_start]);
}

//----------------------------------------------------------------------

size_t count_digits(unsigned int i)
{
  size_t count = 0;

  do
  {
    count++;
    i = i / 10;
  } while (i);

  return count;
}
//----------------------------------------------------------------------
void DynamicSamBuffer::encode_header()
{
  // calculate the current know message length, which is only a partial count
  // because we don't yet know how many bytes are needed to represent the
  // msglen string.

  int headlen = 1 + 7 + 1; // MSG_START + SAM_HDR + META_DELIM
  size_t partlen = this->msg_size() + headlen;

  size_t msglen_strlen = count_digits(partlen);
  size_t msglen_full   = partlen + msglen_strlen;

  // check: there are cases where the calculated msglen_full exceeds the size
  // we have alloted for the msglen_string
  if (count_digits(msglen_full) > msglen_strlen)
  {
    msglen_strlen++;
    msglen_full++;
  }
  headlen += msglen_strlen;

  // TODO: need to check we have reserved enough space
  m_msg_start -= headlen;


  // ----- build the header -----
  char * p = &(m_buf[m_msg_start]);
  char * const p_orig = p;

  *p = sam::MSG_START; p++;
  memcpy(p, sam::SAM0101, 7); p += 7;
  *p = sam::META_DELIM; p++;

  char lenbuf[RESERVE_FOR_SAM_HEADER];
  memset(lenbuf, 0, sizeof(lenbuf));
#ifdef __LP64__
  snprintf(lenbuf, sizeof(lenbuf)-1, "%lu", msglen_full);
#else
  snprintf(lenbuf, sizeof(lenbuf)-1, "%u", msglen_full);
#endif
  memcpy(p, lenbuf, msglen_strlen);

  // TODO: some sanity checks
  size_t acutal_strlen = strlen(lenbuf);
  if ( acutal_strlen != msglen_strlen)
  {
    // TODO: add error logs
    throw std::runtime_error("SAM0101 encoding failed; msglen_string not of expected length");
  }
  p += acutal_strlen;

  if ( (p - p_orig) != headlen)
  {
    // TODO: add error logs
    throw std::runtime_error("SAM0101 encoding failed; generated headlen is not of expected size");
  }

  // change to SAM0100 for msgs of max length 99999
  if (msglen_full <= sam::MAX_MSG_LEN and acutal_strlen <= 5)
  {
    memcpy(p_orig+1, sam::SAM0100, 7);;
  }

  // std::cout << "headlen=" << headlen << "\n";
  // std::cout << "msglen_strlen=" << msglen_strlen << "\n";
  // std::cout << "msglen_full=" << msglen_full << "\n";
  // std::cout << "lenbuf=" <<lenbuf  << "\n";
}



} // namespace exio
