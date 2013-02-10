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


}} // namespace

#endif
