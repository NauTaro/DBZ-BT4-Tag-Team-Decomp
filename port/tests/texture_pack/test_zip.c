#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_TIME
#include "../../third_party/miniz/miniz.h"
#include <assert.h>
#include <string.h>
int main(int argc,char **argv){
    mz_zip_archive z={0};size_t size=0;void *data;
    assert(argc==2&&mz_zip_reader_init_file(&z,argv[1],0));
    data=mz_zip_reader_extract_to_heap(&z,0,&size,0);
    assert(data&&size==12&&!memcmp(data,"BT4 ZIP64 OK",12));
    mz_free(data);mz_zip_reader_end(&z);return 0;
}
