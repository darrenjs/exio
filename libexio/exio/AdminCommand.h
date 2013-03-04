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
#ifndef EXIO_ADMINCOMMAND_H
#define EXIO_ADMINCOMMAND_H

#include "exio/sam.h"
#include "exio/AdminSessionID.h"

#include <string>
#include <map>
#include <sstream>
#include <stdexcept>


namespace exio
{


  /* Helper functions for adding content to messages */
  void set_pending(sam::txMessage&, bool);
  bool has_pending(const sam::txMessage&);

  void add_rescode(sam::txMessage&, int code);  /* code=0 means OK */
  void add_restext(sam::txMessage&, const std::string&);

  void formatreply_simplelist(sam::txContainer& body,
                              const std::list<std::string>&,
                              const char* value_column_name = NULL);

  void formatreply_string(sam::txContainer& body, const std::string&);

  /* Functions within meta namespace are associate with meta-data content. */
  namespace meta
  {
    /* Build a column style attribute */
    sam::txContainer column_attr_style(
      const std::string& style_name,
      const std::string& background_color);

    /* Build a column command attribute */
    sam::txContainer column_attr_command(
      const std::string& command_name,
      std::map<std::string, std::string> args);
  }


// TODO: note that this class is being used.
class TableBuilder
{
  public:
    TableBuilder(sam::txContainer & body);

    // Provide list of table columns
    void set_columns(const std::vector<std::string>&);

    // Add (and serialise) row.  Each row must be associated with a unique
    // row-key.
    void add_row(const std::string& rowkey,
                 const std::vector<std::string>& values);

  private:
    TableBuilder(const TableBuilder&);
    TableBuilder& operator=(const TableBuilder&);

    sam::txContainer & m_tabledescr;
    sam::txContainer & m_tableupdate;

    size_t m_row_count;

    std::vector<std::string> m_columns;
};



class AdminError: public std::runtime_error
{
    int m_code;

  public:
    AdminError(int __code, const std::string& s)
      : std::runtime_error( s ),
        m_code(__code){};

    int code() const { return m_code; }
};


class AdminRequest
{
  public:

    sam::txMessage msg;
    SID id;
  private:
    std::vector< std::string > m_args;
  public:
    sam::txContainer * head;
    sam::txContainer * body;
    std::string reqseqno;

  public:
    AdminRequest();
    AdminRequest(const sam::txMessage& __req, SID __id);
    AdminRequest(const AdminRequest&);
    AdminRequest& operator=(const AdminRequest&);

    typedef std::vector< std::string > Args;
    const Args& args() const { return this->m_args; }

    /* Return the head.user field if found, otherwise NULL. */
    const sam::txField* find_user() const;

  private:
    void init_args_ptrs();
};


struct AdminResponse
{
    bool send;
    sam::txMessage msg;

    AdminResponse(const std::string& reqseqno);
    AdminResponse(const AdminResponse&);

    AdminResponse& operator=(const AdminResponse&);

    sam::txContainer& head();
    sam::txContainer& body();


    static AdminResponse error(const std::string& reqseqno,
                               int errorcode,
                               const std::string& errorstr);

    static AdminResponse error(const std::string& reqseqno,
                               int errorcode);

    /* Generate a response to indicate a success request, with no pending
     * replies. */
    static AdminResponse success(const std::string& reqseqno,
                                 const std::string& str = "",
                                 int code = 0);
  private:
    sam::txContainer * m_head;
    sam::txContainer * m_body;
};



class AdminCommand
{
  public:

    struct Callback
    {
        virtual AdminResponse invoke(AdminRequest& req) = 0;
        virtual ~Callback() {}
    };

  private:

    /* implementation class */
    struct _Target : public Callback
    {
        virtual AdminResponse invoke(AdminRequest& req) = 0;
        virtual _Target* clone() const = 0;
    };

    /* implementation class */
    struct _ObjectTarget : public _Target
    {
        Callback* m_obj;
        _ObjectTarget(Callback* __obj) : m_obj( __obj ){}

        AdminResponse invoke(AdminRequest& r)
        { return m_obj->invoke(r);}

        _Target* clone() const
        {
          return new _ObjectTarget(m_obj);
        }
    };

    /* implementation class */
    template <typename T>
    struct _MemberTarget : public _Target
    {
        typedef AdminResponse (T::*PMF)(AdminRequest&) ;

        PMF m_pmf;
        T*  m_obj;

        _MemberTarget( PMF pmf, T* obj )
          : m_pmf(pmf),
            m_obj(obj){ }

        virtual AdminResponse invoke(AdminRequest& r)
        {
          return (m_obj->*m_pmf)( r );
        }

        _Target* clone() const
        {
          return new _MemberTarget<T>(m_pmf, m_obj);
        }

    };


  public:

    static std::map<std::string, std::string> const EmptyAttrs;

    AdminCommand()
      : m_name(""),
        m_shorthelp(""),
        m_target(NULL)
    {
    }

    /**
     * Construct an AdminCommand instance that invokes a callback on an
     * object. That target object must inhert from AdminCommand::Callback.
     */
    AdminCommand(const std::string& name,
                 const std::string& help,
                 const std::string& longhelp,
                 AdminCommand::Callback * callback,
                 std::map<std::string,std::string> attrs = EmptyAttrs)
      : m_name(name),
        m_shorthelp(help),
        m_longhelp(longhelp),
        m_target( new _ObjectTarget(callback) ),
        m_attrs(attrs)
    {
    }

    /*
     * Construct an AdminCommand instance that invokes a callback to a method
     * on some arbitrary object.
     */
    template <typename T>
    AdminCommand(const std::string& __name,
                 const std::string& help,
                 const std::string& longhelp,
                 typename _MemberTarget<T>::PMF pmf,
                 T* obj,
                 std::map<std::string,std::string> attrs = EmptyAttrs)
      : m_name(__name),
        m_shorthelp(help),
        m_longhelp(longhelp),
        m_target(new _MemberTarget<T>( pmf, obj) ),
        m_attrs(attrs)

    {
    }

    AdminCommand(const AdminCommand&);
    AdminCommand& operator=(const AdminCommand&);

    ~AdminCommand();

    const std::string& name()      const { return m_name; }
    const std::string& shorthelp() const { return m_shorthelp; }
    const std::string& longhelp()  const { return m_longhelp; }

    AdminResponse invoke(AdminRequest&);

    // Admin attributes
    std::map<std::string,std::string>& attrs_() { return m_attrs; }
    const std::map<std::string,std::string>& attrs_() const { return m_attrs; }


  private:

    std::string m_name;
    std::string m_shorthelp;
    std::string m_longhelp;
    _Target * m_target;

    /* Admin attributes - used to allow for non-standard extensions to admin
     * commands. */
    std::map<std::string,std::string> m_attrs;
};



} // namespace qm

#endif
