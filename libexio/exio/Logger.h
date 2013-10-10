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
#ifndef EXIO_LOGGER_H__
#define EXIO_LOGGER_H__

#include <iostream>

#define _HERE_  " (" << __FILE__ << ":" << __LINE__ << ")"

#define _TODO_( X ) \
std::cout << "TODO: " << X << " (" << __FILE__ << ":" << __LINE__ << ")\n"


#define _P_( X ) \
std::cout << "DEBUG: " << # X << "=[" << X << "] (" << __FILE__ << ":" << __LINE__ << ")\n"


#define _DEBUG_( _LOGPTR, X )                                \
  if ( _LOGPTR )                                             \
  {                                                          \
    if ( _LOGPTR and _LOGPTR->want_debug() )                 \
    {                                                        \
      std::ostringstream __xx_oss;                           \
      __xx_oss << "exio: " << X ;                            \
      _LOGPTR->debug( __xx_oss.str(),__FILE__,__LINE__  ) ;  \
    }                                                        \
  }

#define _INFO_( _LOGPTR, X )                                 \
  if ( _LOGPTR )                                             \
  {                                                          \
    if ( _LOGPTR and _LOGPTR->want_info() )                  \
    {                                                        \
      std::ostringstream __xx_oss;                           \
      __xx_oss << "exio: " << X ;                            \
      _LOGPTR->info( __xx_oss.str(),__FILE__,__LINE__  ) ;   \
    }                                                        \
  }

#define _WARN_( _LOGPTR, X )                                 \
  if ( _LOGPTR )                                             \
  {                                                          \
    if ( _LOGPTR and _LOGPTR->want_warn() )                  \
    {                                                        \
      std::ostringstream __xx_oss;                           \
      __xx_oss << "exio: " << X ;                            \
      _LOGPTR->warn( __xx_oss.str(),__FILE__,__LINE__  ) ;   \
    }                                                        \
  }

#define _ERROR_( _LOGPTR, X )                                \
  if ( _LOGPTR )                                             \
  {                                                          \
    if ( _LOGPTR and _LOGPTR->want_error() )                 \
    {                                                        \
      std::ostringstream __xx_oss;                           \
      __xx_oss << "exio: " << X ;                            \
      _LOGPTR->error( __xx_oss.str(),__FILE__,__LINE__  ) ;  \
    }                                                        \
  }

#endif
