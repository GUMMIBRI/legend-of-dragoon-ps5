#include "lod_disc_probe.h"
#include "iso_reader.h"
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <system_error>

/* Disc identity uses the same reader as actual playback. Sector 16 is user
 * data for raw, cooked, CUE and CHD; file extension or length is not geometry.
 * A volume label is a compatibility gate, not full-disc integrity proof. */
extern "C" int lod_disc_probe(const char *path,char *error,unsigned size) {
    auto report=[&](const char *text){if(error&&size)std::snprintf(error,size,"%s",text);};
    report("");
    if(!path||!*path){report("No disc image specified");return LOD_DISC_IO;}
    try {
        std::error_code ec;auto st=std::filesystem::status(std::filesystem::u8path(path),ec);
        if(ec){report(ec.message().c_str());return LOD_DISC_IO;}
        if(!std::filesystem::is_regular_file(st)){report("Disc image is not a regular file");return LOD_DISC_IO;}
        PS1::ISOReader reader;
        if(!reader.Open(path)){report("Disc reader could not open this image or its referenced tracks");return LOD_DISC_IO;}
        unsigned char pvd[2048];
        if(!reader.ReadSector(16,pvd)){report("Cannot read disc volume descriptor");return LOD_DISC_IO;}
        bool label_tail=true;
        for(unsigned i=49;i<72;++i) if(pvd[i]!=0 && pvd[i]!=' ') label_tail=false;
        if(pvd[0]!=1||std::memcmp(pvd+1,"CD001",5)||pvd[6]!=1||std::memcmp(pvd+40,"SCUS94491",9)||!label_tail){
            report("Not recognized as US Legend of Dragoon Disc 1 (SCUS-94491)");return LOD_DISC_WRONG;
        }
        return LOD_DISC_VALID;
    } catch(const std::exception& e){report(e.what());return LOD_DISC_IO;}
    catch(...){report("Disc reader failed");return LOD_DISC_IO;}
}
