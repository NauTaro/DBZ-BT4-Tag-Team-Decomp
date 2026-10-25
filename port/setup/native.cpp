// The setup's own installer, for a release folder (Tenkaichi3Decomp, Tenkaichi3Decomp.dat and Tenkaichi3Decomp-setup together, no build tools):
// reads the user's disc image directly, checks it, unpacks the game data and runs the self-test. No Python, no 7z.
// It reports through the same event lines as install.py (@step, @note, @ok, @skip, @fail, @stopped, @done), so the
// window (setup.cpp) shows both the same way.
//
//   disc image   ISO 9660: the volume descriptor at sector 16, directory records from the root
//   checks       SHA-1 of BIN/DBZP.BIN and of SLUS_219.78 laid out as a flat image from 0x100000 (the values of
//                config/DBZP.yaml and config/SLUS_219.78.yaml)
//   game data    gamedata/disc/<loose files>, and every DATA/*.AFS archive split into gamedata/<name>/00000.bin ...
//                (the layout of port/tools/extract_disc.py)
//   European     optional, a second image (SLES-54945): the 60 entries of its DATA/PZS3EU1.AFS the game makes Spanish,
//                German, French and Italian from, into gamedata/pzs3eu1/ (as port/tools/extract_eu.py); the game makes
//                the catalogs and the menu art at its next start (ui_text_extract.c, menu_labels.c)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <zlib.h>
#include "../src/plat_verify_tab.h"
#include "../src/plat_verify_tab_eng.h"
#include "../include/bt4_build_identity.h"
#include <SDL3/SDL.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "native.h"

namespace {

const char *kRomSha1 = "2ae9c636de479c01d2b4e9906c25b7ca807e80d9";
const char *kDbzpSha1 = "de91913f40bd675d70ee748aa16a42a96dff5919";
bool sEnglish;
std::string sDataRoot, sDedupeRoot;
const unsigned int (*sVerify)[2] = kVerify;
const int *sVerifyCount = kVerifyCount;
std::vector<Uint8> sPrograms[2];
const Uint32 kRomBase = 0x100000, kRomEnd = 0x2FF180;
#ifdef _WIN32
const char *kGame = "Tenkaichi3Decomp.exe";
#else
const char *kGame = "Tenkaichi3Decomp";
#endif

std::mutex sLock;
std::vector<std::string> sLines;
std::thread sThread;
std::atomic<bool> sRunning(false), sCancel(false);

void say(const std::string &line) {
    std::lock_guard<std::mutex> lock(sLock);
    sLines.push_back(line);
}

struct Stop { std::string why; };

// ------------------------------------------------------------------------------------------------------- SHA-1
struct Sha1 {
    Uint32 h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    Uint8 block[64];
    Uint64 total = 0;
    size_t fill = 0;

    static Uint32 rol(Uint32 v, int n) { return (v << n) | (v >> (32 - n)); }
    void compress() {
        Uint32 w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 16; i++) {
            w[i] = (Uint32)block[i * 4] << 24 | (Uint32)block[i * 4 + 1] << 16 | (Uint32)block[i * 4 + 2] << 8 | block[i * 4 + 3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        for (int i = 0; i < 80; i++) {
            Uint32 f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d : i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
            Uint32 k = i < 20 ? 0x5A827999 : i < 40 ? 0x6ED9EBA1 : i < 60 ? 0x8F1BBCDC : 0xCA62C1D6;
            Uint32 t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    void add(const Uint8 *p, size_t n) {
        total += n;
        while (n > 0) {
            size_t take = 64 - fill < n ? 64 - fill : n;
            memcpy(block + fill, p, take);
            fill += take; p += take; n -= take;
            if (fill == 64) {
                compress();
                fill = 0;
            }
        }
    }
    std::string hex() {
        Uint64 bits = total * 8;
        Uint8 pad = 0x80, zero = 0, len[8];
        char out[41];
        add(&pad, 1);
        while (fill != 56) {
            add(&zero, 1);
        }
        for (int i = 0; i < 8; i++) {
            len[i] = (Uint8)(bits >> (56 - i * 8));
        }
        add(len, 8);
        for (int i = 0; i < 5; i++) {
            snprintf(out + i * 8, 9, "%08x", h[i]);
        }
        return out;
    }
};

// ------------------------------------------------------------------------------------------------- the disc image
struct IsoFile { std::string path; Uint64 offset, size; };

struct Iso {
    SDL_IOStream *io = NULL;
    std::vector<IsoFile> files;

    void read(Uint64 offset, void *buf, size_t n) {
        if (SDL_SeekIO(io, (Sint64)offset, SDL_IO_SEEK_SET) < 0 || SDL_ReadIO(io, buf, n) != n) {
            throw Stop{"The disc image could not be read to its end. It may be incomplete."};
        }
    }
    void walk(Uint32 lba, Uint32 size, const std::string &dir, int depth) {
        std::vector<Uint8> d(size);
        read((Uint64)lba * 2048, d.data(), size);
        for (size_t pos = 0; pos + 34 <= size;) {
            Uint8 len = d[pos];
            if (len == 0) { // records do not cross sectors: the rest of this one is empty
                pos = (pos / 2048 + 1) * 2048;
                continue;
            }
            if (pos + len > size) {
                break;
            }
            Uint32 ext, bytes;
            memcpy(&ext, &d[pos + 2], 4);
            memcpy(&bytes, &d[pos + 10], 4);
            Uint8 flags = d[pos + 25], nameLen = d[pos + 32];
            std::string name((const char *)&d[pos + 33], nameLen);
            pos += len;
            if (nameLen == 1 && (name[0] == 0 || name[0] == 1)) {
                continue; // this directory and its parent
            }
            size_t semi = name.find(';');
            if (semi != std::string::npos) {
                name.resize(semi);
            }
            if (!name.empty() && name.back() == '.') {
                name.pop_back();
            }
            if (flags & 2) {
                if (depth < 8) {
                    walk(ext, bytes, dir + name + "/", depth + 1);
                }
            } else if (!files.empty() && files.back().path == dir + name) {
                files.back().size += bytes; // a file in several extents: they follow each other
            } else {
                files.push_back({dir + name, (Uint64)ext * 2048, bytes});
            }
        }
    }
    void open(const std::string &path) {
        Uint8 pvd[2048];
        Uint32 lba, size;
        io = SDL_IOFromFile(path.c_str(), "rb");
        if (io == NULL) {
            throw Stop{"The disc image could not be opened: " + path};
        }
        if (SDL_SeekIO(io, 16 * 2048, SDL_IO_SEEK_SET) < 0 || SDL_ReadIO(io, pvd, sizeof(pvd)) != sizeof(pvd) || pvd[0] != 1 ||
            memcmp(pvd + 1, "CD001", 5) != 0) {
            throw Stop{"This file is not a disc image (.iso). Compressed images (.chd, .cso, .gz) have to be converted to .iso first."};
        }
        memcpy(&lba, pvd + 156 + 2, 4);
        memcpy(&size, pvd + 156 + 10, 4);
        walk(lba, size, "", 0);
    }
    const IsoFile *find(const char *path) {
        for (auto &f : files) {
            if (SDL_strcasecmp(f.path.c_str(), path) == 0) {
                return &f;
            }
        }
        return NULL;
    }
    ~Iso() {
        if (io != NULL) {
            SDL_CloseIO(io);
        }
    }
};

std::string lower(std::string s) {
    for (auto &c : s) {
        c = (char)tolower((unsigned char)c);
    }
    return s;
}

std::string upper(std::string s) {
    for (auto &c : s) {
        c = (char)toupper((unsigned char)c);
    }
    return s;
}

void check_cancel() {
    if (sCancel) {
        throw Stop{"Cancelled. Install continues from where it stopped."};
    }
}

// Copies `size` bytes of the image at `offset` into a new file.
void copy_out(Iso &iso, Uint64 offset, Uint64 size, const std::string &path, std::vector<Uint8> &buf) {
    SDL_IOStream *out = SDL_IOFromFile(path.c_str(), "wb");
    if (out == NULL) {
        throw Stop{"Could not write " + path + ". Is the disk full or the folder read-only?"};
    }
    while (size > 0) {
        size_t n = size < buf.size() ? (size_t)size : buf.size();
        iso.read(offset, buf.data(), n);
        if (SDL_WriteIO(out, buf.data(), n) != n) {
            SDL_CloseIO(out);
            throw Stop{"Could not write " + path + ". Is the disk full?"};
        }
        offset += n;
        size -= n;
    }
    if (!SDL_CloseIO(out)) {
        throw Stop{"Could not write " + path + ". Is the disk full?"};
    }
}

// "@step k n Title": the steps of this run are numbered as the window lists them (setup.cpp, reset_rows).
int sStep, sSteps;
void step(const char *title) {
    char line[96];
    snprintf(line, sizeof(line), "@step %d %d %s", ++sStep, sSteps, title);
    say(line);
}

void step_disc(Iso &iso, const std::string &path) {
    step("Disc image");
    iso.open(path);
    const IsoFile *slus = iso.find("SLUS_219.78"), *dbzp = iso.find("BIN/DBZP.BIN");
    if (slus == NULL || dbzp == NULL) {
        throw Stop{"Esta ISO no corresponde al perfil de Team BT4 B14 Rev2 (falta SLUS_219.78).\n"
                   "Other regions and the Wii version have different programs and are not supported."};
    }
    say("@note checking");
    if (slus->size < 0x34 || slus->size > 16*1024*1024 || dbzp->size > 16*1024*1024) {
        throw Stop{"Unsupported BT4 executable size."};
    }
    std::vector<Uint8> elf((size_t)slus->size), rom(kRomEnd - kRomBase, 0), menu((size_t)dbzp->size);
    iso.read(slus->offset, elf.data(), elf.size());
    iso.read(dbzp->offset, menu.data(), menu.size());
    if (elf.size() > 0x34 && memcmp(elf.data(), "\177ELF", 4) == 0) {
        Uint32 shoff;
        Uint16 shentsize, shnum;
        memcpy(&shoff, &elf[0x20], 4);
        memcpy(&shentsize, &elf[0x2E], 2);
        memcpy(&shnum, &elf[0x30], 2);
        if (shentsize < 24 || (Uint64)shoff + (Uint64)shnum * shentsize > elf.size()) {
            throw Stop{"Invalid BT4 executable section table."};
        }
        for (Uint32 i = 0; i < shnum && (size_t)shoff + (size_t)(i + 1) * shentsize <= elf.size(); i++) {
            Uint32 sh[6]; // name, type, flags, address, offset, size
            memcpy(sh, &elf[shoff + (size_t)i * shentsize], sizeof(sh));
            if ((sh[2] & 2) && sh[1] != 8 && sh[5] != 0 && sh[3] >= kRomBase && (Uint64)sh[3] + sh[5] <= kRomEnd && (Uint64)sh[4] + sh[5] <= elf.size()) {
                memcpy(&rom[sh[3] - kRomBase], &elf[sh[4]], sh[5]);
            }
        }
    }
    Sha1 a, b;
    a.add(rom.data(), rom.size());
    b.add(menu.data(), menu.size());
    const std::string executable = a.hex(), overlay = b.hex();
    sEnglish = executable == "a4776fbadd2d266f42cf559a3530c2e6454d01a3" && overlay == "1d9f8da80c200bb50e7df3210fff30827e5dae9c";
    if (!sEnglish && (executable != kRomSha1 || overlay != kDbzpSha1)) {
        throw Stop{"Unsupported BT4 disc profile. Use the LATINO or English B14 Rev2 ISO.\n"
                   "Both executable and overlay must match one of the verified profiles."};
    }
    sVerify = sEnglish ? kVerifyEng : kVerify;
    sVerifyCount = sEnglish ? kVerifyEngCount : kVerifyCount;
    sPrograms[0] = std::move(rom); sPrograms[1] = std::move(menu);
    say(sEnglish ? "@ok BT4 B14 Rev2 English: verified programs" : "@ok BT4 B14 Rev2 LATINO: verified programs");
}


Uint32 file_crc(const std::string &path, Uint64 size, std::vector<Uint8> &buf) {
    SDL_IOStream *f = SDL_IOFromFile(path.c_str(), "rb");
    if (!f) return ~0u;
    if (SDL_GetIOSize(f) != (Sint64)size) { SDL_CloseIO(f); return ~0u; }
    Uint32 crc = 0;
    while (size) {
        size_t n = size < buf.size() ? (size_t)size : buf.size();
        if (SDL_ReadIO(f, buf.data(), n) != n) { SDL_CloseIO(f); return ~0u; }
        crc = SDL_crc32(crc, buf.data(), n); size -= n; check_cancel();
    }
    SDL_CloseIO(f); return crc;
}

// Link only after the source matches the selected disc's size and CRC.
// This keeps shared assets in one physical copy without mixing differing files.
bool share_file(const std::string &source,const std::string &destination) {
#ifdef _WIN32
    int a=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.c_str(),-1,NULL,0);
    int b=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,destination.c_str(),-1,NULL,0);
    if(a<1||b<1)return false;
    std::vector<WCHAR> src(a),dst(b);
    MultiByteToWideChar(CP_UTF8,0,source.c_str(),-1,src.data(),a);
    MultiByteToWideChar(CP_UTF8,0,destination.c_str(),-1,dst.data(),b);
    return CreateHardLinkW(dst.data(),src.data(),NULL)!=0;
#else
    return link(source.c_str(),destination.c_str())==0;
#endif
}
void select_data_root(const std::string &root) {
    std::string base=root+"gamedata/";size_t bytes=0;
    char *tag=(char *)SDL_LoadFile((base+".bt4-language").c_str(),&bytes);
    bool baseExists=SDL_GetPathInfo((base+"disc/SLUS_219.78").c_str(),NULL);
    bool baseEnglish=tag&&bytes>=2&&!strncmp(tag,"en",2);SDL_free(tag);
    if(baseExists&&!SDL_GetPathInfo((base+".bt4-language").c_str(),NULL)) {
        tag=(char *)SDL_LoadFile((base+".installed-bt4-b14rev2").c_str(),&bytes);
        baseEnglish=tag&&std::string(tag,bytes).find("ENG")!=std::string::npos;SDL_free(tag);
    }
    sDataRoot=base;sDedupeRoot.clear();
    if(baseExists&&sEnglish!=baseEnglish){
        sDataRoot=base+"languages/"+(sEnglish?"en/":"es/");sDedupeRoot=base;
    }
    SDL_CreateDirectory(sDataRoot.c_str());
    const char *locale=sEnglish?"en\n":"es\n";
    if(!SDL_SaveFile((sDataRoot+".bt4-language").c_str(),locale,3))throw Stop{"Could not save the language profile."};
}

void checked_out(Iso &iso, Uint64 off, Uint64 size, const std::string &path,
                 unsigned expectedSize, unsigned expectedCrc, std::vector<Uint8> &buf) {
    if (size != expectedSize) throw Stop{"Tamano incompatible en " + path};
    if (file_crc(path, size, buf) == expectedCrc) return;
    std::string temp = path + ".part";
    if(!sDedupeRoot.empty() && path.compare(0,sDataRoot.size(),sDataRoot)==0) {
        std::string shared=sDedupeRoot+path.substr(sDataRoot.size());
        if(file_crc(shared,expectedSize,buf)==expectedCrc) {
            SDL_RemovePath(temp.c_str());
            if(share_file(shared,temp)) {
                if(!SDL_RenamePath(temp.c_str(),path.c_str()))throw Stop{"Could not finalize the shared language asset."};
                return;
            }
        }
    }
    SDL_IOStream *f = SDL_IOFromFile(temp.c_str(), "wb");
    if (!f) throw Stop{"No se puede escribir en " + temp};
    Uint32 crc = 0;
    try {
        while (size) {
            size_t n = size < buf.size() ? (size_t)size : buf.size();
            iso.read(off, buf.data(), n);
            crc = SDL_crc32(crc, buf.data(), n);
            if (SDL_WriteIO(f, buf.data(), n) != n) throw Stop{"No se pudo guardar " + temp};
            off += n; size -= n; check_cancel();
        }
    } catch (...) { SDL_CloseIO(f); throw; }
    if (!SDL_CloseIO(f)) throw Stop{"No se pudo cerrar " + temp};
    if (crc != expectedCrc) throw Stop{"Los datos de la ISO no coinciden con BT4 B14 Rev2: " + path};
    if (!SDL_RenamePath(temp.c_str(), path.c_str())) throw Stop{"No se pudo finalizar " + path};
}

void step_data(Iso &iso, const std::string &root) {
    step("Game data");
    const IsoFile *index = iso.find("BIN/DBZ4.BIN");
    if (!index || index->size > 16*1024*1024) throw Stop{"Falta el indice DBZ4.BIN de BT4."};
    std::vector<Uint8> db((size_t)index->size), buf(1024*1024);
    iso.read(index->offset, db.data(), db.size());
    Sha1 hash; hash.add(db.data(), db.size());
    const char *indexHash = sEnglish ? "145837274421c39ae7eedc3fa1b697d4212541c2" : "989ee93cbd31508717bc245793c1e89c50109e0f";
    if (hash.hex() != indexHash) throw Stop{"DBZ4.BIN does not match the selected BT4 B14 Rev2 profile."};
    std::string data = sDataRoot.empty() ? root + "gamedata/" : sDataRoot;
    SDL_CreateDirectory(data.c_str());
    int entry = 0, percent = -1, files = 0;
    const int countAll = sVerifyCount[0]+sVerifyCount[1]+sVerifyCount[2]+sVerifyCount[3]+sizeof(kVerifyLoose)/sizeof(kVerifyLoose[0]);
    auto progress = [&]() {
        files++;
        int now = files*100/countAll;
        if (now != percent) {
            char line[128]; snprintf(line,sizeof(line),"@note %d%% - %d / %d archivos verificados",now,files,countAll);
            say(line); percent = now;
        }
        check_cancel();
    };
    for (int part=0; part<4; part++) {
        char upperName[64], lowerName[32];
        snprintf(upperName,sizeof(upperName),"DATA/PZS4US%d.AFS",part);
        snprintf(lowerName,sizeof(lowerName),"pzs4us%d.afs",part);
        const IsoFile *afs=iso.find(upperName);
        if (!afs) throw Stop{"Falta " + std::string(upperName)};
        auto found=std::search(db.begin(),db.end(),lowerName,lowerName+strlen(lowerName)+1);
        if (found==db.end() || found-db.begin()<16) throw Stop{"Indice BT4 incompleto."};
        size_t at=(size_t)(found-db.begin())-16;
        Uint32 header[4]; memcpy(header,db.data()+at,16);
        Uint32 count=header[2];
        if (header[0]!=0 || count!=(Uint32)sVerifyCount[part] || header[3]!=count || header[1]!=0x11A+count*2 || at+header[1]>db.size())
            throw Stop{"Tabla de sectores BT4 incompatible."};
        std::string dir=data+"pzs4us"+std::to_string(part)+"/";
        SDL_CreateDirectory(dir.c_str());
        Uint64 offset=0;
        for (Uint32 i=0;i<count;i++,entry++) {
            Uint16 sectors; memcpy(&sectors,db.data()+at+0x11A+i*2,2);
            Uint64 bytes=(Uint64)sectors*2048;
            if (offset+bytes>afs->size) {
                if (i!=count-1 || offset>=afs->size || offset+bytes-afs->size>=2048) throw Stop{"Contenedor BT4 truncado."};
                bytes=afs->size-offset;
            }
            char name[32]; snprintf(name,sizeof(name),"%05u.bin",i);
            checked_out(iso,afs->offset+offset,bytes,dir+name,sVerify[entry][0],sVerify[entry][1],buf);
            offset+=(Uint64)sectors*2048; progress();
        }
    }
    for (const char *loose:kVerifyLoose) {
        const IsoFile *f=iso.find(loose+5);
        if (!f) throw Stop{"Falta " + std::string(loose+5)};
        std::string path=data+loose;
        SDL_CreateDirectory(path.substr(0,path.rfind('/')).c_str());
        checked_out(iso,f->offset,f->size,path,sVerify[entry][0],sVerify[entry][1],buf);
        entry++; progress();
    }
    say("@ok " + std::to_string(files) + " BT4 files extracted and verified with CRC-32");
}

// Recompute only the small native-table fingerprint, using the strictly verified
// programs. Destinations remain those generated for this exact linked executable.
void step_tables(const std::string &root) {
    step("Native data tables");
    size_t bytes = 0;
    Uint8 *loaded = (Uint8 *)SDL_LoadFile((root + "Tenkaichi3Decomp.dat").c_str(), &bytes);
    if (!loaded) throw Stop{"The matching native data table is missing."};
    std::vector<Uint8> table(loaded, loaded + bytes); SDL_free(loaded);
    Uint32 count = 0;
    if (bytes < 16 || memcmp(table.data(), "BT3D", 4)) throw Stop{"Invalid native data table."};
    memcpy(&count, table.data()+4, 4);
    if (count > 100000 || bytes != 16 + (size_t)count * 16) throw Stop{"Invalid native data table length."};
    Uint64 hash = 0xcbf29ce484222325ULL;
    for (Uint32 i=0; i<count; i++) {
        Uint32 record[4]; memcpy(record, table.data()+16+(size_t)i*16, 16);
        if (record[1]>1 || (Uint64)record[2]+record[3]>sPrograms[record[1]].size()) throw Stop{"Native data table exceeds the selected program."};
        for (Uint32 j=0; j<record[3]; j++) hash=(hash^sPrograms[record[1]][record[2]+j])*0x100000001b3ULL;
    }
    memcpy(table.data()+8, &hash, 8);
    std::string temporary = root + "Tenkaichi3Decomp.dat.part";
    if (!SDL_SaveFile(temporary.c_str(), table.data(), table.size()) ||
        !SDL_RenamePath(temporary.c_str(), (root+"Tenkaichi3Decomp.dat").c_str())) throw Stop{"Could not finalize the native data table."};
    say("@ok Native table matches the selected disc profile");
}

void step_test(const std::string &root) {
    std::string exe = root + kGame, saves = root + "gamedata/.selftest";
    const char *args[2] = {exe.c_str(), NULL};
    step("Self-test");
    say("@note the game's demo fight, no window");
    SDL_Environment *env = SDL_CreateEnvironment(true);
    SDL_SetEnvironmentVariable(env, "BT3_DEMO", "1", true);
    SDL_SetEnvironmentVariable(env, "BT3_SETTINGS", "", true);
    SDL_SetEnvironmentVariable(env, "BT3_SAVES", saves.c_str(), true);
    SDL_SetEnvironmentVariable(env, "BT3_GS", "none", true);
    SDL_SetEnvironmentVariable(env, "BT3_NOSOUND", "1", true);
    SDL_SetEnvironmentVariable(env, "BT4_MUSIC_NO_CACHE", "1", true);
    SDL_SetEnvironmentVariable(env, "BT3_DATA", sDataRoot.c_str(), true); // no window: a release program opens one by default
    for (const char *name : {"BT3_REPLAY", "BT3_PAD_PLAY", "BT3_PAD_REC"}) {
        SDL_UnsetEnvironmentVariable(env, name);
    }
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, root.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process *proc = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDL_DestroyEnvironment(env);
    if (proc == NULL) {
        throw Stop{std::string("The game could not be started: ") + SDL_GetError()};
    }
    size_t size = 0;
    int code = -1;
    char *out = (char *)SDL_ReadProcess(proc, &size, &code);
    std::string text = out != NULL ? std::string(out, size) : "";
    SDL_free(out);
    SDL_DestroyProcess(proc);
    // How the game ended goes into the log too: when it never got as far as printing anything (the system refused
    // to start it, or something stopped it), the exit code is all there is to go on.
    char ended[160];
    const char *why = "";
    switch ((unsigned)code) {
    case 0xC0000135u: why = " (a DLL the program needs was not found)"; break;
    case 0xC0000018u: why = " (the program could not be loaded at its fixed address)"; break;
    case 0xC0000022u: why = " (access denied: often security software blocking the program)"; break;
    case 0xC000007Bu: why = " (a DLL of the wrong kind was loaded)"; break;
    case 0xC0000005u: why = " (crashed: bad memory access)"; break;
    case 0xC0000409u: why = " (stopped by a security check)"; break;
    default: break;
    }
    snprintf(ended, sizeof(ended), "\n[setup] the game exited with code %d (0x%08X)%s\n", code, (unsigned)code, why);
    text += ended;
    {   // a crash report the game wrote itself
        size_t n = 0;
        char *crash = (char *)SDL_LoadFile((root + "bt3_crash.txt").c_str(), &n);
        if (crash != NULL) {
            text += "[setup] bt3_crash.txt:\n" + std::string(crash, n);
            SDL_free(crash);
        }
    }
    SDL_CreateDirectory((root + "net_session/setup/").c_str());
    SDL_IOStream *log = SDL_IOFromFile((root + "net_session/setup/install.log").c_str(), "wb");
    if (log != NULL) {
        SDL_WriteIO(log, text.data(), text.size());
        SDL_CloseIO(log);
    }
    if (text.find("battle finished") == std::string::npos) {
        size_t end = text.find_last_not_of("\r\n"), begin = end == std::string::npos ? std::string::npos : text.rfind('\n', end);
        std::string last = end == std::string::npos ? "no output" : text.substr(begin == std::string::npos ? 0 : begin + 1, end - (begin == std::string::npos ? 0 : begin + 1) + 1);
        throw Stop{"The game's demo fight did not finish. Its last line was:\n" + last + "\nThe full output is in net_session/setup/install.log."};
    }
    say("@ok the demo fight ran to its end");
}

// The European release's archive entries: the battle HUD (5 to 9, one per language: English, Spanish, German,
// French, Italian) and the menu archives (USA 448 + k is 451 + k + 31 per language).
std::vector<Uint32> eu_entries() {
    static const Uint32 kUsa[] = {448, 449, 450, 451, 452, 453, 459, 460, 461, 463, 472};
    std::vector<Uint32> out;
    for (Uint32 bank = 0; bank < 5; bank++) {
        out.push_back(5 + bank);
        for (Uint32 usa : kUsa) {
            out.push_back(451 + (usa - 448) + bank * 31);
        }
    }
    return out;
}

void step_eu(const std::string &root, const std::string &path) {
    Iso iso;
    std::string dir = root + "gamedata/pzs3eu1/";
    step("European languages");
    iso.open(path);
    const IsoFile *sles = iso.find("SLES_549.45"), *afs = iso.find("DATA/PZS3EU1.AFS");
    if (sles == NULL || afs == NULL) {
        throw Stop{"This is not a disc image of the European release of Budokai Tenkaichi 3 (no SLES_549.45 on it).\n"
                   "The languages come from the European release (SLES-54945); the game itself is set up from the USA one."};
    }
    Uint8 head[8];
    Uint32 count;
    iso.read(afs->offset, head, 8);
    memcpy(&count, head + 4, 4);
    if (memcmp(head, "AFS\0", 4) != 0 || count == 0 || (Uint64)count * 8 + 8 > afs->size) {
        throw Stop{"An archive on the European disc image (DATA/PZS3EU1.AFS) is damaged."};
    }
    std::vector<Uint32> table((size_t)count * 2);
    iso.read(afs->offset + 8, table.data(), table.size() * 4);
    std::vector<Uint8> buf(1 << 20);
    std::vector<Uint32> want = eu_entries();
    SDL_CreateDirectory(dir.c_str());
    int done = 0;
    for (Uint32 e : want) {
        char name[32], line[64];
        if (e >= count || (Uint64)table[e * 2] + table[e * 2 + 1] > afs->size) {
            throw Stop{"An archive on the European disc image (DATA/PZS3EU1.AFS) is damaged or not the European release's."};
        }
        snprintf(name, sizeof(name), "%05u", e);
        copy_out(iso, afs->offset + table[e * 2], table[e * 2 + 1], dir + name + ".part", buf);
        if (!SDL_RenamePath((dir + name + ".part").c_str(), (dir + name + ".bin").c_str())) {
            throw Stop{"Could not write " + dir + name + ".bin."};
        }
        snprintf(line, sizeof(line), "@note %d of %d", ++done, (int)want.size());
        say(line);
        check_cancel();
    }
    say("@ok Spanish, German, French and Italian: the game makes them at its next start");
}

#include "private_packages.inc"
#include "language_install.inc"

bool installed_language(const std::string &root,bool english){
    bool base=base_english(root);std::string data=root+"gamedata/"+(base==english?"":english?"languages/en/":"languages/es/");
    if(base!=english&&!SDL_GetPathInfo((data+".installed-bt4-b14rev2").c_str(),NULL))return false;
    const int *counts=english?kVerifyEngCount:kVerifyCount;
    const unsigned int (*verify)[2]=english?kVerifyEng:kVerify;
    int offset=counts[0]+counts[1]+counts[2]+counts[3],checked=0;std::vector<Uint8> buf(1024*1024);
    for(int i=0;i<(int)(sizeof(kVerifyLoose)/sizeof(kVerifyLoose[0]));++i){
        std::string relative=kVerifyLoose[i];
        if(relative=="disc/SLUS_219.78"||relative=="disc/BIN/DBZ4.BIN"||relative=="disc/BIN/DBZP.BIN"){
            if(file_crc(data+relative,verify[offset+i][0],buf)!=verify[offset+i][1])return false;++checked;
        }
    }
    return checked==3;
}
void work(std::string root, std::string path, std::string eu) {
    try {
        sStep=0;sSteps=(path.empty()?0:4)+(sInstallMask==3?2:0);
        if(!path.empty()){
            Iso iso;step_disc(iso,path);check_cancel();
            if(sInstallMask!=3&&sInstallMask!=(sEnglish?2:1))throw Stop{"LANGUAGE_ISO_MISMATCH"};
            select_data_root(root);step_data(iso,root);check_cancel();step_tables(root);check_cancel();step_test(root);
            const char *profile=sEnglish?"BT4 B14 Rev2 ENG; SLUS_219.78; native profile 7\n":"BT4 B14 Rev2 LATINO; SLUS_219.78; native profile 7\n";
            if(!SDL_SaveFile((sDataRoot+".installed-bt4-b14rev2").c_str(),profile,strlen(profile)))throw Stop{"INSTALL_STATE_FAILED"};
        }else if(!Native_Installed(root))throw Stop{"ISO_REQUIRED"};
        bool base=base_english(root);
        if(sInstallMask==3){
            if(!installed_language(root,!base))install_language(root,!base);
            else {step("Language package");say("@package 100");say("@skip verified language is installed");}
        }else if(!installed_language(root,sInstallMask==2))throw Stop{"ISO_REQUIRED"};
        SDL_CreateDirectory((root+"tbo").c_str());
        std::string options=std::to_string(sInstallMask)+"\n";
        if(!SDL_SaveFile((root+"tbo/languages.ini").c_str(),options.data(),options.size()))throw Stop{"INSTALL_STATE_FAILED"};
        const char *identity=BT4_BUILD_IDENTITY;
        SDL_SaveFile((root+BT4_BUILD_MARKER).c_str(),identity,strlen(identity));
        say("@done "+root+kGame);
    }catch(const Stop &e){
        SDL_CreateDirectory((root+"tbo/setup").c_str());
        SDL_SaveFile((root+"tbo/setup/install.log").c_str(),e.why.data(),e.why.size());
        std::string why=e.why;for(size_t i=0;(i=why.find('\n',i))!=std::string::npos;)why.replace(i,1,"\\n");
        say("@fail");say("@stopped "+why);
    }catch(...){say("@fail");say("@stopped INSTALL_FAILED");}
    sRunning=false;
}

} // namespace

void Native_Start(const std::string &root, const std::string &iso, const std::string &eu,int mask) {
    if (sThread.joinable()) {
        sThread.join();
    }
    sInstallMask=mask>=1&&mask<=3?mask:1;
    sCancel = false;
    sRunning = true;
    sThread = std::thread(work, root, iso, eu);
}

bool Native_Poll(std::string &line) {
    std::lock_guard<std::mutex> lock(sLock);
    if (sLines.empty()) {
        return false;
    }
    line = sLines.front();
    sLines.erase(sLines.begin());
    return true;
}

bool Native_Running() { return sRunning; }

void Native_Cancel() { sCancel = true; }

void Native_Join() {
    sCancel = true;
    if (sThread.joinable()) {
        sThread.join();
    }
}

bool Native_Installed(const std::string &root) {
    SDL_PathInfo info;
    if(!SDL_GetPathInfo((root+"gamedata/disc/SLUS_219.78").c_str(),&info))return false;
    if(SDL_GetPathInfo((root+"gamedata/.installed-bt4-b14rev2").c_str(),&info))return true;
    // Older portable installations can add English without extracting their ISO again.
    std::vector<Uint8> buf(1024*1024);
    for(int en=0;en<2;++en){
        const int *counts=en?kVerifyEngCount:kVerifyCount;
        const unsigned int (*verify)[2]=en?kVerifyEng:kVerify;
        int off=counts[0]+counts[1]+counts[2]+counts[3];
        if(file_crc(root+"gamedata/disc/SLUS_219.78",verify[off+7][0],buf)==verify[off+7][1]&&
           file_crc(root+"gamedata/disc/BIN/DBZP.BIN",verify[off+1][0],buf)==verify[off+1][1])return true;
    }
    return false;
}

bool Native_EuInstalled(const std::string &root) { return true; }
bool Native_BundledEnglish(const std::string &root) { return false; }
bool Native_EnglishInstalled(const std::string &root) { return english_installed(root); }
