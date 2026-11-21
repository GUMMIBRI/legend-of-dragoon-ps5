#include <libchdr/chd.h>
extern "C" {
chd_error chd_open(const char*,int,chd_file*,chd_file**){return CHDERR_FILE_NOT_FOUND;}
void chd_close(chd_file*){}
const chd_header* chd_get_header(chd_file*){return nullptr;}
chd_error chd_read(chd_file*,uint32_t,void*){return CHDERR_READ_ERROR;}
chd_error chd_get_metadata(chd_file*,uint32_t,uint32_t,void*,uint32_t,uint32_t*,uint32_t*,uint8_t*){return CHDERR_METADATA_NOT_FOUND;}
}
