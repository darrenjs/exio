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
#include "exio/sam.h"
#include "exio/Logger.h"
#include "exio/utils.h"
#include "exio/SamBuffer.h"
#include "exio/AppSvc.h"

#include <string.h>
#include <stdio.h>
#include <sstream>
#include <math.h>




//----------------------------------------------------------------------
namespace /* unnamed namespace for internal linkage*/
{

  /* Descend a tree according to a qname, updating the container and string
   * paratemers to reflect where the final item can be located. */
  void find_tree_const(const sam::txContainer* & current,
                       std::string  & name,
                       const sam::qname & qn)
  {
    // scan tree - using empty because we use the -1 on an unsigned
    if (not qn.empty())
    {
      for (size_t i = 0; i < ( qn.size() - 1 ); ++i)
      {
        current = current->find_child( qn[i] );
        if (current == NULL) return ;
      }

      name = qn[ qn.size() - 1 ];
    }
  }

  void buid_tree(sam::txContainer* & current,
                 std::string  & name,
                 const sam::qname & qn)
  {
    // build tree - using empty because we use the -1 on an unsigned
    if (not qn.empty())
    {
      for (size_t i = 0; i < ( qn.size() - 1 ); ++i) {
        current = &( current->put_child(qn[i]) );
      }

      name = qn[ qn.size() - 1 ];
    }
  }

} // namespace

namespace sam
{


//======================================================================
ParseError::ParseError(const std::string & error,
                       const std::list<std::string>& hist)
{
  std::ostringstream os;
  os << error;

  if ( hist.size() )
  {
    os << ". Trace: ";
    for (std::list<std::string>::const_iterator i = hist.begin();
         i != hist.end(); )
    {
      os << *i;
      i++;
      if (i != hist.end()) os << ", ";
    }
  }

  m_msg = os.str();
}


ParseError::~ParseError() throw() {}

const char* ParseError::what() const throw()  { return m_msg.c_str(); }

//======================================================================


MsgError::MsgError(const std::string & error, const std::string& field)
  : std::runtime_error(error + ": " + field),
    m_error(error),
    m_field(field)
{
}

MsgError::~MsgError() throw() {}

const std::string& MsgError::field() const { return m_field; }
const std::string& MsgError::error() const { return m_error; }

//======================================================================

std::ostream& operator<<(std::ostream& os, const qname& q)
{
  for (std::vector< std::string >::const_iterator it = q.m_vec.begin();
       it != q.m_vec.end(); ++it)
  {
    if (it != q.m_vec.begin()) os << '.';
    os << *it;
  }
  return os;
}

std::string qname::to_string() const
{
  std::ostringstream os;
  os << *this;
  return os.str();
}

//----------------------------------------------------------------------
qname qname::parse(const char * src)
{
  /* Note. Prefer the const char approach so that a NULL pointer is passed
   * straight through to the tokenize, where it handled, instead of an
   * implicit conversion to an std::string which throws exception. */
  return qname( exio::utils::tokenize(src, '.', false) );
}

//======================================================================
SAMProtocol::SAMProtocol(exio::AppSvc& appsvc)
  : m_appsvc(appsvc)
{
}
//----------------------------------------------------------------------
void SAMProtocol::write_str(exio::SamBuffer* sb, const std::string & str)
{
  /* WARNING: any changes to the encoding must be reflected in the
   * corresponding _calc method. */

  /* Search for a special character.

     size_t strcspn(const char *s, const char *reject);

     The strcspn() function calculates the length of the initial segment of s
     which consists entirely of characters not in reject.
  */

  const char*  src    = str.c_str();
  size_t const len    = str.size();
  size_t const accept = strcspn(src, sam::SPECIAL_CHARS);

  if (accept == len)
  {
    // no special characters - yay!
    sb->append(src, len);
  }
  else
  {
    sb->append(src, accept);
    src  += accept;

    // now write out the region containing special chars, which has to be done
    // character by character
    for (size_t i = 0; i < (len-accept); ++i)
    {
      if ( SAMProtocol::isSpecialChar(*src) )
      {
        sb->append( sam::ESCAPE );
      }

      sb->append( *src++ );
    }
  }
}

//----------------------------------------------------------------------
void SAMProtocol::write_str_calc(const std::string str, size_t & n)
{
  /* Search for a special character.

     size_t strcspn(const char *s, const char *reject);

     The strcspn() function calculates the length of the initial segment of s
     which consists entirely of characters not in reject.
  */

  const char*  src    = str.c_str();
  size_t const len    = str.size();
  size_t const accept = strcspn(src, sam::SPECIAL_CHARS);

  if (accept == len)
  {
    // no special characters - yay!
    n += len;
  }
  else
  {
    n   += accept;  // simulate the write
    src += accept;

    // now write out the region containing special chars, which has to be done
    // character by character
    for (size_t i = 0; i < (len-accept); ++i)
    {
      if ( SAMProtocol::isSpecialChar(*src) )
      {
        ++n;
      }
      n++; src++;
    }
  }
}

//----------------------------------------------------------------------
void SAMProtocol::write_noescape(exio::SamBuffer* sb,
                                 const char* src, size_t srclen)
{
  /* WARNING: any changes to the encoding must be reflected in the
   * corresponding _calc method. */

  sb->append(src, srclen);
}

//----------------------------------------------------------------------
void SAMProtocol::write_noescape_calc(const char* src, size_t srclen,
                                      size_t& n)
{
  n += srclen;
}

//----------------------------------------------------------------------
void SAMProtocol::write_noescape(exio::SamBuffer* sb, char c)
{
  /* WARNING: any changes to the encoding must be reflected in the
   * corresponding _calc method. */

  sb->append( c );
}

//----------------------------------------------------------------------
void SAMProtocol::write_noescape_calc(char c, size_t& n)
{
  n++;
}
//----------------------------------------------------------------------
size_t SAMProtocol::encodeMsg(const txMessage& msg,
                              exio::SamBuffer* sb)
{
  /* WARNING: any changes to the encoding must be reflected in the
   * corresponding _calc method. */

  // start encoding from the message type
  SAMProtocol::write_noescape(sb, META_DELIM);
  SAMProtocol::write_noescape(sb, msg.type().c_str(), msg.type().length());
  SAMProtocol::write_noescape(sb, META_DELIM);

  encode_contents(sb, &( msg.root() ));

  SAMProtocol::write_noescape(sb, MSG_END);
  SAMProtocol::write_noescape(sb, MSG_DELIM);

//  *p = '\0';

  // write the remaining part of the Sam header
  sb->encode_header();

  size_t bytes = sb->msg_size();
  return bytes;


//--

 //  char lenbuf[6] = {'0','0','0','0','0','\0'}; // strlen=5!

//   SAMProtocol::write_noescape(sb, MSG_START);
//   SAMProtocol::write_noescape(sb, sam::SAM0100, 7);
//   SAMProtocol::write_noescape(sb, META_DELIM);

//   char* lenpos = sb.pos(); // remember where we have to write length
//   SAMProtocol::write_noescape(sb, lenbuf, sizeof(lenbuf)-1); // write later

//   SAMProtocol::write_noescape(sb, META_DELIM);
//   SAMProtocol::write_noescape(sb, msg.type().c_str(), msg.type().length());
//   SAMProtocol::write_noescape(sb, META_DELIM);

//   encode_contents(sb, &( msg.root() ));

//   SAMProtocol::write_noescape(sb, MSG_END);
//   SAMProtocol::write_noescape(sb, MSG_DELIM);

// //  *p = '\0';

//   size_t const bytes = sb.bytes();
//   if (bytes > MAX_MSG_LEN) throw OverFlow(OverFlow::eExceedsSamLimit);

//   // write the length
//   snprintf(lenbuf, sizeof(lenbuf), "%05zu", bytes);
//   memcpy(lenpos, lenbuf, sizeof(lenbuf)-1);

//   return bytes;
}

//----------------------------------------------------------------------
size_t SAMProtocol::calc_encoded_size(const txMessage& msg)
{
  size_t n = 0;

  char lenbuf[6] = {'0','0','0','0','0','\0'}; // strlen=5!

  SAMProtocol::write_noescape_calc(MSG_START, n);
  SAMProtocol::write_noescape_calc(sam::SAM0100, 7, n);
  SAMProtocol::write_noescape_calc(META_DELIM, n);

  SAMProtocol::write_noescape_calc(lenbuf, sizeof(lenbuf)-1, n);

  SAMProtocol::write_noescape_calc(META_DELIM, n);
  SAMProtocol::write_noescape_calc(msg.type().c_str(), msg.type().length(), n);
  SAMProtocol::write_noescape_calc(META_DELIM, n);

  encode_contents_calc( &( msg.root() ), n);

  SAMProtocol::write_noescape_calc(MSG_END, n);
  SAMProtocol::write_noescape_calc(MSG_DELIM , n);

  //n++;

  return n;
}
//----------------------------------------------------------------------
bool SAMProtocol::check_enc_size(const txMessage& msg, size_t margin)
{
  size_t encsz = calc_encoded_size(msg);

  return (encsz + margin <= MAX_MSG_LEN);
}
//----------------------------------------------------------------------
bool SAMProtocol::isSpecialChar(char c)
{
  return ( (c == MSG_START)
           || (c == MSG_END)
           || (c == MSG_DELIM)
           || (c == SEQ_START)
           || (c == SEQ_END)
           || (c == FIELD_DELIM)
           || (c == VALUE_DELIM)
           || (c == META_DELIM)
           || (c == ESCAPE)
    );
}

//----------------------------------------------------------------------
void SAMProtocol::encode_contents(exio::SamBuffer* sb, const txContainer* c)
{
  /* WARNING: any changes to the encoding must be reflected in the
   * corresponding _calc method. */

  SAMProtocol::write_noescape(sb, SEQ_START);
  for (ItemList::const_iterator iter = c -> items().begin();
       iter != c -> items().end(); ++iter)
  {
    SAMProtocol::write_noescape(sb,
                                (*iter)->name().c_str(),
                                (*iter)->name().length());
    SAMProtocol::write_noescape(sb, VALUE_DELIM);
    if ( const txContainer* child = (*iter)->asContainer() )
    {
      encode_contents(sb, child);
    }
    else if ( const txField* field = (*iter)->asField() )
    {
      SAMProtocol::write_str(sb, field->value());
    }
    SAMProtocol::write_noescape(sb, FIELD_DELIM);
  }

  SAMProtocol::write_noescape(sb, SEQ_END);
}

//----------------------------------------------------------------------
void SAMProtocol::encode_contents_calc(const txContainer* c, size_t& n)
{
  SAMProtocol::write_noescape_calc(SEQ_START, n);

  for (ItemList::const_iterator iter = c -> items().begin();
       iter != c -> items().end(); ++iter)
  {
    SAMProtocol::write_noescape_calc((*iter)->name().c_str(),
                                     (*iter)->name().length(),
                                     n);
    SAMProtocol::write_noescape_calc(VALUE_DELIM, n);
    if ( const txContainer* child = (*iter)->asContainer() )
    {
      encode_contents_calc(child, n);
    }
    else if ( const txField* field = (*iter)->asField() )
    {
      SAMProtocol::write_str_calc(field->value(), n);
    }
    SAMProtocol::write_noescape_calc(FIELD_DELIM, n);
  }

  SAMProtocol::write_noescape_calc(SEQ_END, n);
}

//----------------------------------------------------------------------
size_t SAMProtocol::decodeMsg(txMessage& msg,
                              int& msg_count,
                              const char* start,
                              const char* end)
{
  // Define max message len as approx 2G 2147483647.
  static double max_msglen = 2147483647;
  msg_count = 0;

  size_t consumed = 0;
  m_hist.clear();

  while (start < end)  // while data is avail
  {
    size_t bytesavail = end - start;

    /* Check we have enough bytes for the message length section. Note that
     * here we don't know how many bytes are used for the msg length. So we
     * only detect the SAM type. */
    size_t const head_len = 1 + 7 + 1 ; //   {SAM0100:
    if (bytesavail < head_len ) break;

    if ((*start != MSG_START) or (start[8] != META_DELIM))
      throw std::runtime_error("bad SAM header");

    bool sam_ver_supported = false;
    if ( memcmp(start+1,SAM0100,7) == 0)
    {
      sam_ver_supported = true;
    }
    else if ( memcmp(start+1,SAM0101,7) == 0 )
    {
      sam_ver_supported = true;
    }

    /* Example SAM headers
       ~~~~~~~~~~~~~~~~~~~

       {SAM0100:00065:request:[head=[command=,],body=[args_COUNT=0,],]}
       {SAM0100:111:x:}
       {SAM0101:2147483647:x:}
       01234567890123
    */


    /* Decode the message length, without making any assumption about how many
     * bytes are used for the msglen section. */

    const char* p = start + head_len;
    double msglen = 0;
    while (p < end and isdigit(*p) and *p != sam::META_DELIM)
    {
      msglen = (msglen * 10) + (*p bitand 0x0F);
      p++;

      // protect our application by assuming a max message size
      if (msglen > max_msglen)
      {
        throw std::runtime_error("SAM message exceeds maximum size");
      }
    }

    if (*p != sam::META_DELIM) break; // not enough data for header
    if (bytesavail < msglen)   break; // not enough data for body

    if (sam_ver_supported)
    {
      p++;  // skip META_DELIM

      /* ----- message name ----- */
      std::ostringstream os;
      while (p < end and *p != sam::META_DELIM)
      {
        if (*p == sam::ESCAPE) p++;
        if (p < end) os << *p;
        p++;
      }
      p++;  // skip META_DELIM

      msg.reset();
      msg.type(os.str());

      decode(&(msg.root()), p, p+(size_t)msglen);
      msg_count++;
    }
    else
    {
      std::string samheader(start+1, 7);
      _WARN_(m_appsvc.log(), "Unsupported SAM format, \"" << samheader << '"');
    }

    consumed += msglen;
    break;  // at the moment we only decode one message at a time
  }

  return consumed;
}


/**
 * Decode the raw bytes, storing data inside the container.  The
 * next byte to decode is at position 'p', and 'p' is valid if p <
 * end. The next byte should be SEQ_P.  Returns a pointer to the
 * next byte of unparsed data.
 */
const char* SAMProtocol::decode(txContainer* parent,
                                const char* p,
                                const char* end)
{
  p++;  // skip the [

  while (p < end)
  {
    if ( *p == sam::SEQ_END) return ++p; // this container done

    /* ----- decode fieldname ----- */

    std::ostringstream fieldname;
    while (p < end and *p != sam::VALUE_DELIM)
    {
      if (*p == sam::ESCAPE) p++;
      if (p < end) fieldname << *p;
      p++;
    }

    if (p >= end) fail("ran out of data, expected field-value-delim");
    if (*p != sam::VALUE_DELIM) fail("failed to find field-value-delim");

    p++; // skip the VALUE_DELIM

    m_hist.push_back(fieldname.str());

    /* ----- decode field-value ----- */

    if ( *p == SEQ_START)
    {
      // we have found a sub container
      txContainer& subcont = parent->put_child( fieldname.str() );
      p = decode(&subcont, p, end); // recursive
    }
    else
    {
      // simple value string
      std::ostringstream value;

      while (p < end && *p != sam::FIELD_DELIM)
      {
        if (*p == sam::ESCAPE) p++;
        if (p < end) value << *p;
        p++;
      }

      parent->put_field(fieldname.str(), value.str());
    }

    if (p >= end)
      fail("ran out of data when expecting field delim");

    if (*p != sam::FIELD_DELIM)
      fail("expected field delim");

    p++;
  }  // main processing loop

  return p;
}

/** Raise an exception to indicate a parsing exception. */
void SAMProtocol::fail(const char* error)
{
  throw ParseError(error, m_hist);
}

//======================================================================


//======================================================================


// Message::Message()
//   : m_root(NULL)
// {
// }

// Message::Message(const std::string& name)
//   : m_name(name),
//     m_root(NULL)
// {
// }

// void Message::reset(const std::string& name)
// {
//   m_name = name;
//   m_root.clear();
// }

// void Message::to_stream(std::ostream& os, bool _inline) const
// {
//   os << "Message:" << m_name << "={";
//   if (not _inline) os << '\n';
//   m_root.to_stream(os, _inline, 1);
//   os << "}\n";  // always terminate message with a newline
// }


//======================================================================


//----------------------------------------------------------------------

txField& txContainer::put_field(const std::string& fname,
                                const std::string& fvalue)
{
  txField* field = this->find_field(fname);

  if (field)
  {
    // field exists, so just update its value
    field -> value(fvalue);
    return *field;
  }
  else
  {
    // TODO: need a try/catch
    txField * f = new txField(fname, fvalue);
    add_internally( f );
    return *f;
  }
}

//----------------------------------------------------------------------
txField& txContainer::put_field(const txField& f)
{
  return put_field(f.name(), f.value());
}

//----------------------------------------------------------------------

txField& txContainer::put_field(const qname& qn,
                                const std::string& value)
{
  // Note: qname qn could be empty! In which case we are just searching for a
  // fieldname which is the empty string. We treat an empty qname this way
  // because if we add a field with an empty qname, it has to be given a valid
  // string name.

  txContainer * current = this;

  // default fieldname is the empty string (needed if the qname is empty)
  std::string name;

  buid_tree(current, name, qn); /* build_tree updates the args */

  // finally add/update
  return current->put_field(name, value);
}
//----------------------------------------------------------------------

txField* txContainer::find_field(const std::string& f)
{
  FieldMap::iterator iter = m_field_map.find( f );
  if (iter != m_field_map.end())
  {
    return iter->second;
  }
  else
  {
    return NULL;
  }
}

//----------------------------------------------------------------------

const txField* txContainer::find_field(const std::string& f) const
{
  FieldMap::const_iterator iter = m_field_map.find( f );
  if (iter != m_field_map.end())
  {
    return iter->second;
  }
  else
  {
    return NULL;
  }
}

//----------------------------------------------------------------------
void txContainer::add_internally(txItem * item)
{
  // TODO: surround with try/catch, and remove 'item' if partially added


  // TODO: before addition, we should make a final check that the field/child
  // name is not already used, in which case we need to delete existing.
  // Note, we MUST add the item pointer, because that pointer value might be
  // returned to the client.

  m_items.push_back( item );
//  m_item_map.insert( std::make_pair(item->name(), item) );

  if (item->isContainer())
  {
    txContainer* container = dynamic_cast<txContainer*>(item);
    m_child_map.insert( std::make_pair(item->name(), container) );
  }
  else
  {
    txField* field = dynamic_cast<txField*>(item);
    m_field_map.insert( std::make_pair(item->name(), field) );
  }
}

//----------------------------------------------------------------------

txContainer& txContainer::put_child(const std::string& f)
{
  txContainer* child = this->find_child( f );

  if (child)
  {
    return *(child);
  }
  else
  {
    // TODO: handle throws here -- might need to delete
    txContainer * c = new txContainer( f );
    add_internally( c );
    return *c;
  }
}
//----------------------------------------------------------------------
txContainer& txContainer::put_child(const txContainer& src)
{
  txContainer* child = this->find_child( src.name() );

  if (child)
  {
    *child = src;  // deep copy
    return *(child);
  }
  else
  {
    // TODO: handle throws here -- might need to delete
    txContainer * c = new txContainer( src ); // deep copy
    add_internally( c );
    return *c;
  }
}

//----------------------------------------------------------------------

txContainer::~txContainer()
{
  for (ItemList::iterator i = m_items.begin(); i != m_items.end(); ++i)
  {
    delete *i;
  }
}
//----------------------------------------------------------------------

// copy constructor
txContainer::txContainer(const txContainer& rhs)
  : txItem( rhs.name() )
{
  // TODO: add try/catch here, just in case one of the copy fails. If we
  // catch, we should undo the operation.
  for (ItemList::const_iterator iter = rhs.items().begin();
       iter != rhs.items().end();
       iter++)
  {
    add_internally( (*iter)->clone() );
  }
}

//----------------------------------------------------------------------

txContainer & txContainer::operator=(const txContainer & rhs)
{
  // TODO: how would I actually use the SWAP idiom here?  I think I need to
  // have an impl class, which is just a struct that contains all of the raw
  // data.


  this->clear();

  txItem::operator=(rhs);

  // TODO: copy
  for (ItemList::const_iterator iter = rhs.m_items.begin();
       iter != rhs.m_items.end();
       iter++)
  {
    this -> add_internally( (*iter)->clone() );
  }

  return *this;
}

//----------------------------------------------------------------------

txContainer* txContainer::clone() const
{
  // TODO: add try/catch here, just in case one of the copy fails. If we
  // catch, we should undo the operation.
  txContainer* newc = new txContainer( m_name );

  for (ItemList::const_iterator iter = m_items.begin();
       iter != m_items.end();
       iter++)
  {
    newc->add_internally( (*iter)->clone() );
  }

  return newc;
}

//----------------------------------------------------------------------

txContainer& txContainer::put_child(const qname& qn)
{
  // Note: qname qn could be empty! In which case we are just searching for a
  // fieldname which is the empty string. We treat an empty qname this way
  // because if we add a field with an empty qname, it has to be given a valid
  // string name.

  txContainer * current = this;

  // default fieldname is the empty string (needed if the qname is empty)
  std::string name;

  buid_tree(current, name, qn); /* build_tree updates the args */

  return current->put_child( name );
}

//----------------------------------------------------------------------

txContainer* txContainer::find_child(const std::string& f)
{
  ChildMap::iterator iter = m_child_map.find( f );
  if (iter != m_child_map.end())
  {
    return iter->second;
  }
  else
  {
    return NULL;
  }
}

//----------------------------------------------------------------------

const txContainer* txContainer::find_child(const std::string& f) const
{
  ChildMap::const_iterator iter = m_child_map.find( f );
  if (iter != m_child_map.end())
  {
    return iter->second;
  }
  else
  {
    return NULL;
  }
}

//----------------------------------------------------------------------

void txContainer::clear()
{
  // tell sub containers to clear()
  for (ChildMap::iterator iter = m_child_map.begin();
       iter != m_child_map.end();
       ++iter)
  {
    iter->second->clear();
  }

  // now delete our items
  for (ItemList::iterator iter = m_items.begin();
       iter != m_items.end();
       ++iter)
  {
    delete *iter;
  }

  // now clear out our state
  m_items.clear();
//  m_item_map.clear();
  m_field_map.clear();
  m_child_map.clear();
}

//----------------------------------------------------------------------

txMessage::txMessage(const std::string& type)
  : m_root( type )
{
}

void txMessage::reset()
{
  m_root.clear();
  m_root.name("");
}

//----------------------------------------------------------------------

void MessageFormatter::newline( std::ostream& os)
{
  if (m_inline) return;
  os << "\n";
}

//----------------------------------------------------------------------

void MessageFormatter::indent( std::ostream& os)
{
  os << m_indent_str;
}

//----------------------------------------------------------------------

void MessageFormatter::indent_incr( std::ostream& )
{
  if (m_inline) return;
  m_depth++;
  m_indent_str =  std::string(2*m_depth, ' ');
}

//----------------------------------------------------------------------

void MessageFormatter::indent_decr( std::ostream& )
{
  if (m_inline) return;
  m_depth--;

  m_indent_str =  std::string(2*m_depth, ' ');
}
//----------------------------------------------------------------------
void MessageFormatter::format(const txMessage& msg, std::ostream& os)
{
  m_indent_str = std::string();
  os << msg.type() << ":";

  newline(os);
  os << "[";

  format_contents(msg.root(), os);

  newline(os);
  os << "]";
}

//----------------------------------------------------------------------

void MessageFormatter::format_contents(const txContainer& cont,
                                       std::ostream& os)
{
  indent_incr(os);
  for (ItemList::const_iterator iter = cont.items().begin();
       iter != cont.items().end();
       ++iter)
  {
    if (iter != cont.items().begin() ) os << ", ";
    newline(os);
    const txItem* item = *iter;
    if ( item->asContainer() )
    {
      os << m_indent_str;
      os << item->name();
      os << "=";

      newline(os); os << m_indent_str;
      os << "[";
      format_contents( *(item->asContainer()), os );
      newline(os); os << m_indent_str;
      os << "]";

    }
    else if ( item->asField() )
    {
      os << m_indent_str;
      os << item->name();
      os << "=";
      os << item->asField()->value();
    }
    else
    {
      os << m_indent_str;
      os << item->name();
      os << "=???";
    }
  }
  indent_decr(os);
}

//----------------------------------------------------------------------
txField* txContainer::find_field(const qname& qn)
{
  // Note: qname qn could be empty! In which case we are just searching for a
  // fieldname which is the empty string. We treat an empty qname this way
  // because if we add a field with an empty qname, it has to be given a valid
  // string name.

  // default fieldname is the empty string (needed if the qname is empty)
  std::string name;

  // start by looking at self
  const txContainer * const_current = this;

  find_tree_const(const_current, name, qn);  // updates current & name

  txContainer * current = const_cast<txContainer *>(const_current);

  return (current)? current->find_field( name ) : NULL;
}
//----------------------------------------------------------------------
const txField* txContainer::find_field(const qname& qn) const
{
  // Note: qname qn could be empty! In which case we are just searching for a
  // fieldname which is the empty string. We treat an empty qname this way
  // because if we add a field with an empty qname, it has to be given a valid
  // string name.

  // default fieldname is the empty string (needed if the qname is empty)
  std::string name;

  // start by looking at self
  const txContainer * current = this;

  find_tree_const(current, name, qn);  // updates current & name

  return (current)? current->find_field( name ) : NULL;
}
//----------------------------------------------------------------------
bool txContainer::check_field(const std::string& fieldname,
                              const std::string& val) const
{
  const txField * field = find_field( fieldname );
  return (field and field->value() == val);
}
//----------------------------------------------------------------------
bool txContainer::check_field(const qname& qn,
                              const std::string& val) const
{
  const txField * field = find_field( qn );
  return (field and field->value() == val);
}
//----------------------------------------------------------------------
txContainer* txContainer::find_child(const qname& qn)
{
  // Note: qname qn could be empty! In which case we are just searching for a
  // fieldname which is the empty string. We treat an empty qname this way
  // because if we add a field with an empty qname, it has to be given a valid
  // string name.

  // default fieldname is the empty string (needed if the qname is empty)
  std::string name;

  // start by looking at self
  const txContainer * const_current = this;

  find_tree_const(const_current, name, qn);  // updates current & name

  txContainer * current = const_cast<txContainer *>(const_current);
  return (current)? current->find_child( name ) : NULL;
}
//----------------------------------------------------------------------
const txContainer* txContainer::find_child(const qname& qn) const
{
  // Note: qname qn could be empty! In which case we are just searching for a
  // fieldname which is the empty string. We treat an empty qname this way
  // because if we add a field with an empty qname, it has to be given a valid
  // string name.

  // default fieldname is the empty string (needed if the qname is empty)
  std::string name;

  // start by looking at self
  const txContainer * current = this;

  find_tree_const(current, name, qn);  // updates current & name

  return (current)? current->find_child( name ) : NULL;
}

//----------------------------------------------------------------------
void txContainer::remove(const std::string& n)
{
  // TODO: once we are 100% certain a field cannot exist, at the same time, as
  // both a field and a child, then can return if the field-check is
  // successful. Update: we probably do want to allow field & child same name
  // coexistence.

  // TODO: instead of this method, probably want to have remove_child() and
  // remove_field() and remove_any()

  // look for a field
  {
    FieldMap::iterator f = m_field_map.find(n);
    if (f != m_field_map.end())
    {
      delete f->second;
      m_field_map.erase(f);
      m_items.remove(f->second);
    }
  }

  {
    // look for a container
    ChildMap::iterator c = m_child_map.find(n);
    if (c != m_child_map.end())
    {
      delete c->second;
      m_child_map.erase(c);
      m_items.remove(c->second);
    }
  }
}

//----------------------------------------------------------------------

void txContainer::merge(const txContainer& src)
{
  /* we loop over src.m_items, so that we add new items to this container in
   * the order which were found in src */
  for (ItemList::const_iterator i = src.m_items.begin();
       i != src.m_items.end(); ++i)
  {
    txContainer * c = find_child((*i)->name());
    //txField     * f = find_field((*i)->name());

    /*
      NOTE: if we add a container but a field already exists with that name,
      there is no need to remove the field because a field & child can
      co-exist with the same name.
    */
    if ((*i)->isContainer())
    {
      txContainer* src_container = dynamic_cast<txContainer*>(*i);

      if (c!=NULL)
      {
        c->merge( * src_container ); // merge child containers
      }
      else
      {
        put_child(*src_container); // copy
      }
    }
    else
    {
      txField* src_field = dynamic_cast<txField*>(*i);

      put_field( *src_field );
    }
  } // loop over items
}


} // namepsace sam

