#include "exio/utils.h"

#include <sstream>

#include <string.h>

namespace exio {
namespace utils {

//----------------------------------------------------------------------
std::vector<std::string> tokenize(const char* src,
                                  char delim,
                                  bool want_empty_tokens)
{
  std::vector<std::string> tokens;

  if (src and *src != '\0')
    while( true )  {
      const char* d = strchr(src, delim);
      size_t len = (d)? d-src : strlen(src);

      if (len or want_empty_tokens)
        tokens.push_back( std::string(src, len) ); // capture token

      if (d) src += len+1; else break;
    }

  return tokens;
}
//----------------------------------------------------------------------
std::string to_str(int i)
{
  // TODO: for perf, here I could use a look up for say, the first 100 numbers

  std::ostringstream os;
  os << i;
  return os.str();
}
//----------------------------------------------------------------------
std::string strerror(int e)
{
  std::string retval;

  char errbuf[256];
  memset(errbuf, 0, sizeof(errbuf));

/*

TODO: here I should be using the proper feature tests for the XSI
implementation of strerror_r .  See man page.

  (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE

*/

#ifdef _GNU_SOURCE
  // the GNU implementation might not write to errbuf, so instead, always use
  // the return value.
  return ::strerror_r(e, errbuf, sizeof(errbuf)-1);
#else
  // XSI implementation
  if (::strerror_r(e, errbuf, sizeof(errbuf)-1) == 0)
    return errbuf;
#endif

  return "unknown";
}





}} // namespace
