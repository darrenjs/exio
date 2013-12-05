
#include "exio/AdminInterface.h"
#include "exio/MsgIDs.h"
#include "exio/Reactor.h"
#include "exio/Client.h"
#include "exio/ClientCallback.h"
#include "exio/AppSvc.h"

#include <iostream>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>


exio::ConsoleLogger logger(exio::ConsoleLogger::eStdout,
                           exio::ConsoleLogger::eAll,
                           true);
exio::AppSvc * g_appsvc = NULL;

//----------------------------------------------------------------------

class TestClient : public exio::ClientCallback
{
  public:
    TestClient(int fd,
               exio::LogService* logsvc)
      : m_fd(fd),
        m_logsvc(logsvc),
        m_cptr(NULL)
    {
      std::cout << "created TestClient for socket fd " << fd << "\n";
    }

    ~TestClient()
    {

      std::cout << "~TestClient\n";
      // TODO: one problem we have with Client is that release() cannot be
      // called during a callback.
      m_cptr->release();
      std::cout << "~TestClient -- done releasing\n";
      m_cptr = NULL;
    }

    void init(exio::Reactor* reactor)
    {
      exio::Client * c = new exio::Client(reactor, m_fd, m_logsvc, this);
      m_cptr = c;
      reactor->add_client( c );
    }

    /* Callbacks to be implemented in derived classes */
    virtual size_t process_input(exio::Client* cptr, const char* str, int s)
    {
      std::cout << "process_input for fd " << m_fd<< "\n";

      if (s and *str == 'q')
      {
        std::cout << "got 'q', so deleting self\n";


        std::cout << "cptr " << cptr << "\n";
        delete this;
      }

      return s;
    }


    virtual void process_close(exio::Client* cptr)
    {
      std::cout << "process_close for fd " << m_fd << "\n";
      std::cout << "client closed so deleting self\n";
      delete this;
    }


  private:
    int m_fd;
    exio::LogService* m_logsvc;
    exio::Client * m_cptr;
};

//----------------------------------------------------------------------

void die(const char* e)
{
  std::cout << e << "\n";
  exit( 1 );
}

//----------------------------------------------------------------------
int Bind(int sockfd,
         const struct sockaddr *addr,
         socklen_t addrlen)
{
  if (bind(sockfd, addr, addrlen) < 0)
  {
    int __errno = errno;

    std::ostringstream err;
    err << "bind() failed: errno " << __errno;
    throw std::runtime_error(err.str());
  }

  return 0;
}
//----------------------------------------------------------------------
/** Create an IPv4 TCP socket
 *
 * Returns a valid file descriptor, or throws if there was an error.
 */
int SocketIPv4()
{

  int socket_family = AF_INET;
  int socket_type = SOCK_STREAM;
  int protocol = 0;

  int fd = socket(socket_family, socket_type, protocol);
  int __errno = errno;

  if (fd < 0)
  {
    std::ostringstream err;
    err << "socket() failed: errno " << __errno;
    throw std::runtime_error(err.str());
  }

  return fd;
}

//----------------------------------------------------------------------
void Listen(int sockfd, int backlog)
{
  if (listen(sockfd, backlog) < 0)
  {
    int __errno = errno;
    std::ostringstream err;
    err << "listen() failed: " << __errno;
    throw std::runtime_error( err.str() );
  }
}
//----------------------------------------------------------------------
int create_listen_socket(int server_port)
{
  /* Create an IPv4 socket */
  int servfd = SocketIPv4();

  /* Specify socket address */
  sockaddr_in sockaddr;
  bzero(&sockaddr, sizeof(sockaddr));
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  sockaddr.sin_port  = htons( server_port );

  int optval = 1;
  setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, (&optval), sizeof(optval));

  /* Bind our socket to its a address */
  Bind(servfd,(struct sockaddr*) &sockaddr, sizeof(sockaddr) );

  /* Mark as a passive socket */
  Listen(servfd, 100);

  return servfd;
}
//----------------------------------------------------------------------
int accept_connections(int server_port)
{
  exio::Reactor* reactor = new exio::Reactor(g_appsvc->log());

  int sockfd = create_listen_socket(server_port);

  // set the server socket to non-blocking, because we are going to use poll
  int fdflags = fcntl(sockfd, F_GETFD);
  fcntl( sockfd, F_SETFD, fdflags bitor O_NONBLOCK);

  struct pollfd fds_array[1];
  nfds_t nfds = 1;

  int timeout = 10 * 1000;  /* milliseconds */

  std::cout << "accepting connections on port "<< server_port<<"\n";
  while ( true )
  {
    int fd = -1;
    sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(clientaddr);
    memset(&clientaddr, 0, sizeof(clientaddr));

    memset(&fds_array, 0, sizeof(fds_array));
    fds_array[0].fd     = sockfd;
    fds_array[0].events = POLLIN;

    int n = poll(fds_array, nfds, timeout); // will block

    if (n)
    {
      // a positive 'n' is number of structures with nonzero revents fields
      if (fds_array[0].revents bitand POLLIN)
      {
        if ( (fd = accept(sockfd, (struct sockaddr*) &clientaddr, &addrlen)) < 0)
        {
          die("error during accept");
        }

        std::cout << "got connection, fd "<< fd << "\n";
        TestClient * tc = new TestClient(fd, &logger);
        tc->init( reactor );
      }
      else
      {
        // TODO: error, we should never come here
      }
    }
    else if (n == 0)
    {
      /* timeout */
    }
    else
    {
      /* error condition */
      die("error during poll");
    }

  }


  return 0;
}
// TODO: create a server socket

// TODO: accept

// TODO:

//----------------------------------------------------------------------
int __main(int argc, char** argv)
{
  exio::Config config;
  g_appsvc = new exio::AppSvc(config, &logger);

  int port = -1;
  for (int i = 1; i < argc; ++i)
  {
    if ( strcmp(argv[i],"-p")==0 )
    {
      if ( ++i < argc)
      {
        port = atoi(argv[i]);
      }
      else die("missing PORT");
    }
  }

  if (port == -1) die("missing -p PORT");

  accept_connections( port );

  while(true)
  {
    sleep(60);
  }

  return 0;
}


//----------------------------------------------------------------------
int main(int argc, char** argv)
{
  try
  {
    return __main(argc, argv);
  }
  catch (const std::exception & e)
  {
    std::cout << "exception caught in main: " << e.what() << "\n";
  }
  catch (...)
  {
    std::cout << "exception caught in main: unknown";
  }

  return 1;
}
