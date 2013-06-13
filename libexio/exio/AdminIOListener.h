#ifndef EXIO_ADMINIOLISTENER_H
#define EXIO_ADMINIOLISTENER_H

namespace sam{
class txMessage;
}

namespace exio {

class AdminIOListener
{
  public:
    virtual ~AdminIOListener() {}
    virtual void io_onmsg(const sam::txMessage& src) = 0;
    virtual void io_closed() = 0;
};

} // namespace exio

#endif
