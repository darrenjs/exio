#include "exio/TableSerialiser.h"
#include "exio/MsgIDs.h"

namespace exio {

//----------------------------------------------------------------------
TableEventSerialiser::~TableEventSerialiser()
{
}

//----------------------------------------------------------------------
void
TableEventSerialiser::set_table_name(const std::string& __table_name)
{
  m_table_name = __table_name;
}

//----------------------------------------------------------------------
UpdateSerialiser::UpdateSerialiser()
  : m_row( NULL )
{
}

//----------------------------------------------------------------------
void UpdateSerialiser::init_msg(sam::txMessage& m,
                                const std::string& table_name,
                                const std::string& row_key)
{
  m.type( id::tableupdate );
  m.root().put_field( id::QN_tablename, table_name);
  m.root().put_field( id::QN_msgtype,   id::tableupdate );

  sam::txContainer& body = m.root().put_child( id::body );

  body.put_field(id::rows, "1");

  // get handle to the container for the row data
  m_row = &( body.put_child( id::row_prefix + "0") );
  m_row->put_field(id::row_key, row_key);
}

//----------------------------------------------------------------------
bool UpdateSerialiser::add_update(const std::string& column,
                                  const std::string& value)
{
  // TODO: here need to plug in the details of the encoding protocol (which
  // may not always be SAM).  We need that protocol here so that we can test
  // the message size.
  m_row->put_field(column, value);
  return true;
}

//----------------------------------------------------------------------
void TableClearSerialise::init_msg(sam::txMessage& m,
                                   const std::string& table_name)
{
  m.type( id::tableclear );
  m.root().put_field( id::QN_msgtype, id::tableclear);
  m.root().put_field( id::QN_tablename, table_name);
}

//----------------------------------------------------------------------
void PCMDSerialiser::init_msg(sam::txMessage& m,
                              const std::string& table_name,
                              const std::string& row_key)
{
  m.type( id::tableupdate );
  m.root().put_field( id::QN_tablename, table_name);
  m.root().put_field( id::QN_msgtype,   id::tableupdate );

  sam::txContainer& body = m.root().put_child( id::body );

  body.put_field(id::rows, "1");

  // get handle to the container for the row data
  m_row = &( body.put_child( id::row_prefix + "0") );
  m_row->put_field(id::row_key, row_key);
}

//----------------------------------------------------------------------
bool PCMDSerialiser::add_update(const std::string& column,
                                const sam::txContainer& meta)
{
  // TODO: here need to plug in the details of the encoding protocol (which
  // may not always be SAM).  We need that protocol here so that we can test
  // the message size.


  std::string meta_fieldname = ".meta." + column;

  m_row->put_child(meta);
  return true;
}


} // namespace

