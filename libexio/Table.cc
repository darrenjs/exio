#include "exio/Table.h"

#include "exio/MsgIDs.h"
#include "exio/AdminInterfaceImpl.h"
#include "exio/Logger.h"
#include "exio/AdminSession.h"
#include "exio/AppSvc.h"
#include "exio/Logger.h"

#include <sstream>
#include <set>


namespace exio
{


class SnapshotSerialiser
{
  public:
    SnapshotSerialiser();
    void set_table_name(const std::string& table_name);

    void add_row(DataRow&, const DataTable::MetaForCol* meta = NULL);

    const sam::txMessage& message() { return m_msg;}

  private:
    sam::txMessage m_msg;

    static sam::txMessage empty_msg();
};




class TableDescrSerialiser
{
  public:
    TableDescrSerialiser(const std::string& table_name);

    void add_column(const std::string&,
                    const std::list<sam::txContainer>* attrptr);
    const sam::txMessage& message() { return m_msg;}

  private:

    std::string m_table_name;
    std::list< std::string > m_column_names;

    sam::txMessage m_msg;

};

//======================================================================
SnapshotSerialiser::SnapshotSerialiser()
  : m_msg( SnapshotSerialiser::empty_msg() )
{
}

void SnapshotSerialiser::set_table_name(const std::string& table_name)
{
  m_msg.root().put_field( id::QN_tablename, table_name );
}

void SnapshotSerialiser::add_row(DataRow& datarow,
                                 const DataTable::MetaForCol* meta)
{
  // get container into which all the rows are stored
  sam::txContainer & body = m_msg.root().put_child(id::body);

  // counting entries gives #rows
  size_t nrows = body.count_child();

  // generate the name of our new row
  std::ostringstream rowname;
  rowname << id::row_prefix << nrows;

  // create container for new row
  sam::txContainer & row = body.put_child( rowname.str() );

  // Now populate the row-container with the fields in the actual DataRow
  for (DataRow::iterator j = datarow.begin(); j != datarow.end(); ++j)
  {
    row.put_field(j.name(), j.value() );
  }

  // Next, add any per cell meta data (PCMD)
  if (meta)
  {
    for( DataTable::MetaForCol::const_iterator m = meta->begin();
         m != meta->end(); ++m)
    {
      row.put_child(m->second);
    }
  }

  // update the row count
  std::ostringstream count;
  count << nrows+1;
  body.put_field(id::rows, count.str());

}

sam::txMessage SnapshotSerialiser::empty_msg()
{
  sam::txMessage msg(id::tableupdate) ;

  msg.root().put_field(id::QN_tablename, "");
  msg.root().put_field(id::QN_body_rows, "0");
  msg.root().put_field(id::QN_msgtype, id::tableupdate);

  return msg;
}



//----------------------------------------------------------------------
DataTable::DataTable(const std::string& table_name,
                     AdminInterfaceImpl * ai)
  : m_table_name( table_name ),
    m_ai( ai ),
    m_appsvc( &(ai->appsvc()) )
{
}
//----------------------------------------------------------------------
void DataTable::add_subscriber(const SID& session)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );


  // NOTE: don't need to have this kind of logging yet.  We don't yet have
  // ability to subscribe/subscribe from individual tables

//  _INFO_(m_appsvc->log(), "Adding " << session << " to table ( " <<
//         m_table_name << " )");

  m_subscribers.push_back( session );


  /* serialise table description */

  // TODO: move to a separate method, and then remove the class?

  // first, build a list of all known columns - this includes columns for
  // which we presently have data, and columns for which we only have meta
  // information (eg style, command, etc).

  std::set<std::string> cols;
  for (std::vector< std::string >::iterator it=m_columns.begin();
       it != m_columns.end(); ++it) cols.insert(*it);
  for (std::map<std::string, std::list<sam::txContainer> >::iterator
         it=m_column_attrs.begin(); it != m_column_attrs.end(); ++it)
    cols.insert(it->first);


  TableDescrSerialiser tabledescr( m_table_name );
  for (std::set< std::string >::iterator it=cols.begin();
       it != cols.end(); ++it)
  {
    // try and obtain some attributes for our column
    std::list<sam::txContainer>* attrs = NULL;
    std::map<std::string, std::list<sam::txContainer> >::iterator attriter
      = m_column_attrs.find( *it );
    if (attriter != m_column_attrs.end()) attrs = &(attriter->second);

    // add column with any attributes is may have
    tabledescr.add_column( *it, attrs );

  }
  m_ai->send_one(tabledescr.message(), session);



  /* Admin description */

//  m_ai->send_one(am, session);

  // serialise table content
  _nolock_send_snapshopt( session );
  // SnapshotSerialiser serial;
  // serial.set_table_name( m_table_name );

  // for( std::vector< DataRow >::iterator iter=m_rows.begin();
  //      iter != m_rows.end(); ++iter)
  // {
  //   const MetaForCol* meta = NULL;

  //   PCMD::const_iterator rowpcmd = m_pcmd.find(iter->rowkey());
  //   if (rowpcmd != m_pcmd.end())
  //   {
  //     meta = &(rowpcmd->second);
  //   }

  //   serial.add_row(*iter, meta);
  // }
  // m_ai->send_one(serial.message(), session);

}

//----------------------------------------------------------------------
void DataTable::del_subscriber(const SID& session)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );


  while( true )
  {
    std::vector< SID >::iterator iter
      = find(m_subscribers.begin(), m_subscribers.end(), session);
    if (iter == m_subscribers.end()) break;



    // NOTE: don't need to have this kind of logging yet.  We don't yet have
    // ability to subscribe/subscribe from individual tables
    //_INFO_(m_appsvc->log(), "Removing " << session << " from " << m_table_name);

    m_subscribers.erase( iter );
  };
}
//----------------------------------------------------------------------

void DataTable::_nolock_publish_update(std::list<TableEventPtr>& events)
{
  /* TODO: This method needs refactoring */

  /* TODO: eventually I should replace this method with a proper table-event
   * mode.  Ie, table events occur, in response to client table instruments.
   * These table events get added to a list, which in turn is dispatched to
   * once or more table-event listeners.  One such listener will be a
   * table-even-publisher, which will serialise and publish the event to
   * subscribers.*/

  std::list<sam::txMessage> msgs;


  for (std::list<TableEventPtr>::iterator iter = events.begin();
       iter != events.end(); ++iter)
  {
    TableEvent* ev = iter->ev;

    if (ev->type == TableEvent::eRowMultiUpdate)
    {
      RowMultiUpdate* rev = dynamic_cast<RowMultiUpdate*>(ev);

      rev->serialise(msgs);
      for (std::list<sam::txMessage>::iterator mit = msgs.begin();
           mit != msgs.end(); ++mit)
      {

        for (std::vector<SID>::iterator s = m_subscribers.begin();
             s != m_subscribers.end(); ++s)
        {
          m_ai->send_one(*mit, *s);
        }
      }
    }
    else if (ev->type == TableEvent::eTableCleared)
    {
      TableCleared* rev = dynamic_cast<TableCleared*>(ev);

      rev->serialise(msgs);
      for (std::list<sam::txMessage>::iterator mit = msgs.begin();
           mit != msgs.end(); ++mit)
      {

        for (std::vector<SID>::iterator s = m_subscribers.begin();
             s != m_subscribers.end(); ++s)
        {
          m_ai->send_one(*mit, *s);
        }
      }
    }
    else if (ev->type == TableEvent::ePCMD)
    {
      PCMDEvent* rev = dynamic_cast<PCMDEvent*>(ev);

      rev->serialise(msgs);

      for (std::list<sam::txMessage>::iterator mit = msgs.begin();
           mit != msgs.end(); ++mit)
      {
        for (std::vector<SID>::iterator s = m_subscribers.begin();
             s != m_subscribers.end(); ++s)
        {
          m_ai->send_one(*mit, *s);
        }
      }
    }
  }

  // TODO: I should be deleting the objecst here????
  events.clear();
}

//----------------------------------------------------------------------
void DataTable::update_row(const std::string & rowkey,
                           const std::map<std::string, std::string> & fields,
                           std::list<TableEventPtr>& events)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock ); // lock table

  // Does the row_exist? If not, add...
  if (not _nolock_has_row(rowkey))
  {
    _nolock_add_row( rowkey, events );
  }

  for (std::map<std::string, std::string>::const_iterator fit = fields.begin();
       fit != fields.end(); ++fit)
  {
    add_column_NOLOCK(fit->first, events);
  }

  m_rows[ m_row_index[ rowkey ] ].update_fields(fields, events);

  if ( not events.empty() ) _nolock_publish_update( events );
}
//----------------------------------------------------------------------
void DataTable::add_columns(const std::list<std::string>& cols)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );

  std::list< TableEventPtr > events;

  for (std::list<std::string>::const_iterator it = cols.begin();
       it != cols.end(); ++it)
  {
    add_column_NOLOCK(*it, events);
  }

  if ( not events.empty() ) _nolock_publish_update( events );
}
//----------------------------------------------------------------------
void DataTable::add_column_NOLOCK(const std::string & column,
                                  std::list<TableEventPtr>& events)
{
  // Are we really adding a new column?
  bool adding_column = (m_column_index.find(column) == m_column_index.end());

  if (adding_column)
  {
    events.push_back( new NewColumn( m_table_name, column ));
    m_columns.push_back( column );

    // rebuild column index
    m_column_index.clear();
    for (size_t c = 0; c < m_columns.size(); ++c)
      m_column_index[ m_columns[c] ] = c;

    // add column to every row
    std::map<std::string, std::string> fields;
    fields[ column ] = "";
    for (std::vector< DataRow >::iterator iter = m_rows.begin();
         iter != m_rows.end(); ++iter)
    {
      iter->update_fields( fields, events );
    }
  }
}

//----------------------------------------------------------------------
const std::string& DataTable::get_row_field(const std::string & rowkey,
                                            const std::string & column) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );

  std::map< std::string, size_t >::const_iterator iter = m_row_index.find( rowkey );
  if ( iter == m_row_index.end())
    throw std::out_of_range( "row not found" );

  return m_rows[ iter->second ].get_field( column );
}

//----------------------------------------------------------------------
std::vector< std::string > DataTable::get_rowkeys() const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );

  std::vector< std::string > __rowkeys( m_rows.size() );

  for (size_t i = 0; i < m_rows.size(); ++i)
    __rowkeys[i] = m_rows[i].rowkey();

  return __rowkeys;
}

//----------------------------------------------------------------------
bool DataTable::has_row(const std::string& rowkey) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );
  return _nolock_has_row( rowkey );
}

//----------------------------------------------------------------------
size_t DataTable::get_column_count() const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );
  return m_columns.size();
}

//----------------------------------------------------------------------
size_t DataTable::get_row_count() const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );
  return m_rows.size();
}

//----------------------------------------------------------------------
void DataTable::table_subscribers(std::list< SID >& __list) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );

  for (std::vector< SID >::const_iterator iter = m_subscribers.begin();
       iter != m_subscribers.end(); ++iter)
  {
    __list.push_back( *iter );
  }
}


//----------------------------------------------------------------------
bool DataTable::_nolock_has_row(const std::string& rowkey) const
{
  return m_row_index.find( rowkey ) != m_row_index.end();
}


//----------------------------------------------------------------------
void DataTable::_nolock_add_row(const std::string& rowkey,
                                std::list<TableEventPtr>& events)
{
  m_rows.push_back( DataRow( rowkey, m_table_name ) );

  // rebuild index
  m_row_index.clear();
  for (size_t i = 0; i < m_rows.size(); ++i)
  {
    m_row_index[ m_rows[i].rowkey() ] = i;
  }

  events.push_back( new NewRow( m_table_name, rowkey ));
}

//----------------------------------------------------------------------
void DataTable::clear_table()
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );

  std::list< TableEventPtr > events;

  // clear all our rows
  m_rows.clear();
  m_row_index.clear();

  // raise an event to indicate this table change
  events.push_back( new TableCleared(m_table_name) );

  _nolock_publish_update(events);
}

//----------------------------------------------------------------------
void DataTable::add_column_attr(const std::string& column,
                              const sam::txContainer& attribute)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );

  _INFO_(m_appsvc->log(), "Adding attr " << column << " to table "
         << m_table_name);

  std::map<std::string, std::list<sam::txContainer> >::iterator iter =
    m_column_attrs.find( column );

  if (iter != m_column_attrs.end())
  {
    iter->second.push_back( attribute );
  }
  else
  {
    std::list<sam::txContainer>& attrlist = m_column_attrs[ column ];
    attrlist.push_back( attribute );
  }

  // NOTE: currently we are not immediately publishing column attributes. This
  // is probably fine, since column attributes will noramlly defined when the
  // server application is created, which means before any clients have had
  // chance to connect. However, this is something we change later.
}


//----------------------------------------------------------------------
void DataTable::copy_table(AdminInterface::Table& dest) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );

  for (std::vector< DataRow >::const_iterator it = m_rows.begin();
       it != m_rows.end(); ++it)
  {

    it->copy_row(dest[it->rowkey()]);
  }
}
//----------------------------------------------------------------------
void DataTable::copy_row(const std::string& rowkey,
                         AdminInterface::Row& dest) const
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock );

  std::map<std::string, size_t>::const_iterator it = m_row_index.find(rowkey);
  if (it != m_row_index.end())
  {
    m_rows[ it->second ].copy_row( dest );
  }
}

//----------------------------------------------------------------------
void DataTable::update_meta(const std::string & rowkey,
                            const std::string & fieldname,
                            const sam::txContainer& meta,
                            std::list<TableEventPtr>& events)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock ); // lock table

  /*  NOTE: we are adding meta data here, not actual data, thus, we don't
   *  ensure that the data table acutally contains the row or column being
   *  referred to.
   */

  // TODO: here, I should test that the new meta is different to the old meta,
  // before doing a publish
  MetaForCol& metaForCol = m_pcmd[ rowkey ];
  metaForCol[ fieldname ] = meta;

  // ensure the meta field name is correct
  metaForCol[ fieldname ].name(".meta." + fieldname);

  events.push_back( new PCMDEvent( m_table_name, rowkey, fieldname, meta) );

  if ( not events.empty() ) _nolock_publish_update( events );
}

//----------------------------------------------------------------------
void DataTable::snapshot()
{
  cpp11::lock_guard<cpp11::mutex> guard( m_tablelock ); // lock table


  for (std::vector<SID>::iterator s = m_subscribers.begin();
       s != m_subscribers.end(); ++s)
  {
    _nolock_send_snapshopt(*s);
  }
}
//----------------------------------------------------------------------
void DataTable::_nolock_send_snapshopt(const SID& session)
{
  /* NOTE: this method assumes the table-lock is held before entry */

  // TODO: here, should also send the tabledescr and embedded admins too.  See
  // toward top of file, where this is called, that site also dopes publish
  // the tabledescr.

  // serialise table content
  SnapshotSerialiser serial;
  serial.set_table_name( m_table_name );

  for( std::vector< DataRow >::iterator iter=m_rows.begin();
       iter != m_rows.end(); ++iter)
  {
    const MetaForCol* meta = NULL;

    PCMD::const_iterator rowpcmd = m_pcmd.find(iter->rowkey());
    if (rowpcmd != m_pcmd.end())
    {
      meta = &(rowpcmd->second);
    }

    serial.add_row(*iter, meta);
  }
  m_ai->send_one(serial.message(), session);
}
//======================================================================

DataRow::DataRow(const std::string& rowkey,
                 const std::string& __table_name)
  : m_rowkey( rowkey ),
    m_table_name(__table_name)
{
  m_fields[ id::row_key ] = rowkey;
}

//----------------------------------------------------------------------
DataRow::iterator DataRow::begin()
{
  return DataRow::iterator( m_fields.begin() );
}

//----------------------------------------------------------------------
DataRow::iterator DataRow::end()
{
  return DataRow::iterator( m_fields.end() );
}


//----------------------------------------------------------------------
bool DataRow::update_fields(const std::map<std::string, std::string>& fields,
                            std::list<TableEventPtr>& events)
{
  /*

  TODO: clean this up.  I really need to use a smart pointer in here to
  prevent the risk of memory leak if an exception is thrown

  */

  RowMultiUpdate * eventptr = NULL;
  bool rowupdated = false;

  for (std::map<std::string, std::string>::const_iterator up = fields.begin();
       up != fields.end(); ++up)
  {
    // Get the existing value.  We will not update the field if the
    // value is the same
    Fields::iterator ours = m_fields.find( up->first );
    if (ours == m_fields.end() or ours->second.value != up->second)
    {
      // we have discovered a field change
      if (ours == m_fields.end())
      {
        m_fields[ up->first ] = up->second;
      }
      else
      {
         ours->second.value = up->second;
      }
      if (eventptr == NULL) eventptr = new RowMultiUpdate( m_table_name,
                                                           m_rowkey );
      eventptr->fields[ up->first ] = up->second;
      rowupdated = true;
    }
  }

  if (eventptr) events.push_back( eventptr );

  return rowupdated;
}

//----------------------------------------------------------------------
bool DataRow::has_field(const std::string& fn) const
{
  return m_fields.find( fn ) != m_fields.end();
}

//----------------------------------------------------------------------
const std::string& DataRow::get_field(const std::string& fn) const
{
  Fields::const_iterator iter = m_fields.find( fn );

  if (iter == m_fields.end()) throw std::out_of_range("field not found");

  return iter->second.value;
}

//----------------------------------------------------------------------
void DataRow::clear()
{
  m_fields.clear();
}

//----------------------------------------------------------------------
void DataRow::copy_row(AdminInterface::Row& dest) const
{
  for (Fields::const_iterator it = m_fields.begin();
       it != m_fields.end(); ++it)
  {
    // TODO: use an insert hint here, to improve performance.  Should I use
    // back() or end()? Maybe to a comparison?
    dest.insert( std::make_pair(it->first, it->second.value) );
  }
}

//======================================================================

void TableDescrSerialiser::add_column(
  const std::string& column_name,
  const std::list<sam::txContainer>* attrptr)
{
  sam::txContainer & columns = m_msg.root().put_child( id::QN_columns );

  // couting entries gives #columns
  size_t ncols = columns.count_child();

  // generate the name of our new item
  std::ostringstream itemname;
  itemname << id::msg_prefix << ncols;

  // create container for new column, and set column name
  sam::txContainer & colcont = columns.put_child( itemname.str() );
  colcont.put_field(id::colname, column_name);

  // if we have been given column attributes, apply them to the column
  // container
  if (attrptr)
  {
    std::list<sam::txContainer>::const_iterator iter = attrptr->begin();
    for (iter = attrptr->begin(); iter != attrptr->end(); ++iter)
    {
      colcont.put_child( *iter );
    }
  }


  // update the count
  std::ostringstream count;
  count << ncols+1;
  columns.put_field( id::msgcount, count.str() );

}

//----------------------------------------------------------------------
TableDescrSerialiser::TableDescrSerialiser(const std::string& table_name)
  : m_msg( id::tabledescr )
{
  m_msg.root().put_field( id::QN_msgtype, id::tabledescr);
  m_msg.root().put_field( id::QN_tablename, table_name );
  sam::txContainer & columns = m_msg.root().put_child( id::QN_columns );
  columns.put_field( id::msgcount, "0" );
}


} // exio