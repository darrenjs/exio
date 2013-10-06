


#include "exio/sam.h"
#include "exio/SamBuffer.h"

#include "exio/AppSvc.h"

#include <iostream>

#include <string.h>


void banner()
{
  for (int i = 0; i < 80; ++i)
    std::cout << "-";
  std::cout <<"\n";
}

void dump(const sam::txContainer& c, const char* prefix = NULL)
{
  sam::MessageFormatter formatter(false);
  if (prefix) std::cout << prefix;
  formatter.format_contents(c, std::cout);
  std::cout << "\n";
}


void test_add_field_and_child_same_name()
{
  banner();
  sam::txContainer meta("meta");

  meta.put_field("test", "v0");
  meta.put_child("test").put_field("sub","v0");

  dump(meta, "META: ");
}

//----------------------------------------------------------------------
void test1()
{
  banner();
  sam::txContainer meta("meta");
  meta.put_field("f0", "v0");
  meta.put_field("f1", "v1");
  meta.put_child("c0");
  meta.put_child("c0").put_child("c0c0").put_field("c0c0f0","val00");
  meta.put_child("c0").put_child("c0c1").put_field("c0c1f0","val00");

  sam::txContainer empty("empty");
  dump(meta, "meta: ");

  meta.merge( empty );
  dump(meta, "AFTER: ");

  empty.merge(meta);
  meta.clear();
  meta.merge(empty);
  dump(meta, "AFTER2: ");

}
//----------------------------------------------------------------------
void test0()
{
  banner();
  sam::txContainer meta("meta");
  sam::txContainer empty("empty");


  meta.put_field("f0", "v0");
  meta.put_field("f1", "v1");
  meta.put_field("f2", "v2");

  meta.put_child("c0");
  meta.put_child("c0").put_field("c0f0","v0");
  meta.put_child("c0").put_field("c0f1","v1");
  meta.put_child("c0").put_field("c0f2","v2");
  meta.put_child("c1");

  meta.put_child("c3");
  meta.put_child("c3").put_child("c3c0");
  meta.put_child("c3").put_child("c3c1");
  meta.put_child("c3").put_child("c3c2");
  meta.put_child("c3").put_field("c3f0","v0");
  meta.put_child("c3").put_field("c3f1","v1");
  meta.put_child("c3").put_field("c3f2","v2");


  meta.put_child("c4");
  meta.put_child("c4").put_child("c4c0");
  meta.put_child("c4").put_child("c4c1");
  meta.put_child("c4").put_child("c4c2");
  meta.put_child("c4").put_field("c4f0","v0");
  meta.put_child("c4").put_field("c4f1","v1");
  meta.put_child("c4").put_field("c4f2","v2");

  dump(meta, "META: ");

  sam::txContainer delta("delta");
  delta.put_field("f0", "VAL_f0");
  delta.put_field("c3", "VAL_c3");
  delta.put_field("f5", "VAL_f5");
  delta.put_child("f1").put_field("F1","v0");
  delta.put_child("new_child");
  meta.put_child("c4").put_child("new_child");  // new child
  meta.put_child("c4").put_child("c4f0");       // new child overwrites field
  meta.put_child("c4").put_field("c4c0");       // new field overwrites child

  dump(delta, "delta: ");

  meta.merge( empty );
  meta.merge( delta );
  meta.merge( empty );
  meta.merge( delta );
  dump(meta, "AFTER: ");
}
//----------------------------------------------------------------------


void test2()
{
  //char * orig="{SAM0100:45:logon:[head=[serviceid=test,],]}\n";
  const char * orig="{SAM0100:45:logon#[head=[serviceid=test,],]}\n{SAM0100:45:LOGON:[HEAD=[SERVICEID=TEST,],]}\n";

  exio::AppSvc appsvc;

  sam::SAMProtocol samp(appsvc);
  sam::txMessage msg;

  samp.decodeMsg(msg, orig, strlen(orig));


  exio::DynamicSamBuffer sbuf;
  samp.encodeMsg(msg, &sbuf);

  if (strncmp(orig, sbuf.msg_start(), std::min(strlen(orig), sbuf.msg_size())) != 0)
  {
    std::cout << "differ!\n";

  }
  dump(msg.root(), "msg: ");

}

void test3()
{
  //char * orig="{SAM0100:45:logon:[head=[serviceid=test,],]}\n";
  const char * orig="{SAM0100:452839fgdfgdf";

  exio::AppSvc appsvc;

  sam::SAMProtocol samp(appsvc);
  sam::txMessage msg;

  samp.decodeMsg(msg, orig, strlen(orig));


  exio::DynamicSamBuffer sbuf;
  samp.encodeMsg(msg, &sbuf);

  if (strncmp(orig, sbuf.msg_start(), std::min(strlen(orig), sbuf.msg_size())) != 0)
  {
    std::cout << "differ!\n";

  }
  dump(msg.root(), "msg: ");

}


int main(int argc, char** argv)
{
  try
  {
    test0();
    test_add_field_and_child_same_name();
    test1();
    //test2();
    test3();
  }
  catch (const std::exception& e)
  {
    std::cout << "exception: "<< e.what() << "\n";
    return 1;
  }
  return 0;
}
