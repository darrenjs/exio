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
