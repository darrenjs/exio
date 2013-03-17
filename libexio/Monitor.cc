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
#include "exio/Monitor.h"
#include "exio/Table.h"
#include "exio/AdminInterfaceImpl.h"
#include "exio/Logger.h"


#include <iostream>

namespace exio {

/* Constructor */
Monitor::Monitor(AdminInterfaceImpl * ai)
  : m_ai( ai )
{
  /* CAUTION: don't try to use the m_ai parameter in here, because that object
   * itself it likely to still be under initialisation. */
}

//----------------------------------------------------------------------

/* Destructor */
Monitor::~Monitor()
{
//  _INFO_(m_ai->appsvc().log(), "Monitor::~Monitor");
}


//----------------------------------------------------------------------

void Monitor::unsubscribe_all(const SID& __id)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

  for (TableCollection::const_iterator it = m_tables.begin();
       it != m_tables.end(); ++it)
  {
    DataTable* table = it->second;

    table->del_subscriber( __id );
  }
}

//----------------------------------------------------------------------

void Monitor::subscribe_all(const SID& __id)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

  for (TableCollection::const_iterator it = m_tables.begin();
       it != m_tables.end(); ++it)
  {
    DataTable* table = it->second;

    try
    {
      table->add_subscriber( __id );
    }
    catch (std::exception& e)
    {
      _WARN_(m_ai->appsvc().log(),
             "problem when adding session " << __id
             << " to table " << table->table_name() << ": " << e.what());
    }
  }
}
//----------------------------------------------------------------------
/* Obtain a list of monitoring tables */
std::list< std::string > Monitor::tables() const
{
  std::list< std::string > tables;

  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

  for (TableCollection::const_iterator it = m_tables.begin();
       it != m_tables.end(); ++it)
  {
    tables.push_back( it->first );
  }

  return tables;
}

//----------------------------------------------------------------------

bool Monitor::table_exists(const std::string& table_name) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

  return m_tables.find( table_name) != m_tables.end();

}

//----------------------------------------------------------------------

void Monitor::table_subscribers(const std::string& table_name,
                                std::list< SID >& __list) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

  TableCollection::const_iterator it = m_tables.find ( table_name );

  if (it != m_tables.end() )
  {
    DataTable* table = it->second;
    table->table_subscribers( __list );
  }
}

//----------------------------------------------------------------------
void Monitor::add_columns(const std::string& table_name,
                          const std::list<std::string>& cols)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

  TableCollection::iterator iter = m_tables.find(table_name);

  DataTable * table = NULL;

  // search for an already existing table
  if ( iter == m_tables.end() )
  {
    /* table not found, so create */
    table = create_table_NOLOCK( table_name );
  }
  else
  {
    table = iter->second;
  }

  table->add_columns(cols);
}

//----------------------------------------------------------------------
void Monitor::update_table(const std::string & table_name,
                           const std::string & row_key,
                           std::map<std::string, std::string> fields)
{
  std::list< TableEventPtr > events;
  DataTable * table = NULL;

  {
    cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
    TableCollection::iterator iter = m_tables.find(table_name);

    // search for an already existing table
    if ( iter == m_tables.end() )
    {
      /* table not found, so create */
      table = create_table_NOLOCK( table_name );

      // If the table did not exist, then we must apply the table update
      // within the table-lock context, so that the table-creation and
      // table-update events occur together.

      table->update_row( row_key, fields, events );

      return;
    }
    else
    {
      // table already existed... get a reference to the table, and perform
      // the update outside of the tables-lock
      table = iter->second;
    }
  }

  // apply row updates, this is for the case where the table already existed.
  table->update_row( row_key, fields, events );
}

//----------------------------------------------------------------------
void Monitor::update_meta(const std::string & table_name,
                          const std::string & row_key,
                          const std::string & column,
                          const sam::txContainer& meta)
{

  std::list< TableEventPtr > events;
  DataTable * table = NULL;

  {
    cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
    TableCollection::iterator iter = m_tables.find(table_name);

    // search for an already existing table
    if ( iter == m_tables.end() )
    {
      /* table not found, so create */
      table = create_table_NOLOCK( table_name );

      // If the table did not exist, then we must apply the table update
      // within the table-lock context, so that the table-creation and
      // table-update events occur together.


      table->update_meta(row_key, column, meta, events);

      return;
    }
    else
    {
      // table already existed... get a reference to the table, and perform
      // the update outside of the tables-lock
      table = iter->second;
    }
  }

  // apply row updates, this is for the case where the table already existed.
  table->update_meta( row_key, column, meta, events);
}
//----------------------------------------------------------------------
void Monitor::clear_all_tables()
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );


  for (TableCollection::const_iterator it = m_tables.begin();
       it != m_tables.end(); ++it)
  {
    DataTable* tableptr = it->second;
    tableptr->clear_table();
  }
}

//----------------------------------------------------------------------

void Monitor::table_column_attr(const std::string& table_name,
                                const std::string& column,
                                const sam::txContainer& attribute)
{
  DataTable * table = NULL;

  {
    cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
    TableCollection::iterator iter = m_tables.find(table_name);

    // search for an already existing table
    if ( iter == m_tables.end() )
    {
      /* table not found, so create */
      table = create_table_NOLOCK( table_name );
      table->add_column_attr(column, attribute);
      return;
    }
    else
    {
      table = iter->second;
      table->add_column_attr(column, attribute);
      return;
    }
  }

}

//----------------------------------------------------------------------
/*
 * Obtain the DataTable for table_name.  Either an existing table is returned,
 * or a new one is constructed.  This method will not take the lock, so ensure
 * the lock is held before calling this method.
 */
DataTable* Monitor::create_table_NOLOCK(const std::string& table_name)
{
  DataTable * table = new DataTable( table_name, m_ai );

  // TODO: future feature: only add a session if is has specified a
  // pattern that matches the current table

  // add all subscribers to our new table
  std::list< SID > sessions;
  m_ai->session_list( sessions );
  for (std::list< SID >::iterator adit = sessions.begin();
       adit != sessions.end(); ++adit)
  {
    table->add_subscriber( *adit );
  }

  // register our table -- this is why we need to hold the tables lock
  m_tables[ table_name ] = table;

  return table;
}
//----------------------------------------------------------------------
bool Monitor::has_row(const std::string& tablename,
                      const std::string& rowkey) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

  TableCollection::const_iterator iter = m_tables.find(tablename);

  if (iter == m_tables.end())
    return false;
  else
    return iter->second->has_row(rowkey);
}
//----------------------------------------------------------------------
bool Monitor::has_table(const std::string& tablename) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

  return m_tables.find(tablename) != m_tables.end();
}
//----------------------------------------------------------------------
void Monitor::copy_table(const std::string& tablename,
                         AdminInterface::Table& dest) const
{
  // TODO: decision/engineering principle needed here.  Should we always keep
  // the m_mutex lock for the entire duration of the function, or, can we give
  // it up once we have performed the search for the table?  Need to go
  // through what the issues are.

  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
  TableCollection::const_iterator iter = m_tables.find(tablename);

  if (iter != m_tables.end())
  {
    iter->second->copy_table(dest);
  }
}
//----------------------------------------------------------------------
void Monitor::copy_row(const std::string& tablename,
                       const std::string& rowkey,
                       AdminInterface::Row& dest) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
  TableCollection::const_iterator iter = m_tables.find(tablename);

  if (iter != m_tables.end())
  {
    iter->second->copy_row(rowkey, dest);
  }
}

//----------------------------------------------------------------------
void Monitor::broadcast_snapshot()
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );

  for (TableCollection::iterator it = m_tables.begin();
       it != m_tables.end(); ++it)
  {
    DataTable* table = it->second;
    table->snapshot();
  }
}

//----------------------------------------------------------------------
void Monitor::broadcast_snapshot(const std::string& tablename)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
  TableCollection::const_iterator iter = m_tables.find(tablename);

  if (iter != m_tables.end()) iter->second->snapshot();
}

//----------------------------------------------------------------------
void Monitor::clear_table(const std::string& tablename)
{
  std::list< TableEventPtr > events;

  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
  TableCollection::iterator iter = m_tables.find(tablename);

  if ( iter != m_tables.end() )
  {
    iter->second->clear_table();
  }
}
//----------------------------------------------------------------------
size_t Monitor::table_size(const std::string& tablename)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
  TableCollection::const_iterator iter = m_tables.find(tablename);

  if (iter != m_tables.end())
    return iter->second->size();
  else
    return 0;
}

} // namespace exio
