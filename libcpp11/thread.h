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
#ifndef __THREAD_H__
#define __THREAD_H__

#include <pthread.h>

namespace cpp11
{

class Callback
{
  public:
    virtual void run() = 0;
    virtual ~Callback() {}
};


template <typename T>
class MemberFunctionCallback : public Callback
{
  public:

    typedef void (T::*PMF)() ;
    MemberFunctionCallback( PMF pmf, T* obj )
      : m_pmf(pmf),
        m_obj(obj)

    {
    }

    virtual void run()
    {
      (m_obj->*m_pmf)();
    }

  private:
    PMF m_pmf;
    T*  m_obj;
};

/*
  TODO: update so that the internals are more liek the C++11 implementation.
 */
class thread
{
  public:
    typedef pthread_t native_handle_type;

    /// thread::id
    class id
    {
        native_handle_type	_M_thread;

      public:
        id() : _M_thread() { }

        explicit
        id(native_handle_type __id) : _M_thread(__id) { }

      private:
        friend class thread;

        friend bool
        operator==(thread::id __x, thread::id __y)
        {
          return pthread_equal(__x._M_thread, __y._M_thread);
        }

        // template<class _CharT, class _Traits>
        // friend basic_ostream<_CharT, _Traits>&
        // operator<<(basic_ostream<_CharT, _Traits>& __out, thread::id __id);
    };


    /* This class is part of the thread internal's implementation, and not
     * intended for use by users. */
    struct _Impl_base
    {
        inline virtual ~_Impl_base() {}
        virtual void _M_run() = 0;
    };

    /* This class is part of the thread internal's implementation, and not
     * intended for use by users. */
    template <typename T>
    struct _MemberImpl : public _Impl_base
    {
        typedef void (T::*PMF)() ;
        _MemberImpl( PMF pmf, T* obj )
          : m_pmf(pmf),
            m_obj(obj){ }

        virtual void _M_run() { (m_obj->*m_pmf)(); }

        PMF m_pmf;
        T*  m_obj;
    };

    /* This class is part of the thread internal's implementation, and not
     * intended for use by users. */
    struct _FuncImpl : public _Impl_base
    {
        typedef void (*PF)() ;
        _FuncImpl( PF pf ) : m_pf(pf){ }
        virtual void _M_run() { m_pf(); }

        PF m_pf;
    };


  public:

    // template <typename T>
    // thread( void (T::*pmf)()  , T* obj )
    //   : m_joinable(true),
    //     m_callback( new MemberFunctionCallback<T>(pmf, obj))
    // {
    //   start_thread();
    // }


    thread( void (*pf)() )
    {
      start_thread( new _FuncImpl(pf) );
    }


    template <typename T>
    thread( void (T::*pmf)(), T* obj)
    {
      start_thread( new _MemberImpl<T>(pmf,obj) );
    }

    ~thread();

    void detach();
    void join();

    bool joinable() const
    {
      return !(_M_id == id());
    }

  private:
    thread(const thread&);
    thread& operator=(const thread&);

    void start_thread(_Impl_base *);
    void invoke_callback();

  private:

    // Handle to the underlying TOE (thread of execution)
    id	_M_id;

};


//----------------------------------------------------------------------

}



#endif
