/** Copyright (C) 2004 Darren Smith
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License as
 *    published by the Free Software Foundation; either version 2 of the
 *    License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *    02111-1307 USA
 */

#ifndef _LOGGER_H_
#define _LOGGER_H_

#ifndef _CPP_IOSTREAM
#include <iostream>
#endif

#define STRINGIZE2(s) #s
#define STRINGIFY(s) STRINGIZE2(s)

// Useful macro for use in call to Exception constructors
#define _FL_  __FILE__, __LINE__

// Useful macro for getting a string of the file/line location
#define _HERE_  "(" __FILE__ ":" STRINGIFY( __LINE__ )  ")"

#define _P_( X ) \
std::cout << "DEBUG: " << # X << "=" << X << " (" << __FILE__ << ":" << __LINE__ << ")\n"

#define _TR_ \
std::cout << "(" << __FILE__ << ":" << __LINE__ << ")\n"

/* Each of these is functionally similar, but offer different trace-roles. */

#define _TODO_( X ) \
std::cout << "TODO: " << X << " (" << __FILE__ << ":" << __LINE__ << ")\n"

#define _INFO_( X ) \
std::cout << "INFO: " << X << " (" << __FILE__ << ":" << __LINE__ << ")\n"

#define _WARN_( X ) \
std::cout << "WARNING: " << X << " (" << __FILE__ << ":" << __LINE__ << ")\n"

#define _ERROR_( X ) \
std::cout << "ERROR: " << X << " (" << __FILE__ << ":" << __LINE__ << ")\n"

#define _DEBUG_( X ) \
std::cout << "DEBUG: !!!" << X << " (" << __FILE__ << ":" << __LINE__ << ")\n"

#endif
