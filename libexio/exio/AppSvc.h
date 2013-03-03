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

    enum StreamType { eStdout, eStderr };

    enum Levels {eAll = 0,
                 eDebug,
                 eInfo,
                 eWarn,
                 eError,
                 eNone = 255};
    int level;

    ConsoleLogger(StreamType stream,
                  int __level) : level(__level), m_stream(stream) {}

    virtual void debug(const std::string& );
    virtual void info(const std::string&  );
    virtual void warn(const std::string&  );
    virtual void error(const std::string& );

    virtual bool want_debug() { return level <= eDebug; }
    virtual bool want_info()  { return level <= eInfo; }
    virtual bool want_warn()  { return level <= eWarn; }
    virtual bool want_error() { return level <= eError; }

  private:

    StreamType m_stream;
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
