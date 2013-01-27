#include "exio/utils.h"

#include <string.h>

namespace sam {

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


} // namespace sam
