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
#ifndef EXIO_TABLEEVENTS_H
#define EXIO_TABLEEVENTS_H

#include <string>
#include <map>
#include <list>

#include "exio/sam.h"



// TODO: introduce a namepspace like   event{ }
namespace sam
{
  class txMessage;
}

namespace exio {

class TableEventSerialiser;

// TODO: need to refactor here.  Bascially the Table event classes should only
// convery information about the event; they shsould not provide methods to
// serialise to a message etc. Eventaully I want to have better event
// handling, so that the client can receive table events; just like the
// session kind of already do.
struct TableEvent
{

    enum Type
    {
      eNoEvent = 0,
      eColAdded,
      eColRemoved,
      eRowAdded,
      eRowRemoved,
      eRowUpdate,
      eRowMultiUpdate,
      eTableAdded,
      eTableCleared,
      ePCMD
    } type;

    std::string table_name;

    TableEvent(Type __type,
               const std::string& __table_name)
      : type(__type),
        table_name(__table_name) {}

    virtual ~TableEvent() {}

};

//----------------------------------------------------------------------

// TODO: use C++ shared pointer instead?

struct TableEventPtr   // when in C++11, replace with shared_pointer
{
    TableEventPtr(TableEvent * __ev)
      : ev(__ev)
    {
    }

    TableEventPtr(const TableEventPtr& rhs)
      : ev( rhs.ev )
    {
      rhs.ev = 0;
    }

    ~TableEventPtr()
    {
      delete ev;
      ev = 0;
    }

    mutable TableEvent * ev;

  private:
    TableEventPtr& operator=(const TableEventPtr&);
};


//----------------------------------------------------------------------
struct TableAdded : public TableEvent
{
    TableAdded(const std::string& __table_name)
      : TableEvent(eTableAdded, __table_name)
    {
    }
};

//----------------------------------------------------------------------
struct TableCleared : public TableEvent
{
    TableCleared(const std::string& __table_name)
      : TableEvent(eTableCleared, __table_name)
    {
    }

    void serialise(std::list<sam::txMessage>&);
};

//----------------------------------------------------------------------
struct NewRow : public TableEvent
{
    std::string row_key;

    NewRow(const std::string& __table_name,
           const std::string& __row_key)
      : TableEvent(eRowAdded, __table_name),
        row_key( __row_key)
    {
    }
};

//----------------------------------------------------------------------
struct RowRemoved : public TableEvent
{
    std::string rowkey;

    RowRemoved(const std::string& __table_name,
               const std::string& __row_key)
      : TableEvent(eRowRemoved, __table_name),
        rowkey( __row_key)
    {
    }

    // TODO: not sure if serialisation methods should appear here.  Because,
    // idea is to have simple classes which represent events.
    void serialise(std::list<sam::txMessage>&);
};


//----------------------------------------------------------------------
struct NewColumn : public TableEvent
{
    std::string column;

    NewColumn(const std::string& __table_name,
              const std::string& __column)
      : TableEvent(eColAdded, __table_name),
        column( __column)
    {
    }
};

//----------------------------------------------------------------------
struct PCMDEvent : public TableEvent
{
    std::string row_key;
    std::string column;
    sam::txContainer meta;

    PCMDEvent(const std::string& __table_name,
              const std::string& __row_key,
              const std::string& __column,
              const sam::txContainer& __meta)
      : TableEvent(ePCMD, __table_name),
        row_key(__row_key),
        column(__column),
        meta(__meta)
    {
    }

    // TODO: not sure if serialisation methods should appear here.  Because,
    // idea is to have simple classes which represent events.
    void serialise(std::list<sam::txMessage>&);
};

//----------------------------------------------------------------------
// struct RowUpdate : public TableEvent
// {
//     std::string row_key;
//     std::string column;
//     std::string new_value;

//     RowUpdate(const std::string& __table_name,
//               const std::string& __row_key,
//               const std::string& __column,
//               const std::string& __new_value)
//       : TableEvent(eRowUpdate, __table_name),
//         row_key( __row_key),
//         column( __column),
//         new_value(__new_value)
//     {
//     }

//     void serialise(std::list<sam::txMessage>&);

// };

//----------------------------------------------------------------------
struct RowMultiUpdate : public TableEvent
{
    std::string row_key;
    std::map<std::string, std::string> fields;

    RowMultiUpdate(const std::string& __table_name,
                   const std::string& __row_key)
      : TableEvent(eRowMultiUpdate, __table_name),
        row_key( __row_key)
    {
    }

    void serialise(std::list<sam::txMessage>&);
};


} // namespace qm

#endif
