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
#include "exio/AdminInterface.h"
#include "exio/AdminInterfaceImpl.h"
#include "exio/Table.h"

#include "mutex.h"

#include <iostream>

namespace exio {

//----------------------------------------------------------------------
AdminInterface::AdminInterface(Config config,
                               LogService* logservice)
  : m_appsvc(config, logservice),
    m_impl( new AdminInterfaceImpl(this) )
{
}

//----------------------------------------------------------------------

AdminInterface::~AdminInterface()
{
  delete m_impl;
}

//----------------------------------------------------------------------

const std::string& AdminInterface::serviceid() const
{
  return m_impl->serviceid();
}

//----------------------------------------------------------------------

void AdminInterface::start()
{
  m_impl->start();
}

//----------------------------------------------------------------------

void AdminInterface::add_admin(AdminCommand c)
{
  m_impl->admin_add(c);
}

//----------------------------------------------------------------------

size_t AdminInterface::session_count() const
{
  return m_impl->session_count();
}
//----------------------------------------------------------------------
void AdminInterface::monitor_update(const std::string& table_name,
                                    const std::string& rowkey,
                                    const std::map<std::string, std::string> & fields)
{
  m_impl->monitor_update(table_name,
                         rowkey,
                         fields);
}
//----------------------------------------------------------------------
void AdminInterface::monitor_update_meta(const std::string& table_name,
                                         const std::string& rowkey,
                                         const std::string& column,
                                         const sam::txContainer& meta)
{
  m_impl->monitor_update_meta(table_name, rowkey, column, meta);
}
//----------------------------------------------------------------------
bool AdminInterface::has_row(const std::string& tablename,
                             const std::string& rowkey) const
{
  return m_impl->has_row(tablename, rowkey);
}

//----------------------------------------------------------------------

bool AdminInterface::has_table(const std::string& tablename) const
{
  return m_impl->has_table(tablename);
}

//----------------------------------------------------------------------
void AdminInterface::copy_table(const std::string& tablename,
                                AdminInterface::Table& dest) const
{
  m_impl->copy_table(tablename, dest);
}

//----------------------------------------------------------------------
void AdminInterface::copy_table2(const std::string& tablename,
                                 std::vector<std::string> & cols,
                                 std::vector< std::vector <std::string> >& rows) const
{
  m_impl->copy_table2(tablename, cols, rows);
}
//----------------------------------------------------------------------
void AdminInterface::copy_row(const std::string& tablename,
                              const std::string& rowkey,
                              AdminInterface::Row& dest) const
{
  m_impl->copy_row(tablename, rowkey, dest);
}
//----------------------------------------------------------------------

void AdminInterface::table_column_attr(const std::string& table_name,
                                       const std::string& column,
                                       const sam::txContainer& attribute)
{
  m_impl->table_column_attr(table_name, column, attribute);
}

//----------------------------------------------------------------------

void AdminInterface::add_columns(const std::string& table_name,
                                 const std::list<std::string>& cols)
{
  m_impl->add_columns(table_name, cols);
}



//----------------------------------------------------------------------

// void AdminInterface::send_one(const sam::txMessage& msg,
//                               const SID& id)
// {
//   m_impl->send_one(msg, id);
// }

//----------------------------------------------------------------------

// void AdminInterface::session_list(std::list< SID > & sessions) const
// {
//   m_impl->session_list( sessions );
// }

//----------------------------------------------------------------------
void AdminInterface::monitor_alert(const std::string& source,
                                   const std::string& source_type,
                                   const std::string& error_str,
                                   const std::string& alert_type)
{
  m_impl->monitor_alert(source,
                        source_type,
                        error_str,
                        alert_type);
}

//----------------------------------------------------------------------

void AdminInterface::clear_table(const std::string& tablename)
{
  m_impl->clear_table(tablename);
}
//----------------------------------------------------------------------
void AdminInterface::delete_row(const std::string& tablename,
                                const std::string& rowkey)
{
  m_impl->delete_row(tablename, rowkey);
}
//----------------------------------------------------------------------
void AdminInterface::session_info(SID sid, sid_desc& sd, bool& sf) const
{
  m_impl->session_info(sid, sd, sf);
}
//----------------------------------------------------------------------
bool AdminInterface::session_open(SID s) const
{
  return m_impl->session_open(s);
}
//----------------------------------------------------------------------
void AdminInterface::monitor_snapshot(const std::string& tablename)
{
  m_impl->monitor_snapshot(tablename);
}
//----------------------------------------------------------------------
void AdminInterface::monitor_snapshot()
{
  m_impl->monitor_snapshot();
}
//----------------------------------------------------------------------
bool AdminInterface::copy_field(const std::string& tablename,
                                const std::string& rowkey,
                                const std::string& field,
                                std::string& dest)
{
  return m_impl->copy_field(tablename, rowkey, field, dest);
}

//----------------------------------------------------------------------

void AdminInterface::copy_rowkeys(const std::string& tablename,
                                  std::list< std::string >& dest) const
{
  m_impl->copy_rowkeys(tablename, dest);
}
//----------------------------------------------------------------------
}
