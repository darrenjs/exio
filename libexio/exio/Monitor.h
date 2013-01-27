#ifndef EXIO_MONITOR_H
#define EXIO_MONITOR_H

#include "exio/AdminInterface.h"

#include <map>
#include <string>
#include <list>

#include "mutex.h"



namespace sam
{
  class txContainer;
}

namespace exio {




class AdminInterfaceImpl;
class SID;
class DataTable;

class Monitor
{
  public:
    Monitor(AdminInterfaceImpl * ai);

    ~Monitor();


    bool has_row(const std::string& tablename,
                 const std::string& rowkey) const;

    bool has_table(const std::string& tablename) const;

    void copy_table(const std::string& tablename,
                    AdminInterface::Table&) const;

    void copy_row(const std::string& tablename,
                  const std::string& rowkey,
                  AdminInterface::Row&) const;

    void table_column_attr(const std::string& tablename,
                           const std::string& column,
                           const sam::txContainer& attribute);

    void add_columns(const std::string& tablename,
                     const std::list<std::string>&);

    void clear_table(const std::string& tablename);

    void update_table(const std::string & table_name,
                      const std::string & row_key,
                      std::map<std::string, std::string> fields);

    void update_meta(const std::string & table_name,
                     const std::string & row_key,
                     const std::string & column,
                     const sam::txContainer& meta);

    void clear_all_tables();

    /* Obtain a list of monitoring tables */
    std::list< std::string > tables() const;

    bool table_exists(const std::string& table_name) const;

    /* Get the sessions subscribed to this table */
    void table_subscribers(const std::string& table_name,
                           std::list< SID >&) const;


    void subscribe_all(const SID&);

    void unsubscribe_all(const SID&);

    void broadcast_snapshot();

  private:
    Monitor(const Monitor&); // no copy
    Monitor& operator=(const Monitor&); // no assignment

    DataTable* create_table_NOLOCK(const std::string& table_name);

    // Tables that are being monitored
    typedef std::map< std::string, DataTable* > TableCollection;

    TableCollection m_tables;
    mutable cpp11::mutex m_mutex;  // protect tables

    AdminInterfaceImpl * m_ai;
};

} // namespace exio

#endif