#include "exio/TableEvents.h"
#include "exio/TableSerialiser.h"


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
} // namespace qm
