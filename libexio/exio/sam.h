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
#ifndef __SAM_HXX_
#define __SAM_HXX_

#include <stdexcept>
#include <list>
#include <ostream>
#include <vector>
#include <map>

// Macro for defining a qname (a Qualified Name, for describing the path to a
// field in a txContainer message).

//#define QNAME( ... )  sam::qname( (std::string[]){  __VA_ARGS__ } )  <-- modern C++
#define QNAME(...) (sam::qname_builder::create(), __VA_ARGS__)

// Macro for expanding the std::string array
#define QNAMEPARTS( ... )  (std::string[]){  __VA_ARGS__ }


namespace exio
{
  class AppSvc;
  class SamBuffer;
}

namespace sam
{
  static const char * const SAM0100 = "SAM0100";  // len 7
  static const char * const SAM0101 = "SAM0101";  // len 7
  static const char   MSG_START     = '{' ;
  static const char   MSG_END       = '}';
  static const char   MSG_DELIM     = '\n';
  static const char   SEQ_START     = '[';
  static const char   SEQ_END       = ']';
  static const char   FIELD_DELIM   = ',';
  static const char   VALUE_DELIM   = '=';
  static const char   META_DELIM    = ':';
  static const char   ESCAPE        =  '\\';

  static const char * const SPECIAL_CHARS =  "{}\n[],=:\\";
  static const char * const SAM_MAJOR_VER = "SAM01"; // always len 5

  static const size_t MAX_MSG_LEN   = 99999;  // for SAM0100, 5 digits

  // TODO: remove from sam.h
  struct Converters
  {
    static const char* fromBool(bool b)
    {
      return (b)? "true" : "false";
    }

    static bool toBool(const std::string& b)
    {
      return (b == "true") or (b == "TRUE");
    }
  };


//----------------------------------------------------------------------


class qname;
class txContainer;
class txField;

/*
  Implementation note: have removed the m_owner field, because (1) it was
  complicating the copy constructor and assignment operator, and (2) there was
  no current use for it.

*/

class txItem
{
  public:

    txItem() {}

    txItem(const std::string& name) : m_name(name) {}
    txItem(const txItem& rhs) : m_name(rhs.name()) {}

    virtual ~txItem() {}

    txItem& operator=(const txItem&rhs){m_name=rhs.name(); return *this;}

    const std::string & name() const    { return m_name; }
    void name(const std::string & name) { m_name = name; }

    virtual bool isContainer() const = 0;
    virtual txItem* clone() const = 0;

    virtual const txContainer* asContainer() const { return NULL; }
    virtual const txField*     asField()     const { return NULL; }

  protected:
    std::string m_name;
};



class txField : public txItem
{
  public:
    txField() {}

    txField(const std::string& name)
      : txItem(name)
    {
    }

    txField(const std::string& name, const std::string& value)
      : txItem(name),
        m_value(value)
    {
    }

    txField(const txField& f)
      : txItem(f.name() ),
        m_value( f.value() )
    {
    }

    txField& operator=(const txField& rhs)
    {
      this->m_name  = rhs.m_name;
      this->m_value = rhs.m_value;
      return *this;
    }

    const std::string& value() const { return m_value; }

    void value(const std::string& newvalue)
    {
      m_value = newvalue;
    }

    void operator=(const std::string& newvalue)
    {
      m_value = newvalue;
    }

    bool isContainer() const { return false; }

    virtual txField* clone() const
    {
      return new txField(m_name, m_value );
    }

    virtual const txField*     asField() const { return this; }

  private:

    std::string m_value;
};


/* Keep things simpler by using map instead of multimap */
typedef std::list< txItem* >                 ItemList;
//typedef std::map<std::string, txItem*>       ItemMap;
typedef std::map<std::string, txField*>      FieldMap;
typedef std::map<std::string, txContainer*>  ChildMap;

/**
 * Container of data objects.  A container contains two sets of data: a
 * collection of simple fields; and a collection of child containers.  The
 * formers are accessed via the various _field methods, the latter via the
 * various _child methods.  Because of this separation between fields and
 * children, an individual field-name can coexist as both a Field and a
 * Container at the same time.
 *
 * In addition to storing a collection of Field objects and a collection of
 * Container objects, the order in which objects have been added is stored,
 * and is reflected several operations, eg, copy, tostream etc.
 *
 *
 * Implemention notes
 * ~~~~~~~~~~~~~~~~~~
 *
 * Currently the txContainer inherits from txItem.  This choice was motivated
 * for two reasons: (1) to share implementation details; (2) to treat fields &
 * containers in a generic manner.
 *
 * For (1), the really only thing shared is the std::string htat holds the
 * name of the field/container.  So not much shared there, and not really a
 * reason to keep txItem.
 *
 * For (2), this is used in places where we have to iterate over m_items. Eg
 * when we delete all items, or if we are performing a copy, or a tostream
 * etc. It is also used to maintain the ordering information, and so the
 * m_items, which is the order data, contains pointers to the actual data.
 *
 */
class txContainer : public txItem
{
  public:

    /* Constuction, assignment, clone */

    txContainer();
    explicit txContainer(const std::string& name);
    txContainer(const txContainer&);
    virtual txContainer* clone() const;
    txContainer & operator=(const txContainer &);
    void merge(const txContainer &);


    ~txContainer();

    /* Operations on txItems */

    size_t count_any()    const;
    size_t count_field()  const;
    size_t count_child()  const;

    size_t size() const;
    bool   empty() const;

    const ItemList & items() const;

    /* ----- Operations on member Fields ----- */

    // add/get - if a field already exists, its value is updated, otherwise a
    //           new field is created. The affected field is returned.
    txField& put_field(const std::string& name, const std::string& value="");
    txField& put_field(const txField&);
    txField& put_field(const qname&, const std::string& value="");

    // search - return null if not found
    txField*       find_field(const std::string&);
    const txField* find_field(const std::string&) const;
    txField*       find_field(const qname&);
    const txField* find_field(const qname&) const;

    // utility function to check a field exists and has specific value
    bool check_field(const std::string& field, const std::string& value) const;
    bool check_field(const qname& field, const std::string& value) const;

    // iterators
    FieldMap::iterator field_begin() { return m_field_map.begin(); }
    FieldMap::iterator field_end()   { return m_field_map.end(); }

    FieldMap::const_iterator field_begin() const { return m_field_map.begin(); }
    FieldMap::const_iterator field_end() const { return m_field_map.end(); }


    /* ----- Operations on member Containers ----- */

    // insert new child container if not found
    txContainer& put_child(const std::string&);

    // insert a copy, overwriting any if already exists
    txContainer& put_child(const txContainer&);

    // nested addition
    txContainer& put_child(const qname&); // find existing, else new

    // search - return null if not exists
    txContainer*       find_child(const std::string&);
    const txContainer* find_child(const std::string&) const;
    txContainer*       find_child(const qname&);
    const txContainer* find_child(const qname&) const;


    // iterators
    ChildMap::iterator child_begin()  { return m_child_map.begin(); }
    ChildMap::iterator child_end()    { return m_child_map.end(); }

    ChildMap::const_iterator child_begin() const { return m_child_map.begin(); }
    ChildMap::const_iterator child_end() const { return m_child_map.end(); }

    /* Other */

    bool isContainer() const { return true; }
    virtual const txContainer* asContainer() const { return this; }

    // Remove all child elements and fields
    void clear();

    // Remove single item
    // TODO: this should be changed to: remove_field() and remove_child().
    void remove(const std::string&);

  private:

    void add_internally(txItem * f);

    // Preserve the order in which each Item was added
    ItemList m_items;

//    ItemMap   m_item_map;
    FieldMap  m_field_map;
    ChildMap  m_child_map;
};



class txMessage
{
  public:

    txMessage() {}
    explicit txMessage(const std::string& type);

    txContainer& root()             { return m_root; }
    const txContainer& root() const { return m_root; }

    const std::string& type() const { return m_root.name(); }
    void type(const std::string& __name )   {  m_root.name( __name ); }

    void reset();

    bool empty() const { return m_root.empty(); }

  private:
    txContainer m_root;
};

//----------------------------------------------------------------------

struct qname_builder
{
    qname_builder& operator,(const std::string&s)
    {
      items.push_back(s);
      return *this;
    }

    static qname_builder create()
    {
      return qname_builder();
    }

    // qname_builder& operator<<(const char* s)
    // {
    //   items.push_back(s);
    //   return *this;
    // }

    // qname_builder& operator<<(const std::string& s)
    // {
    //   items.push_back(s);
    //   return *this;
    // }

    std::vector<std::string> items;
};

//----------------------------------------------------------------------


class qname
{
  public:
    qname()
    {
      /* TODO: decide: if we are gonig to insert an "" into vec to represent
       * an empty qname, then check how this will work with the descenduing
       * routines in the sam.cc fiel, and, who it will effect the push
       * methods.  eg, we don't want to push onto a list that had the first
       * item as ", otherwise we will end up with something like
       * [""]["head"]["body"]["reqstate"] etc */
    }

    qname(const std::vector<std::string>& __v) : m_vec(__v){}

    qname(const qname_builder& qb) : m_vec(qb.items){}

    /*
     * Example constuction:
     *
     *  sam::qname q( (std::string[]){"head", "body", "data_count", } );
     *
     *  sam::qname q( QNAME("head", "body", "field") );      // copy cnstrctr
     *  sam::qname q( QNAMEPARTS("head", "body", "field") ); // better
     */
    template< int N >
    qname(const std::string (&__vec)[N] )
    {
      m_vec.reserve(N);
      for (size_t i = 0; i < N; ++i) m_vec.push_back( __vec[i] );
    }

    /* Parse a string in the form "head.body.data.field_count" to create a
     * qname object that describes the field and it nested location. */
    static qname parse(const char * src);

    // read
    size_t size() const { return m_vec.size(); }
    bool   empty() const { return m_vec.empty(); }
    const std::string& operator[](size_t i) const { return m_vec[i]; }

    // add new parts
    void push(const std::string& p) { m_vec.push_back(p); }
    qname& operator<<(const std::string& p) { m_vec.push_back(p); return *this; }

    // to string utility, but, we also support << operator
    std::string to_string() const;

    friend std::ostream& operator<<(std::ostream&, const qname&);

  private:
    std::vector< std::string > m_vec;
};
std::ostream& operator<<(std::ostream&, const qname&);


//----------------------------------------------------------------------

class MessageFormatter
{
  public:
    explicit MessageFormatter(bool b = false)
      : m_inline(b)
    {
      m_depth = 0;
    }

    void set_inline(int b) { m_inline = b; }

    void format(const txMessage&, std::ostream&);

    void format_contents(const txContainer&, std::ostream&);

  private:

    void indent_incr( std::ostream& );
    void indent_decr( std::ostream& );
    void newline( std::ostream& );
    void indent( std::ostream& );

    int m_depth;
    std::string m_indent_str;
    bool m_inline;
};

//----------------------------------------------------------------------

class OverFlow : public std::exception
{
  public:

    enum ReasonCode
    {
      eUnknown = 0,
      eEncodingBufferTooSmall,
      eExceedsSamLimit
    } reason;

    OverFlow(ReasonCode rc): reason(rc) {}

    const char* what() const throw()
    {
      switch (reason)
      {
        case eEncodingBufferTooSmall : return "buffer too small";
        case eExceedsSamLimit : return "exceeds max message size";
        default: return "unknown";
      }
    }
};

//----------------------------------------------------------------------


class SAMProtocol
{
  public:

    SAMProtocol(exio::AppSvc& appsvc); // TODO: should only need logsvc

    /* Attempt to encode the message into the buffer provided. The buffer will
     * expand as required. */
    size_t encodeMsg(const txMessage& msg, exio::SamBuffer* sbuf);

    /* Attempt to encode the message into the buffer provided. If there is not
     * enought room in the buffer, or the message size exceeds the SAM
     * limit, an OverFlow exception is thrown. */
    // size_t encodeMsg(const txMessage& msg,
    //                  char* const dest,
    //                  size_t len);

    size_t calc_encoded_size(const txMessage& msg);

    /* return true if message can encode, else false */
    bool check_enc_size(const txMessage& msg,
                           size_t margin);
/*
{SAM0100:00000xrequest:[head=[command=,],body=[args_COUNT=0,],]}#
{SAM0100:11:x:}#
01234567890123456
    {SAM0100:00065:
    01234567891
    size_t requred_header_bytes() const { return

*/
    /**
     * Returns the number of bytes that were consumed.
     *
     * start - points to first data byte
     *
     * end - points last data byte + 1
     *
     * will throw runtime_error if there is a decoding problem
     */
    size_t decodeMsg(txMessage& msg, const char* start, size_t len);

    static bool isSpecialChar(char c);

  private:
    const char* decode(txContainer* parent, const char* p, const char* end);
    void fail(const char* error);

    static void write_str(exio::SamBuffer*, const std::string & str);
    static void write_str_calc(const std::string str, size_t & n);


    static void write_noescape(exio::SamBuffer*,
                               const char* src, size_t srclen);
    static void write_noescape_calc(const char* src, size_t srclen, size_t& n);


    static void write_noescape(exio::SamBuffer*, char c);
    static void write_noescape_calc(char c, size_t& n);

    static void check_space(const char* beg, const char* end, size_t len)
    {
      if (len >= (size_t)(end-beg))
        throw OverFlow(OverFlow::eEncodingBufferTooSmall);
    }

    void encode_contents(exio::SamBuffer*, const txContainer* msg);
    void encode_contents_calc(const txContainer* msg, size_t& n);

    exio::AppSvc& m_appsvc;
};

// inline bool txContainer::has_any(const std::string& f)   const
// {
//   return m_item_map.find(f) != m_item_map.end();
// }

// inline bool txContainer::has_field(const std::string& f) const
// {
//   return m_field_map.find(f) != m_field_map.end();
// }

// inline bool txContainer::has_child(const std::string& f) const
// {
//   return m_child_map.find(f) != m_child_map.end();
// }


inline size_t txContainer::count_any()    const
{
  return m_items.size();
}

inline size_t txContainer::count_field()  const
{
  return m_field_map.size();
}

inline size_t txContainer::count_child()  const
{
  return m_child_map.size();
}

inline size_t txContainer::size() const
{
  return m_items.size ();
}

inline bool txContainer::empty() const
{
  return m_items.empty();
}

inline const ItemList & txContainer::items() const
{
  return m_items;
}


} // namepsace sam



#endif
