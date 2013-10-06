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

#ifndef EXIO_SAMBUFFER_H
#define EXIO_SAMBUFFER_H

#include "exio/sam.h"

#include <vector>

#include <unistd.h>

namespace exio {

/**
 * Base class for message buffer encoding classes.
 *
 * Raw data should be added to the buffer via the append(...) methods.
 * Finally, the encode_header should be called.  The raw data -- pointer and
 * message size -- can then be fetched using the msg_start() and msg_size()
 * methods.
 */
class SamBuffer
{
  public:
    virtual ~SamBuffer() {}

    // add data
    virtual void append(const char* src, size_t srclen) = 0;
    virtual void append(char src) = 0;

    // encode header (once all data has been added)
    virtual void encode_header() = 0;

    // obtain location and size of data
    virtual const char* msg_start() const = 0;
    virtual size_t      msg_size()  const = 0;

};

//======================================================================
/**
 * Fixed message memory buffer, for encoding SAM messages. This is the legacy
 * approach, for limited size SAM0100 messages.
 */
// class FixedSamBuffer : public SamBuffer
// {
//   public:
//     FixedSamBuffer();
//     FixedSamBuffer(const FixedSamBuffer&);
//     FixedSamBuffer& operator=(const FixedSamBuffer&);

//     virtual void   append(const char* src, size_t srclen);
//     virtual void   append(char src);

//     virtual size_t msg_size() const;  // current msglen
//     virtual const char*  msg_start() const;

//     void check_space(size_t n);

//     virtual void encode_header();

//   private:

//     char   m_buf[sam::MAX_MSG_LEN];
//     char * m_msg_start;
//     char * m_write_ptr;
//     char * m_end;
// };
//======================================================================

/**
 * Message buffer which will grow as needed, to encode a SAM message.  The
 * initial reserved space should be high, so that in most cases, we don't need
 * to reallocate memory.
 */
class DynamicSamBuffer : public SamBuffer
{
  public:
    DynamicSamBuffer(size_t reserve = 512);
    ~DynamicSamBuffer();

    void append(const char* src, size_t srclen);
    void append(char src);

    virtual size_t msg_size() const;  // current msglen
    virtual const char*  msg_start() const;

    void check_space(size_t n);

    virtual void encode_header();

  private:

    std::vector<char> m_buf;

    size_t m_msg_start; // msg start pointer
    size_t m_wptr;      // write pointer
};

} // namespace exio

#endif
