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
#ifndef SAM_UTILS_H
#define SAM_UTILS_H

#include <string>
#include <vector>

namespace exio {
namespace utils {


  /*
   * want_empty_tokens==true  : include empty tokens, like strsep()
   * want_empty_tokens==false : exclude empty tokens, like strtok()
   */
  std::vector<std::string> tokenize(const char* src,
                                  char delim,
                                  bool want_empty_tokens);

  /* Convert integer to string */
  std::string to_str(int);

  /* wrapper for strerr */
  std::string strerror(int __errno);

  /* date-timestamp */
  std::string datetimestamp(time_t now_secs);


}} // namespace

#endif
