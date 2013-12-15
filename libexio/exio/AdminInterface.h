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
#ifndef EXIO_ADMININTERFACE_H
#define EXIO_ADMININTERFACE_H

#include "exio/AdminCommand.h"
#include "exio/AppSvc.h"

namespace exio
{

class AdminInterfaceImpl;
class AdminInterface;
class AdminSession;
class AdminSessionListener;

/* Commonly used alert severity codes - note, leading two digits are the Alert
 * Severity Rating (ASR) */
static std::string const SEV_CRITICAL = "70-critical";
static std::string const SEV_HIGH     = "60-high";
static std::string const SEV_MODERATE = "50-moderate";
static std::string const SEV_LOW      = "40-low";
static std::string const SEV_INFO     = "30-info";
static std::string const SEV_DEBUG    = "20-debug";


const char* version_string();

class AdminInterface
{
  public:
    AdminInterface(Config config,
                   LogService* logservice);
    ~AdminInterface();

    AppSvc& appsvc() { return m_appsvc; }

    const std::string& serviceid() const;

    /* Start the interface.  Creates a passive socket for accepting client
     * connections, and a dedicated thread for creating new sessions when
     * clients connect. Returns immediately. */
    void start();

    // Stop the interface
    void stop() { /* TODO */ }

    /* ----- Admin command management ----- */

    void add_admin(AdminCommand);

    /* ----- Client-sessions management ----- */

    bool session_open(SID) const;

    void session_info(SID, sid_desc&, bool& found) const;

    size_t session_count() const;


    /* Sessions initiated locally */
    void   register_ext_session(AdminSession*);
    void deregister_ext_session(AdminSession*);

    /* ----- Monitoring ----- */

    typedef std::map<std::string, std::string> Row;
    typedef std::map<std::string, Row >        Table;

    bool has_row(const std::string& tablename,
                 const std::string& rowkey) const;

    bool has_table(const std::string& tablename) const;

    void copy_rowkeys(const std::string& tablename,
                      std::list< std::string >&) const;

    void copy_table(const std::string& tablename,
                    AdminInterface::Table&) const;

    void copy_table2(const std::string& tablename,
                     std::vector<std::string> & cols,
                     std::vector< std::vector <std::string> >& rows) const;

    void copy_row(const std::string& tablename,
                  const std::string& rowkey,
                  AdminInterface::Row&) const;

    bool copy_field(const std::string& tablename,
                    const std::string& rowkey,
                    const std::string& field,
                    std::string& dest);

    void table_column_attr(const std::string& tablename,
                           const std::string& column,
                           const sam::txContainer& attribute);

    void add_columns(const std::string& tablename,
                     const std::list<std::string>&);


    void clear_table(const std::string& tablename);

    void delete_row(const std::string& tablename,
                    const std::string& rowkey);

    void monitor_snapshot(const std::string& tablename);
    void monitor_snapshot();

    void monitor_update(const std::string& tablename,
                        const std::string& rowkey,
                        const std::map<std::string, std::string>& fields);

    void monitor_update_meta(const std::string& table_name,
                             const std::string& rowkey,
                             const std::string& column,
                             const sam::txContainer& meta);

    /* Alerting
     *
     * Standard codes for severity:
     *
     * SEV_CRITICAL
     * SEV_HIGH
     * SEV_MODERATE   <- default
     * SEV_LOW
     * SEV_INFO / SEV_DEBUG    <- use for non-alerts
     */
    void monitor_alert(const std::string& source,
                       const std::string& source_type,
                       const std::string& error_message,
                       const std::string& alert_type = "",
                       const std::string& alert_id = "",
                       const std::string& severity = SEV_MODERATE);


  private:
    AppSvc m_appsvc;
    AdminInterfaceImpl * m_impl;
};

}

#endif
