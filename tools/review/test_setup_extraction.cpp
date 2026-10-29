// Run against SDL3 and zlib. Creates only fixtures under the supplied directory.
// Exercises the production reader, CRC/resume path and progress event producer.
#include "../../port/setup/native.cpp"
#include "../../port/setup/progress.h"
#include <cassert>

int main(int argc,char **argv) {
    if(argc!=2)return 2;
    SDL_Init(0);std::string root=argv[1];SDL_CreateDirectory(root.c_str());
    std::vector<Uint8> data(6*1024*1024),buf(1024*1024);
    for(size_t i=0;i<data.size();++i)data[i]=(Uint8)(i*37+i/257);
    std::string source=root+"/source.bin",out=root+"/out.bin";
    assert(SDL_SaveFile(source.c_str(),data.data(),data.size()));
    Uint32 crc=SDL_crc32(0,data.data(),data.size());
    Iso iso;iso.io=SDL_IOFromFile(source.c_str(),"rb");assert(iso.io);
    ExtractionProgress progress;progress.total=data.size();progress.count=1;
    auto run=[&](){
        progress.completed=0;progress.files=0;progress.emit(0,true);
        checked_out(iso,0,data.size(),out,(unsigned)data.size(),crc,buf,[&](Uint64 done){
            SDL_Delay(105);progress.emit(std::min(done,(Uint64)data.size()-1));
        });
        progress.completed=data.size();progress.files=1;progress.emit(0,true);
        std::string line;SetupProgress parsed;unsigned updates=0;
        while(Native_Poll(line))if(line.rfind("@progress ",0)==0){
            assert(parsed.read(line.c_str()+10));++updates;
        }
        assert(updates>=7 && parsed.files==1 && parsed.fraction==1);
        assert(file_crc(out,data.size(),buf)==crc);
    };
    run(); // fresh extraction, including chunk updates within a large file
    run(); // complete cached installation: CRC verification still progresses
    data[0]^=1;assert(SDL_SaveFile(out.c_str(),data.data(),data.size()));data[0]^=1;
    run(); // corrupt cached file is rewritten, never silently skipped
    sCancel=false;bool cancelled=false;
    try{file_crc(out,data.size(),buf,[&](Uint64){sCancel=true;});}
    catch(const Stop &){cancelled=true;}
    assert(cancelled);sCancel=false;
    // Cancellation closes the reader, so repair/removal can proceed on Windows.
    assert(SDL_RemovePath(out.c_str()));run();
    bool rejected=false;
    try{checked_out(iso,0,data.size(),root+"/bad.bin",(unsigned)data.size(),crc^1,buf);}
    catch(const Stop &){rejected=true;}
    assert(rejected && !SDL_GetPathInfo((root+"/bad.bin").c_str(),NULL));
    printf("PASS: extraction, chunk progress, cached CRC, repair, cancel/resume, corrupt source rejection\n");
    SDL_Quit();return 0;
}
