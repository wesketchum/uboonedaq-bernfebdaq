/* sends literal command without parameters to feb */
#include "FEBCONF.h"
#include <cstring>
#include <zmq.h>

void usage()
{
 printf("Usage: ");
 printf("febconf tcp://localhost:5555 SCRfile.txt PMRfile.txt <FEB S/N>\n");
 printf("tcp://localhost:5555 should be replaced with proper febdrv port\n");
 printf("SCRfile is a bitstream .txt file for SC register.\n");
 printf("PMRfile is a bitstream .txt file for PM register.\n");
 printf("<FEB S/N> cannot be 255 or omitted\n");
}


int main (int argc, char **argv)
{
  if(argc!=53) { usage(); return 0;}

  uint8_t mac5 = atoi(argv[4]);

  FEBCONF confobj;

  confobj.SetSCRConf(argv[1]);
  confobj.SetPMRConf(argv[2]);

  if(!confobj.configOK())
    return 0;

  confobj.SendConfs(mac5,argv[1]);
  
  return 1;
}
