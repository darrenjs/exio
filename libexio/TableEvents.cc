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
#include "exio/TableEvents.h"
#include "exio/TableSerialiser.h"
#include "exio/MsgIDs.h"


namespace exio {


//----------------------------------------------------------------------
void TableCleared::serialise(std::list<sam::txMessage>& msglist)
{
  msglist.push_back( sam::txMessage() );

  // serialise into back() message
  TableClearSerialise serialiser;
  serialiser.init_msg(msglist.back(), table_name);
}

//----------------------------------------------------------------------
// void RowUpdate::serialise(std::list<sam::txMessage>& msglist)
// {
//   msglist.push_back( sam::txMessage() );

//   // Prepare the row-update serialiser. We serialise into back() message.
//   UpdateSerialiser serialiser;
//   serialiser.init_msg(msglist.back(), table_name, row_key);

//   serialiser.add_update(column, new_value);
// }

//----------------------------------------------------------------------
void RowMultiUpdate::serialise(std::list<sam::txMessage>& msglist)
{
  msglist.push_back( sam::txMessage() );

  // Prepare the row-update serialiser. We serialise into back() message.
  UpdateSerialiser serialiser;
  serialiser.init_msg(msglist.back(), table_name, row_key);

  for (std::map<std::string, std::string>::const_iterator it = fields.begin();
       it != fields.end(); ++it)
  {
    if ( serialiser.add_update( it->first, it->second) == false)
    {
      /* We failed to add a field to a txMessage, due to capacity problems. So
       * here we will try creating an empty txMessage, and see if we can add
       * our field there. */

      // TODO: test this splitting logic!

      // TODO: review this approach of splitting messages.  I'm note sure if
      // it should occur at this layer.

       // We need to create a new message & init the serialiser
       msglist.push_back( sam::txMessage() );

       serialiser.init_msg(msglist.back(), table_name, row_key);
       if ( serialiser.add_update( it->first, it->second) == false)
       {
         // Hard error.  Even using a new txMessage, we seem unable to
         // serialise this field.
         throw std::runtime_error("Field update for row is too large");
       }
     }
  }
}

//----------------------------------------------------------------------
void RowRemoved::serialise(std::list<sam::txMessage>& msglist)
{
  msglist.push_back( sam::txMessage() );

  // The message representation of a RowRemoved event is quite simple, so we
  // can implement it directly here.
  sam::txMessage& msg = msglist.back();

  msg.type( id::tablerowdel );
  msg.root().put_field( id::QN_msgtype,   id::tablerowdel);
  msg.root().put_field( id::QN_tablename, table_name);
  msg.root().put_field( QNAME( id::head, id::row_key) );
}

//----------------------------------------------------------------------
void PCMDEvent::serialise(std::list<sam::txMessage>& msglist)
{
  static std::string row_0 = id::row_prefix + "0";

  msglist.push_back( sam::txMessage() );

  sam::txMessage& msg = msglist.back();

  msg.type( id::tableupdate );
  msg.root().put_field( id::QN_tablename, table_name);
  msg.root().put_field( id::QN_msgtype,   id::tableupdate);

  sam::txContainer& body = msg.root().put_child( id::body );

  // insert a single row
  sam::txContainer& row = body.put_child( row_0 );
  row.put_field(id::row_key, row_key);
  body.put_field(id::rows, "1");

  // obtain a container for the meta
  sam::txContainer& __meta = row.put_child( meta );
  std::string meta_fieldname = ".meta." + column;
  __meta.name( meta_fieldname );
}

//----------------------------------------------------------------------
} // namespace qm
