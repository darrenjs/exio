


#include "exio/AdminInterface.h"
#include "exio/utils.h"
#include "exio/MsgIDs.h"

//#include "exio/DataRow.h"

#include "thread.h"

#include <string.h>
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> // for strchr and strlen



/* std::cout console logging service */


class coutLogService : public exio::LogService
{
  public:

    virtual void debug(const std::string& s)
    { std::cout << "INFO: " << s << "\n"; }
    virtual void info(const std::string&  s)
    { std::cout << "INFO: " << s << "\n"; }
    virtual void error(const std::string& s)
    { std::cout << "INFO: " << s << "\n"; }
    virtual void warn(const std::string&  s)
    { std::cout << "INFO: " << s << "\n"; }

    virtual bool want_debug() { return true; }
    virtual bool want_info()  { return true; }
    virtual bool want_error() { return true; }
    virtual bool want_warn()  { return true; }

    virtual bool inc_source() { return true; }
};


/* Sample server */


exio::AdminInterface * g_admin = NULL;

class DemoAdmin : public exio::AdminCommand::Callback
{
  public:

    exio::AdminResponse invoke(exio::AdminRequest& req)
    {
      std::cout << "executing command" << "\n";

      return exio::AdminResponse::success(req.reqseqno, "test test");
    }
};



/*
 * want_empty_tokens==true  : include empty tokens, like strsep()
 * want_empty_tokens==false : exclude empty tokens, like strtok()
 */
std::vector<std::string> tokenize(const char* src,
                                  const char * delim,
                                  bool want_empty_tokens)
{
  std::vector<std::string> tokens;

  if (src and *src != '\0') // defensive
    while( true )  {
      const char* d = strpbrk(src, delim);
      size_t len = (d)? d-src : strlen(src);

      if (len or want_empty_tokens)
        tokens.push_back( std::string(src, len) ); // capture token

      if (d) src += len+1; else break;
    }

  return tokens;
}


// TODO: need to identify when a new session is added.

// TODO: need to identify when a session has been removed

void update(const std::string& tablename,
            const std::string& rowkey,
            std::map<std::string, std::string> fields)
{

  // TODO: integrate the incoming fields with the current status for this
  // table/row

  // TODO: identify what has changed, and generate a change-reprot

  // TODO: from the change-report, build a SAM message

  // TODO: send update to each session  -- implemetn send_all;

  // TODO: done
}


int run_ps_cmd(std::list< std::map<std::string, std::string> > & rows)
{
  const char* cmd="ps -u darrens -o pid,ppid,nlwp,pcpu,pmem,comm";

  FILE * fp = popen(cmd, "r");

  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  if (fp == NULL) return EXIT_FAILURE;

  std::vector<std::string> cols;

  while ((read = getline(&line, &len, fp)) != -1)
  {
    if (len == 0) break;
    std::vector<std::string> fields = tokenize(line, " \n", false);
    if ( cols.empty() )
    {
      cols = fields;

      for (std::vector<std::string>::iterator it = cols.begin();
           it != cols.end(); ++it)
      {
        std::string & col=*it;
        for (size_t i = 0; i < col.length(); ++i) col[i] = tolower(col[i]);
      }

      continue;
    }

    if (fields.size() != cols.size()) continue;

    rows.push_back( std::map<std::string, std::string>() );
    std::map<std::string, std::string>& row = rows.back();

    for (size_t i = 0; i < fields.size(); ++i)
    {
      row[ cols[i] ] = fields[i];
    }
  }

  free(line);
  pclose(fp);

  return EXIT_SUCCESS;
}


void ps_monitor_TEP()
{
  std::cout << "Starting process monitor thread\n";

  std::list< std::map<std::string, std::string> > rows;

  std::string table_name="ps";

  while (true)
  {
    if ( run_ps_cmd(rows) == EXIT_SUCCESS )
//    if ( run_ps( row ) == EXIT_SUCCESS )
    {
      std::list< std::map<std::string, std::string> >::iterator it;
      for (it=rows.begin(); it != rows.end(); ++it)
      {
        std::map<std::string, std::string> & row = *it;

        // skip some processes
        std::string command = row ["command"];
        if (command == "ps" or command == "sh") continue;

        std::string row_key = row ["pid"];
        if (not row_key.empty() )
        {
          row[ exio::id::row_key ] = row_key;
          g_admin->monitor_update(table_name,
                                  row_key,
                                  row);
        }
      }

      rows.clear();
    }

    sleep(5);
  }

}


int main(int agrc, char** argc)
{
  cpp11::thread monitor_thread( &ps_monitor_TEP );


  // hmmm, but if you forget to add a null! This is teh problem with C++
  // arrays! As you pass them around, they don't keep track of their length



  // sam::qname q1 ( (std::string[]){"head", "data", "reqstate", "ddd"} );
  // q1.push("last");
  // q1 << "hello" << "world";

  // std::cout << "q1: " << q1 << "\n";



  // sam::qname q2 = sam::qname::parse("head.pending");
  // std::cout << "q2: " << q2 << "\n";

  // sam::qname q3 = sam::qname::parse("mistake..was.extra.dot.");
  // std::cout << "q3: " << q3 << "\n";

  // sam::qname q4 = sam::qname::parse( NULL );
  // std::cout << "q4: " << q4 << "\n";

  // return 1 ;
  coutLogService coutlogger;

  exio::Config config;
  config.serviceid="serverdemo";
  config.server_port = 44444;

  exio::AdminInterface myadmin(config, &coutlogger);

  g_admin = &myadmin;


  // TODO: how to do callbacks
  DemoAdmin d;
  exio::AdminCommand admin1("version", "version info","", &d);
  myadmin.add_admin( admin1 );



  myadmin.start();

  monitor_thread.join();

  return 0;
}
