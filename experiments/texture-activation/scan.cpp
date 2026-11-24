#include "duckstation_texture_pack.h"
#include "texture_image_decode.h"
#include <cstdio>
bool texture_image_probe(const uint8_t*,size_t,size_t,TextureImageProbe&,std::string&){return false;}
bool texture_image_decode(const uint8_t*,size_t,size_t,DecodedTextureImage&,std::string&){return false;}
int main(int argc,char**argv){if(argc!=2)return 2; DuckTexturePack* p=nullptr;char e[512]={};if(!duck_texture_pack_create(argv[1],&p,e,sizeof(e))){std::printf("scan failed: %s\n",e);return 1;}DuckTexturePackInfo i{};duck_texture_pack_get_info(p,&i);std::printf("entries=%zu ambiguous=%zu ignored=%zu diagnostic=%s\n",i.replacement_count,i.ambiguous_count,i.ignored_count,i.diagnostic?i.diagnostic:"");duck_texture_pack_destroy(p);return 0;}
