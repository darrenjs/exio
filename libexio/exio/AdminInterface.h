#ifndef EXIO_ADMININTERFACE_H
#define EXIO_ADMININTERFACE_H

#include "exio/AdminCommand.h"
#include "exio/AppSvc.h"

namespace exio
{

class AdminInterfaceImpl;


class AdminInterface;


class AdminInterface
{
  public:

  public:
    AdminInterface(Config config,
                   LogService* logservice);
    ~AdminInterface();

    AppSvc& appsvc() { return m_appsvc; }

    const std::string& serviceid() const;

    // Start the interface
    void start();

    // Stop the interface
    void stop() { /* TODO */ }

    /* ----- Admin command management ----- */

    void   add_admin(AdminCommand);

    /* ----- Client-sessions management ----- */

    size_t session_count() const;

    /* ----- Monitoring ----- */

    typedef std::map<std::string, std::string> Row;
    typedef std::map<std::string, Row >        Table;

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

    void monitor_update(const std::string& tablename,
                        const std::string& rowkey,
                        const std::map<std::string, std::string>& fields);

    void monitor_update_meta(const std::string& table_name,
                             const std::string& rowkey,
                             const std::string& column,
                             const sam::txContainer& meta);

    void monitor_alert(const std::string& source,
                       const std::string& source_type,
                       const std::string& error_str,
                       const std::string& alert_type);

  private:

    AppSvc m_appsvc;
    AdminInterfaceImpl * m_impl;
};

}

#endif
