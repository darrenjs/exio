#ifndef EXIO_TABLESERIALISER_H
#define EXIO_TABLESERIALISER_H

#include "exio/sam.h"

#include <string>

namespace exio
{


//----------------------------------------------------------------------
/*
 * Base class for all classes that are involved with table serialisation
 */
class TableEventSerialiser
{
  public:
    virtual ~TableEventSerialiser();

    void set_table_name(const std::string&);


  protected:
    std::string m_table_name;
};



//----------------------------------------------------------------------
class TableClearSerialise : public TableEventSerialiser
{
  public:

    /* Initialise the message we wil work with */
    void init_msg(sam::txMessage& msg,
                  const std::string& table_name);


};

//----------------------------------------------------------------------
class UpdateSerialiser : public TableEventSerialiser
{
  public:
    UpdateSerialiser();


    /* Initialise a message for use.  All subsequent calls of add_update will
     * update the provided message. */
    void init_msg(sam::txMessage& msg,
                  const std::string& table_name,
                  const std::string& row_key);

    // Return true if field was added ok, ie, there was enough space. False
    // means that the message is now too large to fit in the encoding
    // protocol.
    bool add_update(const std::string& column,
                    const std::string& value);

  private:
    sam::txContainer* m_row;
};


//----------------------------------------------------------------------
class PCMDSerialiser : public TableEventSerialiser
{
  public:
    PCMDSerialiser();

    /* Initialise a message for use.  All subsequent calls of add_update will
     * update the provided message. */
    void init_msg(sam::txMessage& msg,
                  const std::string& table_name,
                  const std::string& row_key);

    // Return true if field was added ok, ie, there was enough space. False
    // means that the message is now too large to fit in the encoding
    // protocol.
    bool add_update(const std::string& column,
                    const sam::txContainer& meta);

  private:
    sam::txContainer* m_row;
};

}


#endif