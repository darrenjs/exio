
#include "exio/AdminInterface.h"
#include "exio/MsgIDs.h"

#include <iostream>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>



exio::AdminInterface * ai = NULL;

exio::ConsoleLogger logger(exio::ConsoleLogger::eStdout,
                           exio::ConsoleLogger::eWarn,
                           true);

//----------------------------------------------------------------------

struct Admin_LargeReply : public exio::AdminCommand::Callback
{
    virtual exio::AdminResponse invoke(exio::AdminRequest& req)
    {
      exio::AdminRequest::Args::const_iterator i;

      size_t len = 100;

      if (req.args().size())
      {
        len = atoi( req.args()[0].c_str() );
      }

      std::ostringstream os;

      try
      {
        size_t bytes_count = len;
        for (size_t i = 0; i < bytes_count; ++i)
        {
          os << char('a' + (i%26));
        }
      }
      catch(const std::exception& e)
      {
        std::cout << "OUT OF MEMORY!\n";
        throw;
      }

      std::string data = os.str();
      std::ostringstream msg;
      msg << "sending data with length " << data.size();
      logger.info(msg.str());

      return exio::AdminResponse::success(req.reqseqno, data);
    }
};
//----------------------------------------------------------------------
struct Admin_LotsOfNumbers : public exio::AdminCommand::Callback
{
    virtual exio::AdminResponse invoke(exio::AdminRequest& req)
    {
      exio::AdminRequest::Args::const_iterator i;

      std::string data;

      size_t len = 100;

      if (req.args().size())
      {
        len = atoi( req.args()[0].c_str() );
      }

      std::ostringstream os;

      std::cout << "STARTING\n";
      try
      {
        for (size_t i = 0; i < len; ++i)
        {
          os << i << "\n";


          // TODO: use this code below for the session-check-feature
          // if (not req.intf->session_open(req.id))
          // {
          //   logger.error("aborting admin, because sessions closed");
          //   return exio::AdminResponse::error(req.reqseqno, exio::id::err_no_session);
          // }
          // sleep(1);
          // std::cout << "i=" << i << "\n";
        }

        std::ostringstream msg;
        data = os.str();
        msg << "sending data with length " << data.size();
        logger.info(msg.str());
      }
      catch(std::bad_alloc& e)
      {
        std::cout << "OUT OF MEMORY!\n";
        throw;
      }

      return exio::AdminResponse::success(req.reqseqno, data);
    }
};

//----------------------------------------------------------------------

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

//----------------------------------------------------------------------

void die(const char* e)
{
  std::cout << e << "\n";
  exit( 1 );
}


struct  StockPrice
{
    std::string name;
    double bbp1;
    double bap1;
    int    bbs1;
    int    bas1;


    StockPrice(const char * __name,
               double price)
      : name(__name),
        bbp1(price*0.9),
        bap1(price*1.1),
        bbs1(0),
        bas1(0)
    {

    }

    void update()
    {
      double r = double(rand()) / RAND_MAX;
      if (r > 0.6)
      {
        bbp1 += 1;
        bap1 += 1;
        bbs1 = 1 + (100*(double(rand()) / RAND_MAX));
        bas1 = 1 + (100*(double(rand()) / RAND_MAX));
      }
      else if (r > 0.3)
      {
        bbp1 -= 1;
        bap1 -= 1;
        bbs1 = 1 + (100*(double(rand()) / RAND_MAX));
        bas1 = 1 + (100*(double(rand()) / RAND_MAX));
      }
      else
      {
      }

    }

    void table(std::map<std::string, std::string>& dest )
    {
      char buf[100];

      sprintf(buf, "%f", bbp1); dest["bbp1"] = buf;
      sprintf(buf, "%f", bap1); dest["bap1"] = buf;
      sprintf(buf, "%d", bbs1); dest["bbs1"] = buf;
      sprintf(buf, "%d", bas1); dest["bas1"] = buf;
      dest["ric"] = name;
    }


};

//----------------------------------------------------------------------
int __main(int argc, char** argv)
{

  int port = -1;
  for (int i = 1; i < argc; ++i)
  {
    if ( strcmp(argv[i],"-p")==0 )
    {
      if ( ++i < argc)
      {
        port = atoi(argv[i]);
      }
      else die("missing PORT");
    }
  }

  if (port == -1) die("missing -p PORT");


  exio::Config config;
  config.serviceid = "test";
  config.server_port = port;

  AdminObject adminobj;
  ai = new exio::AdminInterface(config, &logger);

  exio::AdminCommand ac("test_admin",
                        "a test", "a test help",
                        &adminobj);
  ai->add_admin(ac);


  Admin_LargeReply adminLargeReply;
  ac = exio::AdminCommand("large_admin",
                          "send a large reply",
                          "Generate a large message to test the exio layer",
                          &adminLargeReply);
  ai->add_admin(ac);

  Admin_LotsOfNumbers adminLotsOfNumbers;
  ac = exio::AdminCommand("lots_of_numbers",
                          "send a large reply",
                          "Generate a large message to test the exio layer",
                          &adminLotsOfNumbers);
  ai->add_admin(ac);

  // main loop
  ai->start();


  std::vector<StockPrice> names;
  names.push_back(StockPrice("VOD.L", 200.0));
  names.push_back(StockPrice("BT.L", 120.0));
  names.push_back(StockPrice("HSBC.L", 50.0));
  names.push_back(StockPrice("RIO.L", 150.0));
  names.push_back(StockPrice("BA.L", 450.0));


  while(true)
  {
    for (std::vector<StockPrice>::iterator i = names.begin(); i != names.end(); ++i)
    {
      std::map<std::string, std::string> up;
      i->update();
      i->table(up);
      ai->monitor_update("lse", i->name, up);
    }


    usleep(1000000);
  }
  while(true) { sleep(10); }

  return 0;
}

//----------------------------------------------------------------------
int main(int argc, char** argv)
{
  try
  {
    return __main(argc, argv);
  }
  catch (const std::exception & e)
  {
    std::cout << "exception caught in main: " << e.what() << "\n";
  }
  catch (...)
  {
    std::cout << "exception caught in main: unknown";
  }

  return 1;
}
