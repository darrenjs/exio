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
#ifndef EXIO_ADMININTERFACEIMPL_H
#define EXIO_ADMININTERFACEIMPL_H

#include "exio/Monitor.h"
#include "exio/AdminSession.h"
#include "exio/AdminCommand.h"
#include "exio/AdminServerSocket.h"

#include <string.h>

#define MAX_SESSIONS 1000
#define SESSION_REG_SIZE (MAX_SESSIONS+1)

namespace exio {

class Config;
class LogService;
class AdminInterfaceObserver;

struct SessionReg
{
    AdminSession* ptr;


    bool used() const { return ptr != NULL; }
    static void reset(SessionReg* reg)
    {
      memset(reg, 0, sizeof(SessionReg));
    }
};

class AdminInterfaceImpl : public AdminSession::Listener
{
  private:

    static cpp11::mutex admin_sessionid_lock;
    static unsigned long long admin_sessionid;

    static unsigned long long next_admin_sessionid()
    {
      cpp11::lock_guard< cpp11::mutex > guard( admin_sessionid_lock );

      return admin_sessionid++;
    }

  public:

    AdminInterfaceImpl(AdminInterface *);

    // TODO: why no destructor?

    AppSvc& appsvc() { return m_appsvc; }

    const std::string& serviceid() const { return m_svcid; }

    /* ----- Session management ----- */
    bool session_exists(const SID& id) const;
    void session_list(std::list< SID > &) const;

    void session_info(SID, sid_desc&, bool& found) const;

    void session_stop_one(const SID&);
    void session_stop_all();
    size_t session_count() const;

    void createNewSession(int fd);

    /* ----- Messaging ----- */
    void send_one(const sam::txMessage&,
                  const SID&);
    void send_one(const std::list<sam::txMessage>&,
                  const SID&);
    void send_all(const sam::txMessage& msg);


    /* ----- Session listener callbacks ----- */
    virtual void session_msg_received(const sam::txMessage&,
                                      AdminSession&);
    virtual void session_closed(AdminSession& session);

    /* ----- Admin management ----- */
    void          admin_add(AdminCommand);
    AdminCommand* admin_find(const std::string& name);

    /* ----- Monitoring ----- */

    bool has_row(const std::string& tablename,
                 const std::string& rowkey) const;

    bool has_table(const std::string& tablename) const;

    void copy_table(const std::string& tablename,
                    AdminInterface::Table&) const;

    void copy_row(const std::string& tablename,
                  const std::string& rowkey,
                  AdminInterface::Row&) const;

    bool copy_field(const std::string& tablename,
                    const std::string& rowkey,
                    const std::string& field,
                    std::string& dest);

    void table_column_attr(const std::string& table_name,
                           const std::string& column,
                           const sam::txContainer& attribute);

    void add_columns(const std::string& tablename,
                     const std::list<std::string>&);

    void clear_table(const std::string& tablename);

    void delete_row(const std::string& tablename,
                    const std::string& rowkey);

    void monitor_snapshot(const std::string& tablename);
    void monitor_snapshot();

    void monitor_update(const std::string& table_name,
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

    /* ----- Lifetime ----- */
    void start();
    void housekeeping();

  private:

    AdminResponse admincmd_list_tables(AdminRequest& r);
    AdminResponse admincmd_subscribe(AdminRequest& r);
    AdminResponse admincmd_sessions(AdminRequest& r);
    AdminResponse admincmd_help(AdminRequest& r);
    AdminResponse admincmd_info(AdminRequest& r);
    AdminResponse admincmd_snapshot(AdminRequest& r);
    AdminResponse admincmd_diags(AdminRequest& r);
    AdminResponse admincmd_table_subs(AdminRequest& r);
    AdminResponse admincmd_del_session(AdminRequest& r);

    void handle_logon_msg(const sam::txMessage&, AdminSession&);
    void session_cleanup();

    void serialise_admins(sam::txMessage&) const;

    void handle_admin_request(const sam::txMessage&, AdminSession&);

    static void helper_session_descr(std::ostream&, AdminSession&,
                                     const std::string&);

  private:
    AppSvc&          m_appsvc;
    LogService *     m_logsvc;
    std::string      m_svcid;

    AdminServerSocket m_serverSocket;

    mutable struct
    {
        unsigned long createdCount;
//        std::map< SID, AdminSession* > items;
        cpp11::mutex lock;

        /* valid indexed are: 1 to MAX_SESSIONS */
        SessionReg reg[SESSION_REG_SIZE];
        size_t next;

    } m_sessions;

    mutable struct
    {

        std::list< AdminSession* > items;
        cpp11::mutex lock;
    } m_expired_sessions;

    struct
    {
        std::map<std::string, AdminCommand> items;
        mutable cpp11::mutex lock;
    } m_admins;

    // Application monitoring component
    Monitor m_monitor;

    time_t m_start_time;

    mutable cpp11::mutex m_create_session_lock;
};

} // namespace exio

#endif
