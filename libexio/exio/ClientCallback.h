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
#ifndef EXIO_CLIENT_CALLBACK_H
#define EXIO_CLIENT_CALLBACK_H

namespace exio {

class Client;

class ClientCallback
{
  public:
    virtual ~ClientCallback(){}

    /** Data has arrived on the the underlying IO session */
    virtual size_t process_input(Client*, const char*, int) = 0;

    /** Underlying IO session has closed */
    virtual void   process_close(Client*) = 0;
};

}

#endif
