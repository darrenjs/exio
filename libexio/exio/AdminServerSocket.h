#ifndef EXIO_ADMINSERVERSOCKET_H__
#define EXIO_ADMINSERVERSOCKET_H__

#include "thread.h"


namespace exio
{

class AdminInterfaceImpl;

class AdminServerSocket
{
  public:
    AdminServerSocket(AdminInterfaceImpl* ai);

    /* Start listening on the server socket. */
    void start();

  private:

    void create_listen_socket();

    /* Thread entry point for thread that will accept new socket
     * connections. */
    void accept_TEP();

  private:
    AdminServerSocket(const AdminServerSocket&);
    AdminServerSocket& operator=(const AdminServerSocket&);

  private:

    AdminInterfaceImpl* m_aii;
    int m_servfd;


//    qm::Thread m_acceptThr;
};

}

#endif