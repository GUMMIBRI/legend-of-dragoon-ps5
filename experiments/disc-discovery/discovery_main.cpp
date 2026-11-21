#include "lod_disc_discovery.h"
#include <cstdio>
int main(int argc,char **argv){if(argc!=2)return 3;char path[4096],error[512];int r=lod_disc_find_in_directory(argv[1],path,sizeof path,error,sizeof error);std::printf("result=%d path=%s error=%s\n",r,path,error);return r==1?0:r==0?1:r==-2?2:3;}
