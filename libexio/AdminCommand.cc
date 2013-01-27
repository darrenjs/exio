#include "exio/AdminCommand.h"
#include "exio/MsgIDs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


namespace exio {


std::map<std::string, std::string> const AdminCommand::EmptyAttrs
= std::map<std::string, std::string>();

namespace meta
{

//----------------------------------------------------------------------
/* Build a column style attribute */
 sam::txContainer column_attr_style(
  const std::string& style_name,
  const std::string& background_color)
{
  sam::txContainer container("style");

  container.put_field("style_name", style_name);
  container.put_field("bg", background_color);

  return container;
}

//----------------------------------------------------------------------
/* Build a column style attribute */
 sam::txContainer column_attr_command(
   const std::string& name,
   std::map<std::string, std::string> args)
{
  sam::txContainer container("command");

  container.put_field("name", name);

  sam::txContainer& args_container = container.put_child("args");

  for (std::map<std::string, std::string>::iterator iter = args.begin();
       iter != args.end(); ++iter)
  {
    args_container.put_field(iter->first, iter->second);
  }

  return container;
}

} // meta namespace

//----------------------------------------------------------------------
void set_pending(sam::txMessage& msg, bool p)
{
  static sam::qname qn_pending = QNAME( id::head, id::pending );

  if (p)
  {
    msg.root().put_field(qn_pending, id::True);
  }
  else
  {
    msg.root().put_field(qn_pending, id::False);
  }
}
//----------------------------------------------------------------------
bool has_pending(const sam::txMessage& msg)
{
  static sam::qname qn_pending = QNAME( id::head, id::pending );

  return msg.root().check_field(qn_pending, id::True);
}

//----------------------------------------------------------------------

void add_rescode(sam::txMessage& msg, int e)
{
  std::ostringstream os;
  os << e;
  msg.root().put_field(id::QN_rescode, os.str() );
}

//----------------------------------------------------------------------
void add_restext(sam::txMessage& msg, const std::string& str)
{
  msg.root().put_field(id::QN_restext, str );
}

//----------------------------------------------------------------------
void formatreply_string(sam::txContainer& body,
                        const std::string& value)
{
  body.put_field(id::resptype, id::text);
  sam::txContainer& txcData = body.put_child(id::respdata);
  txcData.put_field(id::text, value);
}
//----------------------------------------------------------------------
void formatreply_simplelist(sam::txContainer& body,
                            const std::list<std::string>& values,
                            const char* value_column_name)
{
  // note that our response type is a synthetic table
  body.put_field(id::resptype, id::table);
  body.put_field(id::synthetic, id::scalarlist);

  // message layout
  sam::txContainer & respdata    = body.put_child(id::respdata);
  sam::txContainer & tabledescr  = respdata.put_child(id::tabledescr);
  sam::txContainer & tableupdate = respdata.put_child(id::tableupdate);


  // columns
  sam::txContainer & columns = tabledescr.put_child(id::columns);
  columns.put_field(id::msgcount, "1");
//  columns.put_child(id::msg_prefix+"0").put_field(id::colname, id::row_key);

  if (value_column_name==NULL) value_column_name = id::value.c_str();

  columns.put_child(id::msg_prefix+"0").put_field(id::colname,
    value_column_name);

  // table rows
  char buf[50]; memset(buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf)-1, "%zu", values.size());
  tableupdate.put_field(id::rows, buf);
  size_t rowindex = 0;
  for (std::list<std::string>::const_iterator iter=values.begin();
       iter != values.end(); ++iter)
  {
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf)-1, "%zu", rowindex++);
    sam::txContainer & row = tableupdate.put_child(buf);
    row.put_field(id::row_key, buf);
    row.put_field(value_column_name, *iter);
  }

}
//======================================================================


//----------------------------------------------------------------------
TableBuilder::TableBuilder(sam::txContainer & body)
  : m_tabledescr(  body.put_child( QNAME(id::respdata, id::tabledescr)  )),
    m_tableupdate( body.put_child( QNAME(id::respdata, id::tableupdate) )),
    m_row_count(0)
{
  body.put_field(id::resptype, id::table);
}

//----------------------------------------------------------------------
void TableBuilder::set_columns(const std::vector<std::string>& cols)
{
  typedef std::vector<std::string>::const_iterator Iter;

  char buf[50]; memset(buf, 0, sizeof(buf));

  // container for holding columns
  sam::txContainer & columns = m_tabledescr.put_child(id::columns);
  columns.clear();

  // store the list of columns, for later use
  m_columns = cols;

  // generate: msgcount=NNN
  snprintf(buf, sizeof(buf)-1, "%zu", cols.size());
  columns.put_field(id::msgcount, buf);

  size_t index = 0;
  for (Iter it = cols.begin(); it != cols.end(); ++it, ++index)
  {
    // generate fieldname: msg_NNN
    snprintf(buf, sizeof(buf)-1, "%s%zu", id::msg_prefix.c_str(), index);

    // add field
    columns.put_child(buf).put_field(id::colname, *it);
  }
}

//----------------------------------------------------------------------
void TableBuilder::add_row(const std::string& rowkey,
                           const std::vector<std::string>& values)
{
  typedef std::vector<std::string>::const_iterator Iter;

  char buf[50]; memset(buf, 0, sizeof(buf));

  // update the count of rows
  m_row_count++;
  snprintf(buf, sizeof(buf)-1, "%zu",m_row_count);
  m_tableupdate.put_field(id::rows, buf);

  // generate field name for this new row
  snprintf(buf, sizeof(buf)-1, "%s%zu", id::row_prefix.c_str(), m_row_count-1);

  // obtain container for our new row
  sam::txContainer & row = m_tableupdate.put_child(buf);

  // Add the rowkey. We have to do this because we are constructing a table.
  // I.e., the whole data can be processed by code which builds a map from
  // rowkey to row-content.  If we do not provide this field, then the
  // ximon.gui will ignore the row.  Note though, we don't seem to have to add
  // the RowKey as an explicit column in the tabledescr section. Odd, yes, but
  // we hagve to try to maintain compatibilty with ximon.gui.
  row.put_field(id::row_key, rowkey);

  Iter coliter = m_columns.begin();
  Iter valiter = values.begin();
  while (coliter != m_columns.end() and valiter != values.end())
  {
    row.put_field(*coliter, *valiter);

    ++coliter;
    ++valiter;
  }

}


//======================================================================

AdminRequest::AdminRequest()
  : head(NULL),
    body(NULL)
{
}
//----------------------------------------------------------------------
AdminRequest::AdminRequest(const sam::txMessage& __req,
                           SID __id)
  : msg( __req ),
    id ( __id ),
    head(NULL),
    body(NULL)
{
  init_args_ptrs();
}
//----------------------------------------------------------------------
AdminRequest::AdminRequest(const AdminRequest& rhs)
  : msg(rhs.msg),
    head(NULL),
    body(NULL)
{
  init_args_ptrs();
}
//----------------------------------------------------------------------
AdminRequest& AdminRequest::operator=(const AdminRequest& rhs)
{
  this->msg = rhs.msg;
  init_args_ptrs();

  return *this;
}
//----------------------------------------------------------------------
void AdminRequest::init_args_ptrs()
{
  this->m_args.clear();
  this->head = NULL;
  this->body = NULL;


  const sam::txField* f_reqseqno = msg.root().find_field(id::QN_head_reqseqno);
  if (f_reqseqno) this->reqseqno = f_reqseqno->value();



  this->head = msg.root().find_child(id::head);
  this->body = msg.root().find_child(id::body);

  if (this->head == NULL)
  {
    std::ostringstream os;
    os << "missing required field: " << id::head;
    throw std::runtime_error( os.str() );
  }
  if (this->body == NULL)
  {
    std::ostringstream os;
    os << "missing required field: " << id::body;
    throw std::runtime_error( os.str() );
  }

  sam::txField * f_argv = body->find_field(id::args_COUNT);
  if (f_argv)
  {
    size_t argc = atoi( f_argv->value().c_str() );
    for (size_t i = 0; i < argc; ++i)
    {
      std::ostringstream msgstr;
      msgstr << id::args_prefix << i;

      sam::txField * f_argi = this->body->find_field(msgstr.str());
      if (!f_argi)
      {
        std::ostringstream os;
        os << "missing required field: " << msgstr.str();
        throw std::runtime_error( os.str() );
      }
      else
        this->m_args.push_back( f_argi->value() );
    } // for

  }
}

//----------------------------------------------------------------------

AdminResponse::AdminResponse(const std::string& reqseqno)
  : send(true),
    msg(id::msg_response),
    m_head( &msg.root().put_child(id::head) ),
    m_body( &msg.root().put_child(id::body) )
{
  msg.root().put_field(id::QN_head_reqseqno, reqseqno);
}

//----------------------------------------------------------------------
AdminResponse::AdminResponse(const AdminResponse& src)
  : send(src.send),
    msg(src.msg),
    m_head( &msg.root().put_child(id::head) ),
    m_body( &msg.root().put_child(id::body) )

{
}
//----------------------------------------------------------------------
AdminResponse& AdminResponse::operator=(const AdminResponse& src)
{
  this->send = src.send;
  this->msg = src.msg;
  this->m_head = &msg.root().put_child(id::head);
  this->m_body = &msg.root().put_child(id::body);

  return *this;
}
//----------------------------------------------------------------------
sam::txContainer& AdminResponse::head()
{
  return *m_head;
}
//----------------------------------------------------------------------
sam::txContainer& AdminResponse::body()
{
  return *m_body;
}
//----------------------------------------------------------------------
AdminResponse AdminResponse::success(const std::string& reqseqno,
                                     const std::string & str,
                                     int code)
{
  AdminResponse resp(reqseqno);

  exio::add_rescode(resp.msg, code);
  exio::set_pending(resp.msg, false);
  exio::formatreply_string(resp.body(), str);

  return resp;
}

//----------------------------------------------------------------------
// AdminResponse AdminResponse::immediateError(const std::string& reqseqno,
//                                                     const std::string & str,
//                                                     int code)
// {
//   AdminResponse resp(reqseqno);

//   AdminMsg::add_rescode(resp.msg, code);
//   AdminMsg::set_pending(resp.msg, false);
//   AdminMsg::formatreply_string(resp.body(), str);

//   return resp;
// }

//----------------------------------------------------------------------
AdminResponse AdminResponse::error(const std::string& reqseqno,
                                   int errorcode,
                                   const std::string& errorstr)
{
  AdminResponse resp(reqseqno);

  exio::add_rescode(resp.msg, errorcode);
  exio::set_pending(resp.msg, false);

  exio::formatreply_string(resp.body(), errorstr);

  return resp;
}
//----------------------------------------------------------------------
AdminResponse AdminResponse::error(const std::string& reqseqno,
                                   int errorcode)
{
  std::ostringstream os;
  os << "Admin failed, error code " << errorcode;
  return AdminResponse::error(reqseqno, errorcode, os.str());
}
//----------------------------------------------------------------------
// AdminResponse AdminResponse::successComplete(const std::string & str,
//                                                      int code)
// {
//   return AdminResponse(true,
//                            eReqSuccess,
//                            eNothingPending,
//                            code,
//                            str,
//                            false);
// }


//----------------------------------------------------------------------


/* Copy constructor */
AdminCommand::AdminCommand(const AdminCommand& rhs)
  : m_name(rhs.m_name),
    m_shorthelp(rhs.m_shorthelp),
    m_longhelp(rhs.m_longhelp),
    m_target( rhs.m_target->clone() ),
    m_attrs(rhs.m_attrs)
{
}


//----------------------------------------------------------------------
AdminCommand& AdminCommand::operator=(const AdminCommand& rhs)
{
  this->m_name       = rhs.m_name;
  this->m_shorthelp  = rhs.m_shorthelp;
  this->m_longhelp   = rhs.m_longhelp;

  delete this->m_target;
  this->m_target = rhs.m_target->clone();

  this->m_attrs = rhs.m_attrs;

  return *this;
}

//----------------------------------------------------------------------
AdminCommand::~AdminCommand()
{
  delete m_target;
}

//----------------------------------------------------------------------
AdminResponse AdminCommand::invoke(AdminRequest& req)
{
  if (not m_target) throw std::runtime_error("command not implemented");
  return m_target->invoke( req );
}

} // namespace qm
