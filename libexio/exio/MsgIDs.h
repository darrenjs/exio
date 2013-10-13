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
#ifndef EXIO_MSGIDS_H
#define EXIO_MSGIDS_H

namespace exio
{
namespace id  /* identifiers */
{
  // id values for field names and values
  static const std::string admin_prefix   = "admin_";
  static const std::string admincount     = "admincount";
  static const std::string adminset       = "adminset";
  static const std::string alert_type     = "alert_type";
  static const std::string args_COUNT     = "args_COUNT";
  static const std::string args_prefix    = "args_";
  static const std::string autoclose      = "autoclose";
  static const std::string body           = "body";
  static const std::string colname        = "colname";
  static const std::string columns        = "columns";
  static const std::string command        = "command";
  static const std::string dest           = "dest";
  static const std::string error          = "error";
  static const std::string head           = "head";
  static const std::string longhelp       = "long";
  static const std::string more           = "more";
  static const std::string msg_prefix     = "msg_";
  static const std::string msgcount       = "msgcount";
  static const std::string msgtype        = "msgtype";
  static const std::string name           = "name";
  static const std::string noautosub      = "noautosub";
  static const std::string pending        = "pending";
  static const std::string reqseqno       = "reqseqno";
  static const std::string rescode        = "rescode";
  static const std::string respdata       = "respdata";
  static const std::string resptype       = "resptype";
  static const std::string restext        = "restext";
  static const std::string row_key        = "RowKey";
  static const std::string row_last       = "RowLastUpdated";
  static const std::string row_prefix     = "row_";
  static const std::string rows           = "rows";
  static const std::string scalarlist     = "scalarlist";
  static const std::string serviceid      = "serviceid";
  static const std::string shorthelp      = "short";
  static const std::string snapi          = "snapi";
  static const std::string snapn          = "snapn";
  static const std::string source         = "source";
  static const std::string source_type    = "source_type";
  static const std::string synthetic      = "synthetic";
  static const std::string table          = "table";
  static const std::string tablename      = "tablename";
  static const std::string testrequest    = "testrequest";
  static const std::string text           = "text";
  static const std::string user           = "user";
  static const std::string value          = "value";

  // true/false representation
  static const std::string True   = "1";  // yes
  static const std::string False  = "0";  // no

  // TODO: where is QNAME defined?  It should really be in this header.

  // qualified-name id for fields
  static const sam::qname QN_body_respdata  = QNAME( body, respdata );
  static const sam::qname QN_body_resptype  = QNAME( body, resptype );
  static const sam::qname QN_body_rows      = QNAME( body, rows );
  static const sam::qname QN_columns        = QNAME( body, columns );
  static const sam::qname QN_command        = QNAME( head, command );
  static const sam::qname QN_head_reqseqno  = QNAME( head, reqseqno );
  static const sam::qname QN_head_snapi     = QNAME( head, snapi );
  static const sam::qname QN_head_snapn     = QNAME( head, snapn );
  static const sam::qname QN_head_user      = QNAME( head, user );
  static const sam::qname QN_more           = QNAME( head, more );
  static const sam::qname QN_msgtype        = QNAME( head, msgtype );
  static const sam::qname QN_noautosub      = QNAME( head, noautosub );
  static const sam::qname QN_rescode        = QNAME( head, rescode );
  static const sam::qname QN_restext        = QNAME( head, restext );
  static const sam::qname QN_serviceid      = QNAME( head, serviceid );
  static const sam::qname QN_tablename      = QNAME( head, tablename );
  static const sam::qname QN_testrequest    = QNAME( head, testrequest );

  // message types
  static const std::string admindescr   = "admindescr";
  static const std::string msg_alert    = "alert";
  static const std::string msg_bye      = "bye";
  static const std::string msg_logon    = "logon";
  static const std::string msg_request  = "request";
  static const std::string msg_response = "response";
  static const std::string tableclear   = "tableclear";
  static const std::string tabledescr   = "tabledescr";
  static const std::string tableupdate  = "tableupdate";
  static const std::string tablerowdel  = "tablerowdel";
  static const std::string heartbeat    = "heartbeat";

  // error codes
  static int const err_unknown          = 1;
  static int const err_admin_not_found  = 2;
  static int const err_no_table         = 3;
  static int const err_bad_command      = 4;
  static int const err_no_session       = 5;

  const char * error_code_str(int);

} // namespace id

} // namespace exio


#endif
