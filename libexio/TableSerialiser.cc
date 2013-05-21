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

} // namespace

