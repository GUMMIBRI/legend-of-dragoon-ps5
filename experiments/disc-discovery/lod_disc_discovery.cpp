#include "lod_disc_discovery.h"
#include "lod_disc_probe.h"
#include "disc_path.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" int lod_disc_find_in_directory(const char* root,char* selected,size_t capacity,char* error,size_t error_capacity){
 namespace fs=std::filesystem;
 auto report=[&](const std::string &s){if(error&&error_capacity)std::snprintf(error,error_capacity,"%s",s.c_str());};
 if(error&&error_capacity)*error=0;if(selected&&capacity)*selected=0;
 if(!root||!*root||!selected||!capacity){report("Invalid disc directory");return -1;}
 try {
  fs::path dir=fs::u8path(root);if(!dir.is_absolute()){report("Disc directory must be absolute");return -1;}
  std::error_code ec;auto state=fs::symlink_status(dir,ec);
  if(ec==std::errc::no_such_file_or_directory)return 0;
  if(ec||!fs::is_directory(state)){report(std::string("Cannot inspect ")+root+": "+(ec?ec.message():"not a directory"));return -1;}
  std::vector<fs::path> candidates;size_t entries=0;
  fs::directory_iterator it(dir,ec),end;if(ec){report(std::string("Cannot open ")+root+": "+ec.message());return -1;}
  for(;it!=end;it.increment(ec)){
   if(ec){report(std::string("Cannot enumerate ")+root+": "+ec.message());return -1;}
   if(++entries>512){report(std::string("Too many entries in ")+root);return -1;}
   auto status=it->symlink_status(ec);if(ec){report(ec.message());return -1;}if(!fs::is_regular_file(status))continue;
   std::string ext=it->path().extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return (char)std::tolower(c);});
   if(ext==".cue"||ext==".bin"||ext==".iso"||ext==".img"||ext==".chd")candidates.push_back(it->path());
  }
  if(ec){report(ec.message());return -1;}std::sort(candidates.begin(),candidates.end());
  fs::path matched_data,matched_mount;
  for(const auto &candidate:candidates){
   auto resolution=PSXRecompV4::resolve_disc_path(candidate);
   auto parent=fs::weakly_canonical(resolution.data.parent_path(),ec);if(ec){report(ec.message());return -1;}
   auto expected=fs::weakly_canonical(dir,ec);if(ec||parent!=expected){report("Disc CUE references a file outside its directory");return -1;}
   auto st=fs::symlink_status(resolution.data,ec);if(ec){report(std::string("Disc data unavailable: ")+resolution.data.u8string()+": "+ec.message());return -1;}
   if(!fs::is_regular_file(st)){report("Disc data must be a regular file");return -1;}
   char why[256]={};int result=lod_disc_probe(resolution.mount.u8string().c_str(),why,sizeof why);
   if(result<0){report(std::string("Cannot inspect ")+resolution.mount.u8string()+": "+why);return -1;}
   if(result==0)continue;
   if(!matched_data.empty()){bool same=fs::equivalent(matched_data,resolution.data,ec);if(ec){report(ec.message());return -1;}if(!same){report(std::string("Multiple valid Disc 1 images in ")+root);return -2;}}
   else {matched_data=resolution.data;matched_mount=resolution.mount;}
  }
  if(matched_data.empty())return 0;
  std::string output=matched_mount.u8string();if(output.size()>=capacity){report("Disc path is too long");return -1;}
  std::memcpy(selected,output.c_str(),output.size()+1);return 1;
 }catch(const std::exception &e){report(e.what());return -1;}catch(...){report("Disc discovery failed");return -1;}
}
