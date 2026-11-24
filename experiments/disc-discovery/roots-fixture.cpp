#include <cassert>
#include <cstring>
#include <string>
#include <vector>
extern "C" {
#include "disc_resolver.h"
}
static std::vector<std::string> seen;static std::string hit,fail,denied;
extern "C" int lod_disc_find_in_directory(const char*p,char*out,size_t n,char*err,size_t cap){seen.emplace_back(p);if(denied==p){if(cap)std::strncpy(err,"denied",cap);return -3;}if(fail==p){if(cap)std::strncpy(err,"failure",cap);return -1;}if(hit==p){if(n)std::strncpy(out,"selected",n);return 1;}return 0;}
int main(){char out[100];const char* usb[]={"/mnt/usb0/lod","/mnt/usb1/lod",nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
 auto test=[&](const char* expected){seen.clear();hit=expected;fail.clear();assert(lod_find_disc("/mnt/usb0/homebrew/LOD-test","/data/lod",usb,nullptr,out,sizeof out,nullptr,nullptr)==1);assert(seen.back()==expected);};
 test("/mnt/usb0/homebrew/LOD-test");test("/mnt/usb0/homebrew/LOD-test/discs");test("/data/lod");test("/data");test("/mnt/usb0/lod");test("/mnt/usb0");test("/mnt/usb1");
 seen.clear();hit="/mnt/usb0";denied="/data/lod";assert(lod_find_disc("/app","/data/lod",usb,nullptr,out,sizeof out,nullptr,nullptr)==1);assert(seen.back()=="/mnt/usb0");denied.clear();
 seen.clear();hit="/explicit";assert(lod_find_disc("/app","/data/lod",usb,"/explicit",out,sizeof out,nullptr,nullptr)==1);assert(seen.size()==1);seen.clear();hit="";fail="/app/disks";assert(lod_find_disc("/app","/data/lod",usb,nullptr,out,sizeof out,nullptr,nullptr)==-1);assert(seen.size()==2);
}
