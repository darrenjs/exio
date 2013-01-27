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
      if (_LOGPTR ->inc_source())                            \
      {                                                      \
        __xx_oss << " (" << __FILE__ << ":"                  \
                 << __LINE__ << ")";                         \
      }                                                      \
      _LOGPTR->debug( __xx_oss.str() ) ;                     \
    }                                                        \
  }

#define _INFO_( _LOGPTR, X )                                 \
  if ( _LOGPTR )                                             \
  {                                                          \
    if ( _LOGPTR and _LOGPTR->want_info() )                  \
    {                                                        \
      std::ostringstream __xx_oss;                           \
      __xx_oss << "exio: " << X ;                            \
      if (_LOGPTR ->inc_source())                            \
      {                                                      \
        __xx_oss << " (" << __FILE__ << ":"                  \
                 << __LINE__ << ")";                         \
      }                                                      \
      _LOGPTR->info( __xx_oss.str() ) ;                      \
    }                                                        \
  }

#define _WARN_( _LOGPTR, X )                                 \
  if ( _LOGPTR )                                             \
  {                                                          \
    if ( _LOGPTR and _LOGPTR->want_warn() )                  \
    {                                                        \
      std::ostringstream __xx_oss;                           \
      __xx_oss << "exio: " << X ;                            \
      if (_LOGPTR ->inc_source())                            \
      {                                                      \
        __xx_oss << " (" << __FILE__ << ":"                  \
                 << __LINE__ << ")";                         \
      }                                                      \
      _LOGPTR->warn( __xx_oss.str() ) ;                      \
    }                                                        \
  }

#define _ERROR_( _LOGPTR, X )                                \
  if ( _LOGPTR )                                             \
  {                                                          \
    if ( _LOGPTR and _LOGPTR->want_error() )                 \
    {                                                        \
      std::ostringstream __xx_oss;                           \
      __xx_oss << "exio: " << X ;                            \
      if (_LOGPTR ->inc_source())                            \
      {                                                      \
        __xx_oss << " (" << __FILE__ << ":"                  \
                 << __LINE__ << ")";                         \
      }                                                      \
      _LOGPTR->error( __xx_oss.str() ) ;                     \
    }                                                        \
  }

#endif
