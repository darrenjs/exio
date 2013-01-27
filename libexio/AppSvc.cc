#include "exio/AppSvc.h"

#include <iostream>

namespace exio {


AppSvc::AppSvc(Config config,
               LogService * logsvc)
  : m_config(config),
    m_logsvc(logsvc)
{
}

void ConsoleLogger::info(const std::string&  s)
{ std::cout << "info: " << s << "\n"; }
void ConsoleLogger::error(const std::string& s)
{ std::cout << "error: " << s << "\n"; }
void ConsoleLogger::warn(const std::string&  s)
{ std::cout << "warn: " << s << "\n"; }


} // namespace
