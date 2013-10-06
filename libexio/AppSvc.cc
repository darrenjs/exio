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

#include <iostream>

#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */


namespace exio {


//----------------------------------------------------------------------
AppSvc::AppSvc(Config config,
               LogService * logsvc)
  : m_config(config),
    m_logsvc(logsvc)
{
}
//----------------------------------------------------------------------
ConsoleLogger::ConsoleLogger(StreamType stream,
                             int __level,
                             bool incsource)
  : m_stream(stream),
    m_incsource(incsource),
    m_level(__level)
{
}
//----------------------------------------------------------------------
void ConsoleLogger::debug(const std::string&  s)
{
  int tid = syscall(SYS_gettid);
  std::ostream& os = (m_stream==eStdout)? std::cout : std::cerr;

  cpp11::lock_guard<cpp11::mutex> lock( m_mutex );
  os << "[" << tid << "] " << "debug: " << s << "\n";
}
//----------------------------------------------------------------------
void ConsoleLogger::info(const std::string&  s)
{
  int tid = syscall(SYS_gettid);
  std::ostream& os = (m_stream==eStdout)? std::cout : std::cerr;

  cpp11::lock_guard<cpp11::mutex> lock( m_mutex );
  os << "[" << tid << "] " << "info: " << s << "\n";
}
//----------------------------------------------------------------------
void ConsoleLogger::error(const std::string& s)
{
  int tid = syscall(SYS_gettid);
  std::ostream& os = (m_stream==eStdout)? std::cout : std::cerr;

  cpp11::lock_guard<cpp11::mutex> lock( m_mutex );
  os << "[" << tid << "] " << "error: " << s << "\n";
}
//----------------------------------------------------------------------
void ConsoleLogger::warn(const std::string&  s)
{
  int tid = syscall(SYS_gettid);
  std::ostream& os = (m_stream==eStdout)? std::cout : std::cerr;

  cpp11::lock_guard<cpp11::mutex> lock( m_mutex );
  os << "[" << tid << "] " << "warn: " << s << "\n";
}


} // namespace
