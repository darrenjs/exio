#ifndef EXIO_APPSVC_H
#define EXIO_APPSVC_H

#include <string>

namespace exio
{

struct Config
{
    std::string serviceid;

    // Port to listen on of a server socket is created
    int server_port;
};


class LogService
{
  public:
    virtual ~LogService() {}

    virtual void debug(const std::string&  ) {}
    virtual void info(const std::string&  ) {}
    virtual void warn(const std::string&  ) {}
    virtual void error(const std::string& ) {}

    virtual bool want_debug() { return false; }
    virtual bool want_info()  { return false; }
    virtual bool want_warn()  { return false; }
    virtual bool want_error() { return false; }

    virtual bool inc_source() { return false; }
};


class ConsoleLogger : public LogService
{
  public:

    enum Levels {eDebug=0x01,
                 eInfo =0x02,
                 eWarn =0x04,
                 eError=0x08,
                 eAll  =0xFF};
    int level;

    ConsoleLogger(int l = eInfo|eWarn|eError) : level(l){}

    virtual void info(const std::string&  );
    virtual void warn(const std::string&  );
    virtual void error(const std::string& );

    virtual bool want_info()  { return level bitand eInfo; }
    virtual bool want_warn()  { return level bitand eWarn; }
    virtual bool want_error() { return level bitand eError; }
};

class AppSvc
{
  public:
    AppSvc(Config, LogService *);

    const Config& conf() const  { return m_config; }
    LogService*    log()        { return m_logsvc; }

  private:

    Config       m_config;
    LogService * m_logsvc;
};


} // namespace



#endif
