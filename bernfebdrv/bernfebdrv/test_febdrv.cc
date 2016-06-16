#include <iostream>
#include "febdrv.h"

void usage()
{
  std::cout << "Usage: to init febdrv on eth0 interface type" << std::endl;
  std::cout << "febdrv eth0" << std::endl;
}

int main (int argc, char* argv[])
{
  if(argc!=2){
    usage();
    return 0;
  }

  std::cout << argv[0] << " " << argv[1] << std::endl;

  std::cout << "Starting febdrv..." << std::endl;
  std::cout << "Starting febdrv..." << std::endl;
  std::cout << "Starting febdrv..." << std::endl;
  std::cout << "Starting febdrv..." << std::endl;

  FEBDRV febdrv;
  febdrv.Init(argv[1]);

  return 1;
}
