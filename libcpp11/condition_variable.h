#ifndef CPP11_CONDITION_VARIABLE_H
#define CPP11_CONDITION_VARIABLE_H

#include "mutex.h" // unique_lock

namespace cpp11 {

class condition_variable
{
    typedef pthread_cond_t __native_type;
    __native_type _M_cond;

  public:
    typedef __native_type* 		native_handle_type;

    condition_variable();
    ~condition_variable();

    void wait( unique_lock<mutex>& __lock );
    void notify_one();
    void notify_all();

    native_handle_type
    native_handle()
    { return &_M_cond; }

  private:
    condition_variable(const condition_variable&); // no copy
    condition_variable& operator=(const condition_variable&); // no assignment
};

} // namespace qm

#endif
