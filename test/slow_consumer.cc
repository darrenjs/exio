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
#include "exio/AdminInterface.h"
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


std::string exec(const std::string & path,
                 const std::string & cmd,
                 const std::vector<std::string>& args,
                 int & retval,
                 const char* quotes)
{
  // TODO: see if we can capture stderr too. I tyhink it is just a case of
  // adding ampersand to the command

  std::ostringstream cmdss;

  if (not path.empty()) cmdss << path << "/";
  cmdss << cmd;

  for (std::vector<std::string>::const_iterator it = args.begin();
       it != args.end(); ++it)
  {
    cmdss << " " << quotes << *it << quotes;
  }

  FILE * pipe = popen(cmdss.str().c_str(), "r");

  if (pipe == NULL)
  {
    retval = -1;
    return "popen error";
  }

  std::ostringstream os;
  try
  {
    char temp[4096];
    while (!feof(pipe) and !ferror(pipe))
    {
      if (fgets(temp, sizeof(temp), pipe) != NULL)
      {
        os << temp;
      }
    }
  }
  catch (...)
  {
    /* prevent exceptions from unwinding the stack, because pclose() must be
     * called.  */
    pclose(pipe);
    retval = -1;
    return "failure when reading from popen pipe";
  }

  retval = pclose(pipe);
  if (retval > 0 ) retval = retval >> 8;
  //_P_(retval);
  //_INFO_("Out:" << os.str());

  return os.str();
}



/*
 * Taken from Unix Network Programming, section 3.8.  Look up sock_ntop.c for
 * full implementation of this function.
 */
std::string sock_ntop(const struct sockaddr * sa,
                      socklen_t salen)
{

  char str[128];

  switch (sa->sa_family)
  {
    case AF_INET:
    {
      struct sockaddr_in * sin = (struct sockaddr_in*) sa;
      if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
        return "";

      std::stringstream os;
      os << str << ":" << ntohs(sin->sin_port);
      return os.str();
    }
    default:
      return "";
  }
}


//----------------------------------------------------------------------
// TODO: put into a library
std::string peername(int fd)
{
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);
  memset(&addr,0,sizeof(addr));

  int __e =  getpeername(fd, (struct sockaddr *) &addr, &addrlen);

  if (!__e)
    return sock_ntop((const struct sockaddr *)&addr, addrlen);
  else
    return "unknown";
}
//----------------------------------------------------------------------
// TODO: put into a library
std::string sockname(int fd)
{
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);
  memset(&addr,0,sizeof(addr));

  int __e =  getsockname(fd, (struct sockaddr *) &addr, &addrlen);

  if (!__e)
    return sock_ntop((const struct sockaddr *)&addr, addrlen);
  else
    return "unknown";
}



struct ProgramOptions
{
    std::string addr;
    std::string port;
} po;
pid_t g_pid;

class AdminListener : public exio::AdminSessionListener
{
  public:

    AdminListener(int fd);

    /* inherited from AdminSession::Listener */
    virtual void session_msg_received(const sam::txMessage&,
                                      exio::AdminSession&);
    virtual void session_closed(exio::AdminSession&);


    void wait_for_session_closure();

    void show_unsol_messages(bool b) { m_show_unsol = b; }

  private:

    void trigger_close();

    cpp11::condition_variable m_convar;
    cpp11::mutex m_mutex;
    bool m_session_open;
    bool m_show_unsol;
    int m_fd;

    std::string m_peername;
    std::string m_sockname;
};
//----------------------------------------------------------------------
AdminListener::AdminListener(int fd)
  : m_session_open( true ),
    m_show_unsol(false),
    m_fd(fd)
{
  m_peername = peername(fd);
  m_sockname = sockname(fd);
}
//----------------------------------------------------------------------
void AdminListener::trigger_close()
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
  m_session_open = false;
  m_convar.notify_one();
}
//----------------------------------------------------------------------

void AdminListener::session_msg_received(const sam::txMessage& msg,
                                         exio::AdminSession& session)
{
  sam::MessageFormatter fmt(true);
  fmt.format( msg, std::cout );
  std::cout << "\n";
  std::cout.flush();

  std::cout << "sleeping...\n";

  std::cout << "this process " << g_pid << " owns tcp: " <<  m_sockname << " --> " << m_peername
            << " (local / foreign)\n\n";

  std::ostringstream cmdstr;

  cmdstr  << "netstat -n"
          << "|\\grep tcp"
          << "|\\grep " << m_sockname
          << "|\\grep " << m_peername;

  std::cout << "Proto Recv-Q Send-Q Local Address           Foreign Address\n";
  std::vector<std::string> args;
  int retval;
  std::string results = exec("", cmdstr.str(), args, retval, NULL);
  std::cout << results << "\n";

  sleep(10);
}

//----------------------------------------------------------------------
void AdminListener::session_closed(exio::AdminSession& /*session*/)
{
  cpp11::lock_guard<cpp11::mutex> guard( m_mutex );
  m_session_open = false;
  m_convar.notify_one();
}

//----------------------------------------------------------------------
void AdminListener::wait_for_session_closure()
{
  cpp11::unique_lock<cpp11::mutex> lock( m_mutex );
  while ( m_session_open ) { m_convar.wait(lock); }
}
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

void process_cmd_line(int argc, char** argv)
{
  for (int i = 1; i < argc; ++i)
  {

    if (po.addr.empty())
    {
      po.addr=argv[i];
    }
    else if (po.port.empty())
    {
      po.port=argv[i];
    }
  }

  if (po.addr.empty() or po.port.empty())
    die("please specify address and port");
}



//----------------------------------------------------------------------
int main(const int argc, char** argv)
{
  g_pid = getpid();

  /*
    NEXT: for the admin io session, can we find the local socket?

  */

  try
  {
    process_cmd_line(argc,argv);


    // define application services required by exio objects
    exio::Config config;

    // TODO: allow the error level to be specified by config

    exio::ConsoleLogger::Levels loglevel = exio::ConsoleLogger::eNone;

    exio::ConsoleLogger logger(exio::ConsoleLogger::eStderr, loglevel);

    exio::ConsoleLogger* logptr = &logger;

    exio::AppSvc appsvc(config, &logger);

    /* Create a socket and connect */

    int fd = connect_ipv4(po.addr.c_str(),
                          po.port.c_str());
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

    AdminListener listener(fd);

    exio::AdminInterface * ai = new exio::AdminInterface(config, &logger);
    ai->start();

    exio::AdminSession adminsession(appsvc, fd, &listener, 1);

    // begin session io
    ai->register_ext_session( &adminsession );

    // Generate a logon message

    // TODO: once the ximon is able to send logon messages first, then we won't
    // do this until a logon has been received from the remote.
    std::string serviceid = build_service_id(argv[0]);
    sam::txMessage logon(exio::id::msg_logon);
    logon.root().put_field(exio::id::QN_serviceid, serviceid);
    logon.root().put_field(exio::id::QN_head_user, build_user_id());

    adminsession.enqueueToSend( logon );

    listener.wait_for_session_closure();

    delete ai;

    exit( 0 );
  }
  catch (const std::exception& e)
  {
    std::cout << e.what() << "\n";
    return 1;
  }

}

