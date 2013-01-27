#ifndef EXIO_ADMINSESSIONID_H
#define EXIO_ADMINSESSIONID_H

#include <string>

namespace exio
{

/* Unique Session ID provided to each AdminSession */
class SID
{
  public:
    SID();  // default value represents invalid session ID

    SID(unsigned long long id,
       int fd,
       const std::string & label);

    bool operator==(SID rhs) const
    {
      return (this->m_unqiue_id == rhs.m_unqiue_id)
        and  (this->m_fd == rhs.m_fd);
    }

    bool operator<(SID rhs) const
    {
      return
        (this->m_unqiue_id < rhs.m_unqiue_id)
        or
        (this->m_unqiue_id == rhs.m_unqiue_id and this->m_fd < rhs.m_fd);
    }
    // Textual description of the connection address
    const std::string & addr() const { return m_label; }

    // Textual description of the connection address
    std::string toString() const;

  private:
    unsigned long long  m_unqiue_id;
    int m_fd;
    std::string m_label;
};


} // namespace exio

#endif
