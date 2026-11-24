extern "C" {
#include "disc_resolver.h"
}
#include "lod_disc_discovery.h"
#include "lod_disc_probe.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
extern "C" int lod_find_disc(const char* app,const char* data,const char* const usb[8],const char* explicit_dir,char* out,size_t cap,lod_attempt_log log,void* ctx){
 if(!app||!out||!cap)return -1;out[0]=0;
 auto search=[&](const char* dir){char error[512]={};int r=lod_disc_find_in_directory(dir,out,cap,error,sizeof error);if(log)log(dir,r==1?"selected":r==0?"no matching disc":error,r<0?-r:0,ctx);return r;};
 try {
  if(explicit_dir&&*explicit_dir)return search(explicit_dir);
  const char* suffixes[]={"","disks","discs","lod/disc"};
  for(auto suffix:suffixes){std::filesystem::path p=std::filesystem::u8path(app);if(*suffix)p/=suffix;std::string path=p.u8string();int r=search(path.c_str());if(r)return r;}
  if(data&&*data){int r=search(data);if(r)return r;}
  for(int i=0;i<8;++i)if(usb&&usb[i]&&*usb[i]){int r=search(usb[i]);if(r)return r;}
  return 0;
 }catch(...){if(log)log(app,"disc directory resolution failed",0,ctx);return -1;}
}
extern "C" int lod_validate_explicit(const char* dir,int raw,char* out,size_t cap,lod_attempt_log log,void* ctx){(void)raw;char error[512]={};int r=lod_disc_find_in_directory(dir,out,cap,error,sizeof error);if(log)log(dir,r==1?"selected":r==0?"no matching disc":error,r<0?-r:0,ctx);return r;}
