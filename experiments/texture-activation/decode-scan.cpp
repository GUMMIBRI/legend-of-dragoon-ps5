#include "texture_image_decode.h"
#include <webp/decode.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
extern "C" VP8StatusCode WebPGetFeaturesInternal(const uint8_t*, size_t, WebPBitstreamFeatures*, int){return VP8_STATUS_UNSUPPORTED_FEATURE;}
extern "C" uint8_t* WebPDecodeRGBAInto(const uint8_t*,size_t,uint8_t*,size_t,int){return nullptr;}
int main(int argc,char**argv){if(argc!=2)return 2;size_t good=0,bad=0;for(auto const& e:std::filesystem::recursive_directory_iterator(argv[1])) {if(!e.is_regular_file()||e.path().extension()!=".png")continue;std::ifstream f(e.path(),std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});DecodedTextureImage out;std::string err;if(texture_image_decode(b.data(),b.size(),TEXTURE_IMAGE_DEFAULT_DECODED_BUDGET,out,err))++good;else{++bad;std::cerr<<e.path().string()<<": "<<err<<"\n";}}std::cout<<"PNG actual decoder passed="<<good<<" failed="<<bad<<"\n";return bad?1:0;}
