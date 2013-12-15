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
#ifndef EXIO_APPSVC_H
#define EXIO_APPSVC_H

#include <string>

#define EXIO_NO_SERVER -1

namespace exio
{

struct Config
{
    std::string serviceid;

    // Port to listen, or EXIO_NO_SERVER to disable server socket
    int server_port;
};


class LogService
{
  public:
    virtual ~LogService() {}

    virtual void debug(const std::string& msg, const char* file, int ln) {}
    virtual void info(const std::string& msg,  const char* file, int ln) {}
    virtual void warn(const std::string& msg,  const char* file, int ln) {}
    virtual void error(const std::string& msg, const char* file, int ln) {}

    virtual bool want_debug() { return false; }
    virtual bool want_info()  { return false; }
    virtual bool want_warn()  { return false; }
    virtual bool want_error() { return false; }
};


class ConsoleLoggerImpl;
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

    ConsoleLogger(StreamType stream,
                  int level,
                  bool incsource=false);
    ~ConsoleLogger();

    virtual void debug(const std::string& msg, const char* file, int ln);
    virtual void info(const std::string& msg,  const char* file, int ln);
    virtual void warn(const std::string& msg,  const char* file, int ln);
    virtual void error(const std::string& msg, const char* file, int ln);

    virtual bool want_debug();
    virtual bool want_info();
    virtual bool want_warn();
    virtual bool want_error();

  private:
    void dolog(const char*, const std::string&, const char*, int);
    ConsoleLoggerImpl * m_impl;

  private:
    ConsoleLogger(const ConsoleLogger&);
    ConsoleLogger& operator=(const ConsoleLogger&);
};

class AppSvc
{
  public:
    AppSvc() : m_logsvc(NULL){}
    AppSvc(Config, LogService *);


    const Config& conf() const  { return m_config; }

    /* Can return null */
    LogService*    log()        { return m_logsvc; }

  private:

    Config       m_config;
    LogService * m_logsvc;
};


} // namespace



#endif
