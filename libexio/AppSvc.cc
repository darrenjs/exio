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
#include "exio/AppSvc.h"

#include "mutex.h"

#include <iostream>
#include <sstream>

#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/time.h>
#include <stdio.h>

namespace exio {

struct ConsoleLoggerImpl
{
    ConsoleLoggerImpl(ConsoleLogger::StreamType __stream,
                      int __level,
                      bool __incsource)
      : stream(__stream),
        incsource(__incsource),
        level(__level)
    {
    }
    ConsoleLogger::StreamType stream;
    bool incsource;
    int   level;
    cpp11::mutex  mutex;
};

//----------------------------------------------------------------------
AppSvc::AppSvc(Config config,
               LogService * logsvc)
  : m_config(config),
    m_logsvc(logsvc)
{
}
//----------------------------------------------------------------------
ConsoleLogger::ConsoleLogger(StreamType __stream,
                             int __level,
                             bool __incsource)
  : m_impl(new ConsoleLoggerImpl(__stream, __level, __incsource))
{
}
//----------------------------------------------------------------------
ConsoleLogger::~ConsoleLogger()
{
  delete m_impl;
}
//----------------------------------------------------------------------
void ConsoleLogger::debug(const std::string&  s, const char* file, int ln)
{
  dolog("DEBUG", s, file, ln);
}
//----------------------------------------------------------------------
void ConsoleLogger::info(const std::string& s, const char* file, int ln)
{
  dolog("INFO", s, file, ln);
}
//----------------------------------------------------------------------
void ConsoleLogger::error(const std::string& s, const char* file, int ln)
{
  dolog("ERROR", s, file, ln);
}
//----------------------------------------------------------------------
void ConsoleLogger::warn(const std::string&  s, const char* file, int ln)
{
  dolog("WARN", s, file, ln);
}
//----------------------------------------------------------------------
void ConsoleLogger::ConsoleLogger::dolog(const char* level,
                          const std::string& s,
                          const char* file,
                          int ln)
{
  int tid = syscall(SYS_gettid);
  std::ostringstream oss;

  // get current time
  timeval now;
  struct timezone * const tz = NULL; /* not used on Linux */
  gettimeofday(&now, tz);

  // break time down into parts
  struct tm parts;
  localtime_r(&now.tv_sec, &parts);

  // build timestamp
  char timestamp[30];
  snprintf(timestamp,
           sizeof(timestamp),
           "%02d/%02d/%02d-%02d:%02d:%02d.%06lu ",
           parts.tm_year + 1900,
           parts.tm_mon + 1,
           parts.tm_mday,
           parts.tm_hour,parts.tm_min,parts.tm_sec,now.tv_usec);


  oss << timestamp;
  oss << "[" << tid << "] ";
  if (m_impl->incsource)
  {
    oss << "(" << file << ":" << ln << ") ";
  }
  oss << level << " " << s << "\n";

  {
    cpp11::lock_guard<cpp11::mutex> lock( m_impl->mutex );
    if (m_impl->stream==eStdout)
      std::cout << oss.str();
    else
      std::cerr << oss.str();
  }
}

bool ConsoleLogger::want_debug() { return m_impl->level <= eDebug; }
bool ConsoleLogger::want_info()  { return m_impl->level <= eInfo;  }
bool ConsoleLogger::want_warn()  { return m_impl->level <= eWarn;  }
bool ConsoleLogger::want_error() { return m_impl->level <= eError; }

} // namespace
