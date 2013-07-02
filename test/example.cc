


#include "exio/AdminInterface.h"

#include <unistd.h>

exio::AdminInterface * ai = NULL;

struct AdminObject : public exio::AdminCommand::Callback
{
    virtual exio::AdminResponse invoke(exio::AdminRequest& req)
    {
      exio::AdminRequest::Args::const_iterator i;

      std::map<std::string,std::string> update;

      std::string rowkey;

      for (i = req.args().begin(); i != req.args().end(); ++i)
      {
        if (i == req.args().begin())
        {
          rowkey = *i;
        }
        else
        {
          int now = time(NULL);
          std::ostringstream os;
          os << *i << "_" << now;
          update[ *i ] = os.str();
        }
      }

      if (not rowkey.empty()) ai->monitor_update("table", rowkey, update);

      return exio::AdminResponse::success(req.reqseqno);
    }

};



int main(int argc, char** argv)
{
  exio::ConsoleLogger logger(exio::ConsoleLogger::eStdout,
                             exio::ConsoleLogger::eAll,
                             true);
  exio::Config config;
  config.serviceid = "test";
  config.server_port = 55555;

  AdminObject adminobj;
  ai = new exio::AdminInterface(config, &logger);

  exio::AdminCommand ac("test_admin",
                        "a test", "a test help",
                        &adminobj);
  ai->add_admin(ac);


  ai->start();

  while(true)
  {
    sleep(60);
  }

}
