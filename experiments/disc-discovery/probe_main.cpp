#include "lod_disc_probe.h"
#include <cstdio>
int main(int argc,char **argv){if(argc!=2)return 3;char error[256];int result=lod_disc_probe(argv[1],error,sizeof error);std::printf("result=%d error=%s\n",result,error);return result==1?0:result==0?1:2;}
