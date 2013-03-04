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

namespace exio {


AppSvc::AppSvc(Config config,
               LogService * logsvc)
  : m_config(config),
    m_logsvc(logsvc)
{
}

void ConsoleLogger::debug(const std::string&  s)
{
  if (m_stream==eStdout)
    std::cout << "debug: " << s << "\n";
  else
    std::cerr << "debug: " << s << "\n";
}
void ConsoleLogger::info(const std::string&  s)
{
  if (m_stream==eStdout)
    std::cout << "info: " << s << "\n";
  else
    std::cerr << "info: " << s << "\n";
}
void ConsoleLogger::error(const std::string& s)
{
  if (m_stream==eStdout)
    std::cout << "error: " << s << "\n";
  else
    std::cerr << "error: " << s << "\n";
}
void ConsoleLogger::warn(const std::string&  s)
{
  if (m_stream==eStdout)
    std::cout << "warn: " << s << "\n";
  else
    std::cerr << "warn: " << s << "\n";
}


} // namespace
