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
#include "exio/AdminSession.h"
#include "exio/AdminCommand.h"
#include "exio/AppSvc.h"
#include "exio/sam.h"
#include "exio/MsgIDs.h"
#include "exio/Logger.h"
#include "config.h"

#include "condition_variable.h"
#include "mutex.h"

#include <sstream>
#include <stdexcept>
#include <iostream>

#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h> /* for getopt_long; standard getopt is in unistd.h */

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <sys/prctl.h>

struct ProgramOptions
{
    std::string addr;
    std::string port;
    std::string cmd;
    std::list< std::string > cmdargs;

    int verbose; // count of '-v'

    ProgramOptions()
      : verbose(0)
    {
    }
} program_options;


class AdminListener : public exio::AdminSession::Listener
{
  public:

    AdminListener();

    /* inherited from AdminSession::Listener */
    virtual void session_msg_received(const sam::txMessage&,
                                      exio::AdminSession&);
    virtual void session_closed(exio::AdminSession&);


    int wait_for_session_closure();

    void show_unsol_messages(bool b) { m_show_unsol = b; }

  private:

    void trigger_close(int);

    void handle_table_response(const sam::txContainer* body);


    void handle_reponse(const sam::txMessage& msg,
                        exio::AdminSession& session);


    cpp11::condition_variable m_convar;
    cpp11::mutex m_mutex;
    bool m_session_open;
    bool m_show_unsol;
    int  m_retval;
};
//----------------------------------------------------------------------
AdminListener::AdminListener()
  : m_session_open( true ),
    m_show_unsol(false),
    m_retval(0)
{
}
//----------------------------------------------------------------------
void AdminListener::trigger_close(int retval)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
  m_session_open = false;
  m_retval = retval;
  m_convar.notify_one();
}
//----------------------------------------------------------------------
void AdminListener::handle_table_response(const sam::txContainer* body)
{
  /* handler for: body.resptype=table */

  typedef std::vector<std::string>::const_iterator VSIter;

  bool synthetic = (body->find_field(exio::id::synthetic) != NULL);

  const sam::txContainer * respdata    = body->find_child(exio::id::respdata);
  if (!respdata) return;

  const sam::txContainer * tabledescr  = respdata->find_child(exio::id::tabledescr);
  const sam::txContainer * tableupdate = respdata->find_child(exio::id::tableupdate);

  std::vector< std::string > columns;
  if (tabledescr)
  {
    const sam::txContainer * c_columns = tabledescr->find_child(exio::id::columns);
    if (c_columns)
    {
      size_t msgcount = 0;
      const sam::txField * fmsgcount = c_columns->find_field(exio::id::msgcount);
      if (fmsgcount) msgcount = atoi( fmsgcount->value().c_str() );

      for (size_t msgn = 0; msgn < msgcount; ++msgn)
      {
        std::ostringstream os;
        os << exio::id::msg_prefix << msgn;
        const sam::txContainer * c_msgn = c_columns->find_child(os.str());
        if (c_msgn)
        {
          const sam::txField * f_colname
            = c_msgn->find_field(exio::id::colname);
          if (f_colname)
          {
            if (synthetic and f_colname->value() == exio::id::row_key)
              continue;
            columns.push_back( f_colname->value() );
          }
        }
      }
    }
  }

  if (!columns.empty() and !synthetic)
  {
    for (VSIter it = columns.begin(); it != columns.end(); ++it)
    {
      if (it != columns.begin()) std::cout << ", ";
      std::cout << *it;
    }
    std::cout << "\n";
  }


  if (tableupdate)
  {
    size_t rowcount = 0;
    const sam::txField * f = tableupdate->find_field(exio::id::rows);
    if (f) rowcount = atoi( f->value().c_str() );

    for (size_t r = 0; r < rowcount; ++r)
    {
      std::ostringstream os;
      os << exio::id::row_prefix << r;
      const sam::txContainer * child  = tableupdate->find_child(os.str());
      if (!child) continue;

      bool usedelim = false;

      // iterate over know columns
      for (VSIter it = columns.begin(); it != columns.end(); ++it)
      {
        if (usedelim) std::cout << ", "; else usedelim=true;
        const sam::txField * f_ptr = child->find_field( *it );
        if (f_ptr) std::cout << f_ptr->value();
        else std::cout << "";
      }

      // // iterate over all the fields
      // for (sam::FieldMap::const_iterator f = child->field_begin();
      //     f != child->field_end(); ++f)
      // {
      //   if (f->first == exio::id::row_key) continue;
      //   if (usedelim) std::cout << ", "; else usedelim=true;
      //   std::cout << f->second->value();
      // }
      std::cout << "\n";
    }
  }

}

//----------------------------------------------------------------------
void AdminListener::handle_reponse(const sam::txMessage& msg,
                                   exio::AdminSession& session)
{
  const sam::txField* rescode = msg.root().find_field(exio::id::QN_rescode);
  const sam::txField* restext = msg.root().find_field(exio::id::QN_restext);

  int retval = (rescode)? atoi( rescode->value().c_str() ) : -1;

  if (retval == 0)
  {
//    std::cout << retval;
//    std::cout << " OK, ";  // Inspired by ZX Spectrum format!!! ;-)
  }
  else
  {
    std::cout << retval;
    std::cout << " FAIL, ";
  }

  if (restext) std::cout << restext->value() << "\n"; // terminate the response summary

  /* process head */
  const sam::txContainer * body = msg.root().find_child("body");

  /* process body
   *
   * Here we need to identify where in the message the data can be
   * found. EXIO supports several data-structures, so each of them must be
   * supported here.
   */
  if (body and body->check_field(exio::id::resptype, exio::id::table))
  {
    handle_table_response( body );
  }
  else if (body and body->check_field(exio::id::resptype, exio::id::text))
  {
    const sam::txContainer * respdata =
      body->find_child(exio::id::respdata);

    if (respdata)
    {
      const sam::txField* text = respdata->find_field(exio::id::text);
      if (text != NULL and text->value().size() )
      {
        std::cout << text->value() << "\n";
      }
    }
  }
  else
  {
    std::cout << "NO FORMAT\n";
  }
  // {
  //   const sam::txContainer * data = NULL;
  //   if (body) data = body->find_child("data");
  //   if (data)
  //   {
  //     for( sam::ChildMap::const_iterator iter = data->child_begin();
  //          iter != data->child_end(); ++iter)
  //     {
  //       const sam::txField* value  = iter->second->find_field("value");
  //       const sam::txField* format = iter->second->find_field("format");

  //       if (format and value and (format->value() == "scalar"))
  //       {
  //         std::cout << value->value() << "\n";
  //       }
  //     }
  //   }
  // }

  if ( not exio::has_pending(msg) )
  {
    trigger_close(retval);
    return;
  }
}

//----------------------------------------------------------------------
void AdminListener::session_msg_received(const sam::txMessage& msg,
                                         exio::AdminSession& session)
{

  if (program_options.verbose > 2)
  {
    // if we are here, then just dump the message to cout
    sam::MessageFormatter fmt(true);
    std::cout <<"MESSAGE: ";
    fmt.format( msg, std::cout );
    std::cout << "\n";
    std::cout.flush();
  }

  // Does this look like a bye ?
  if (msg.type() == exio::id::msg_bye )
  {
    trigger_close(0);
    return;
  }

  // Does this look like a response to a request?
  if (msg.root().name() == exio::id::msg_response )
  {
    handle_reponse(msg, session);
    return;
  }

  // skip some expected message types
  if ((msg.root().name() == exio::id::msg_logon) or
      (msg.root().name() == exio::id::heartbeat) )
  {
    return;
  }


  /* If we are here, it looks like an unsoliticed message, which we may want
   * to display. */
  if (m_show_unsol)
  {
    sam::MessageFormatter fmt(true);
    fmt.format( msg, std::cout );
    std::cout << "\n";
    std::cout.flush();
  }
}

//----------------------------------------------------------------------
void AdminListener::session_closed(exio::AdminSession& /*session*/)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
  m_session_open = false;
  m_convar.notify_one();
}

//----------------------------------------------------------------------
int AdminListener::wait_for_session_closure()
{
  cpp11::unique_lock<cpp11::mutex> lock( m_mutex );
  while ( m_session_open ) { m_convar.wait(lock); }

  return m_retval;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------




void die(const char* e)
{
  std::cout << e << "\n";
  exit( 1 );
}


// TODO: EASY: replace references to "telnet"
/* NOTE
 *
 * This function has been reused almost verbatim from the source code for the
 * linux telnet program (inetutils-1.8).
 *
 */
int connect_ipv4(const char* hostp, const char* portp)
{
  struct hostent *host = 0;
  struct sockaddr_in sin;
  struct servent *sp = 0;
  in_addr_t temp;


  /* clear the socket address prior to use */
  memset ((char *) &sin, 0, sizeof (sin));

  /* First try to convert from a dotted notation.  If that doesn't work, then
   * we will need to attempt a hostname lookup. */
  temp = inet_addr (hostp);
  if (temp != (in_addr_t) - 1)
  {
    sin.sin_addr.s_addr = temp;
    sin.sin_family = AF_INET;
  }
  else
  {
    // Okay, address must be a hostname, so try to convert
    host = gethostbyname (hostp);
    if (host)
	{
	  sin.sin_family = host->h_addrtype;
	  memmove (&sin.sin_addr, host->h_addr_list[0], host->h_length);
	}
    else
	{
	  printf ("Can't lookup hostname %s\n", hostp);
	  return 0;
	}
  }

  /* Try to find a port value.  This will come from either parsing a numerical
   * value, eg "33333", or taking a text string and assuming it is a known tcp
   * service. */
  sin.sin_port = atoi (portp);
  if (sin.sin_port == 0)
  {
    sp = getservbyname (portp, "tcp");  // "tcp" here refers to the protocol
    if (sp == 0)
	{
	  printf ("tcp/%s: unknown service\n", portp);
	  return 0;
	}
    sin.sin_port = sp->s_port;
  }
  else
    sin.sin_port = htons (sin.sin_port);

//  printf ("Trying %s...\n", inet_ntoa (sin.sin_addr));

  int connected = 0;
  int net;
  do
  {
    net = socket (AF_INET, SOCK_STREAM, 0);
    if (net < 0)
    {
      perror ("admin: socket");
      return 0;
    }

# if defined(IPPROTO_IP) && defined(IP_TOS)
    {
#  ifdef IPTOS_LOWDELAY
      const int tos = IPTOS_LOWDELAY;
#  else
      const int tos = 0x10;
#  endif

      int err = setsockopt (net, IPPROTO_IP, IP_TOS,
                            (char *) &tos, sizeof (tos));
      if (err < 0 && errno != ENOPROTOOPT)
        perror ("admin: setsockopt (IP_TOS) (ignored)");
    }
# endif	/* defined(IPPROTO_IP) && defined(IP_TOS) */

    /* Now try to connect */
    if (connect (net, (struct sockaddr *) &sin, sizeof (sin)) < 0)
    {
      if (host && host->h_addr_list[1])
      {
        int oerrno = errno;

        fprintf (stderr, "admin: connect to address %s: ",
                 inet_ntoa (sin.sin_addr));
        errno = oerrno;
        perror ((char *) 0);
        host->h_addr_list++;
        memmove ((caddr_t) & sin.sin_addr,
                 host->h_addr_list[0], host->h_length);
        close (net);
        continue;
      }
      perror ("admin: Unable to connect to remote host");
      return 0;
    }
    connected++;
  }
  while (connected == 0);

  return net;
}


void add_arguments(sam::txContainer& c,
                   const std::list<std::string>&  args)
{
  std::ostringstream args_COUNT;
  args_COUNT << args.size();
  c.put_field("args_COUNT", args_COUNT.str());

  // now for each individual argument
  size_t i = 0;
  for (std::list<std::string>::const_iterator iter=args.begin();
       iter != args.end(); ++iter)
  {
    std::ostringstream args_n_fieldname;
    args_n_fieldname << "args_" << i++;
    c.put_field(args_n_fieldname.str(), *iter);
  }
}

std::string build_user_id()
{
  char username[256];
  memset(username, 0, sizeof(username));
  if (getlogin_r(username, sizeof(username)) != 0)
    strcpy(username, "unknown");
  username[sizeof(username)-1] = '\0';

  return username;
}

std::string build_service_id(const char* argv_0)
{
  char hostname[256];

  memset(hostname, 0, sizeof(hostname));
  // TODO: need to understand set-user-ID because there is another call: cuserid


  if (gethostname(hostname, sizeof(hostname)) != 0)
    strcpy(hostname, "unknown");
  hostname[sizeof(hostname)-1] = '\0';

  pid_t pid = getpid();

  char procname[256];
  memset(procname, 0, sizeof(procname));

#ifdef PR_GET_NAME
  if (prctl(PR_GET_NAME, &procname, 0, 0, 0))
    strcpy(procname, "unknown");
#else
  strcpy(procname, "admin");
#endif

  procname[sizeof(procname)-1] = '\0';


  std::ostringstream os;
  os
    << pid << "/"
    << procname << ":"
    << build_user_id() << "@"
    << hostname;
  return os.str();
}


//----------------------------------------------------------------------
void usage()
{
  std::cout << "Usage: admin [OPTION]... ADDRESS PORT [COMMAND] [ARG]...\n";
  std::cout << "exio admin command-line client, version " PACKAGE_VERSION "\n\n";

  std::cout << "Options:\n\n";
  std::cout << "  -v\tlog exio problems; repeat twice for info, thrice for debug\n";
}

//----------------------------------------------------------------------

//----------------------------------------------------------------------
void process_cmd_line(int argc, char** argv)
{

/*
          struct option {
               const char *name;
               int         has_arg;
               int        *flag;
               int         val;
           };
*/

//  int digit_optind = 0;
  static struct option longopts[] = {
    {"help", no_argument, 0, 'h'},
    {NULL, 0, NULL, 0}
  };



  while (true)
  {
    /* "optind" is the index of the next element to be processed in argv.  It
       is defined in the getopts header, and the system initializes this value
       to 1.  The caller can reset it to 1 to restart scanning of the same
       argv, or when scanning a new argument vector. */

    // take a copy to remember value for after return from getopt_long()
    int this_option_optind = optind ? optind : 1;
    int longindex = 0;

      // 'c' is the option character returned
    int c = getopt_long(argc, argv,
                        "hv",
                        longopts, &longindex);

    if (c == -1) break;

    // _P_( c );
    // _P_(this_option_optind);
    // _P_(optind);

    switch(c)
    {
      case 'v' : program_options.verbose++; break;
      case 'h' : usage(); exit(0);
      case '?' : {
        std::cout << "invalid option: '"
                  << argv[this_option_optind]<<"'\n";
        exit(1);
      }
      default:
      {
        std::cout << "getopt_long() returned (dec) " << (unsigned int)(c) << "\n";
          exit(1);
      }

    }

  }

  if (optind < argc) program_options.addr = argv[optind++];
  if (optind < argc) program_options.port = argv[optind++];
  if (optind < argc) program_options.cmd  = argv[optind++];
  while (optind < argc) program_options.cmdargs.push_back(argv[optind++]);

  if (program_options.addr.empty() or program_options.addr.empty())
    die("please specify address and port");

}

//----------------------------------------------------------------------
int main(const int argc, char** argv)
{
  try
  {
    process_cmd_line(argc,argv);


    // define application services required by exio objects
    exio::Config config;

    // TODO: allow the error level to be specified by config

    exio::ConsoleLogger::Levels loglevel = exio::ConsoleLogger::eNone;
    if (program_options.verbose > 0 ) loglevel = exio::ConsoleLogger::eWarn;
    if (program_options.verbose > 1 ) loglevel = exio::ConsoleLogger::eInfo;
    if (program_options.verbose > 2 ) loglevel = exio::ConsoleLogger::eAll;

    exio::ConsoleLogger logger(exio::ConsoleLogger::eStderr, loglevel);

    exio::ConsoleLogger* logptr = &logger;

    exio::AppSvc appsvc(config, &logger);

    /* Create a socket and connect */

    int fd = connect_ipv4(program_options.addr.c_str(),
                          program_options.port.c_str());
    if (!fd) return 1;

#if defined (IP_TOS) && defined (IPTOS_LOWDELAY) && defined (IPPROTO_IP)
    /* To  minimize delays for interactive traffic.  */
    {
      int tos = IPTOS_LOWDELAY;
      if (setsockopt (fd, IPPROTO_IP, IP_TOS,
                      (char *) &tos, sizeof (int)) < 0)
      {
        int __errno = errno;
        _WARN_(logptr, "setsockopt (IP_TOS): "
               << __errno); // TODO: add strerr
      }
    }
#endif

#ifdef SO_KEEPALIVE
        /* Set keepalives on the socket to detect dropped connections. */
        {
          int keepalive = 1;
          if (setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE,
                          (char *) &keepalive, sizeof (keepalive)) < 0)
          {
            int __errno = errno;
            _WARN_(logptr, "setsockopt (SO_KEEPALIVE): "
                   << __errno); // TODO: add strerr
          }
        }
#endif




    // TODO: build up an admin service ID

    AdminListener listener;

    if (program_options.cmd.empty()) listener.show_unsol_messages(true);

    exio::AdminSession adminsession(appsvc, fd, &listener, 1);

    // Generate a logon message

    // TODO: once the ximon is able to send logon messages first, then we won't
    // do this until a logon has been received from the remote.
    std::string serviceid = build_service_id(argv[0]);
    sam::txMessage logon(exio::id::msg_logon);
    logon.root().put_field(exio::id::QN_serviceid, serviceid);
    logon.root().put_field(exio::id::QN_head_user, build_user_id());

    if (not program_options.cmd.empty())
    {
      /* because we are going to send a command, lets include the
       * no-automatic-subscription flag in the logon*/
      logon.root().put_field(exio::id::QN_noautosub,
                             exio::id::True);

      /* We are sending a command. By default, indicate to the server that we
       * want them to close our connection once the adim command completes. */
      logon.root().put_field(QNAME(exio::id::head, exio::id::autoclose),
                                   exio::id::True);
    }
    adminsession.enqueueToSend( logon );


    if ( not program_options.cmd.empty() )
    {
      /*
       * Generate a message to represent a command request
       */
      sam::txMessage msg(exio::id::msg_request);

      // head
      msg.root().put_field(exio::id::QN_command, program_options.cmd);
      sam::txContainer& head = msg.root().put_child(exio::id::head);

      std::ostringstream os;
      os << program_options.addr << ":" << program_options.port;
      head.put_field(exio::id::dest, os.str());

      head.put_field(exio::id::source, serviceid);
      head.put_field(exio::id::user, build_user_id());
      head.put_field(exio::id::reqseqno, "0");

      // body
      sam::txContainer& body = msg.root().put_child(exio::id::body);

      // for legacy reasons, we also have to place the command into the messge
      // body
      body.put_field(exio::id::command, program_options.cmd);

      add_arguments(body, program_options.cmdargs);

      adminsession.enqueueToSend( msg );
    }


    return listener.wait_for_session_closure();
  }
  catch (const std::exception& e)
  {
    std::cout << e.what() << "\n";
    return 1;
  }

}

