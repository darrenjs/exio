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
#ifndef EXIO_TABLE_H
#define EXIO_TABLE_H

#include "exio/TableEvents.h"
#include "exio/AdminInterface.h"

#include "mutex.h"

#include <algorithm>
#include <sstream>

namespace exio
{

class AdminInterfaceImpl;
class AppSvc;
class SID;

/**
 * Represent a row of data in a table.
 */
class DataRow
{
    class FieldMeta
    {
    };

    struct Field
    {
        std::string value;
        FieldMeta   meta;

        Field() {}
        Field(const std::string & v) : value(v) {}
    };

    typedef std::map< std::string, Field > Fields;

  public:

    class iterator
    {
      public:

        bool operator==(const iterator& rhs) const
        {
          return m_iter == rhs.m_iter;
        }

        bool operator!=(const iterator& rhs) const
        {
          return m_iter != rhs.m_iter;
        }

        void operator++()  { ++m_iter; }

        const std::string& name() const { return m_iter->first;}
        const std::string& value() const { return m_iter->second.value;}

      private:
        iterator(Fields::const_iterator i) : m_iter(i) {}
        Fields::const_iterator m_iter;

        friend class DataRow;
    };

  public:

    /* Methods */

    explicit DataRow(const std::string& rowkey,
                     const std::string& __table_name);

    const std::string& rowkey() const { return m_rowkey; }

    bool update_fields(const std::map<std::string, std::string>& fields,
                       std::list<TableEventPtr>& events);

    void copy_row(AdminInterface::Row& dest) const;

    bool               has_field(const std::string&) const;

    /** Get field, or throw out_of_range exception if not found */
    const std::string& get_field(const std::string&) const;

    /** Copy field. Return value is true if field was found. */
    bool copy_field(const std::string&, std::string&) const;

    void clear();

    iterator       begin() const;
    iterator       end()   const;

  private:

    std::string m_rowkey;
    std::string m_table_name;  // table this row belongs to.
    Fields m_fields;
};


/*
 * Represent a table of data, which is the basic unit of monitoring in exio.
 */
class DataTable
{
  public:
    explicit DataTable(const std::string& table_name,
                       AdminInterfaceImpl * ai);


    void copy_table(AdminInterface::Table& dest) const;
    void copy_table(std::vector<std::string> & cols,
                    std::vector< std::vector <std::string> >& rows) const;

    void copy_row(const std::string& rowkey, AdminInterface::Row&) const;

    void copy_rowkeys(std::list< std::string >& dest) const;

    bool copy_field(const std::string& rowkey,
                    const std::string& field,
                    std::string& dest) const;

    const std::string& table_name() const { return m_table_name; }

    bool has_row(const std::string& rowkey) const;

    std::vector< std::string > get_rowkeys() const;
    std::vector< std::string > get_columns() const { return m_columns; }

    size_t get_column_count() const;
    size_t get_row_count() const;

    const std::string& get_row_field(const std::string & rowkey,
                                     const std::string & column) const;

    void clear_table();

    void delete_row(const std::string & rowkey);

    /* Principle method for updating table content */
    void update_row(const std::string & rowkey,
                    const std::map<std::string, std::string> & fields,
                    std::list<TableEventPtr>& events);

    /* Update per-cell-meta-data */
    void update_meta(const std::string & rowkey,
                     const std::string & column,
                     const sam::txContainer& meta,
                     std::list<TableEventPtr>& events);

    void add_columns(const std::list<std::string>& cols);

    /* Can throw */
    void add_subscriber(const SID& session);

    void del_subscriber(const SID& session);

    /* Get the sessions subscribed to this table */
    void table_subscribers(std::list< SID >&) const;

    void add_column_attr(const std::string& column,
                         const sam::txContainer& attribute);

    /* Publish snapshot to all subscribers */
    void snapshot();

    size_t size() const;
  private:


    void _nolock_add_row(const std::string& rowkey,
                         std::list<TableEventPtr>& events);

    bool _nolock_has_row(const std::string& rowkey) const;

    void _nolock_publish_update(std::list<TableEventPtr>&);

    void _nolock_send_snapshopt(const SID&);
    //void _nolock_send_snapshopt_as_single_msg(const SID&);

    void add_column_NOLOCK(const std::string & column,
                           std::list<TableEventPtr>& events);

    void copy_subscribers(std::vector< SID > &subs) const;

    std::string m_table_name;
    AdminInterfaceImpl * m_ai;
    AppSvc * m_appsvc;

    std::vector< std::string >      m_columns;
    std::map< std::string, size_t > m_column_index;

    // TODO: do we need to have a separate vector<> and map<> ? Why not just
    // use a map<> ?
    std::vector< DataRow >          m_rows;
    std::map< std::string, size_t > m_row_index;

    mutable cpp11::mutex m_subscriberslock;
    std::vector< SID > m_subscribers;

    // Note: if ever the subscriber-lock and table-lock have to be held at the
    // same time, then the table-lock must be locked first, followed by the
    // subscribers-lock
    mutable cpp11::mutex m_tablelock; // big table lock




    /* Column attributes */
    std::map<std::string, std::list<sam::txContainer> > m_column_attrs;


    /* Per-cell-meta-data
     *
     * NOTE: currently we are not storing this directly in the DataRow.
     * Simple reason is that it is possible to assign meta data to a cell that
     * does not exist, eg, for a row or column that is presently not part of
     * the actual table.  Thus, if we tryied to palce our meta data inside
     * DataRow, we would need to handle DataRow instances which didn't
     * actually correspond to any real data.
     */

  public:
    typedef std::map< std::string, sam::txContainer  > MetaForCol;
    typedef std::map< std::string, MetaForCol  > PCMD; // per-cell-meta-data

  private:
    PCMD m_pcmd;  // map of rowkey to col-to-meta
    int m_batchsize;
};


//======================================================================

} // namespace

#endif
