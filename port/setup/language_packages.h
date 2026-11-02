#pragma once
// Pinned optional packages in the BT4 project repository.
struct LanguagePackage{const char *name;Uint64 bytes;const char *sha256;};
static const char *kLanguageRepository="NauTaro/DBZ-BT4-Tag-Team-Decomp";
static const char *kLanguageRelease="bt4-language-packs-b14rev2-v1";
static const char *kTextureRelease="bt4-hd-textures-v2";
static const LanguagePackage kTexturePackage={"BT4-HD-textures.zip",3153533409ull,"05d66d7f8fe891cd8734f7f3573c4618ccbef2e11b9eb61e5aeb385e7464e810"};
static const LanguagePackage kTexturePieces[]={{"BT4-HD-textures.zip.001",1850000000ull,"293505312ca355ce96f09336d0175f8859597f4eacb586da21005d9685738744"},{"BT4-HD-textures.zip.002",1303533409ull,"6bb013309820ad8ce50ba7ef46bf33fbfc53913c483a726a601ff1cf27864c9d"}};
static const LanguagePackage kLanguagePackages[]={{"spanish-b14rev2.bt4lang",1403690328ull,"8a23297ad0019e08837360b26da417f717b9dbd52042897bffb87044625579d5"},{"english-b14rev2.bt4lang",1907429425ull,"043dc5bdc88a60fc7af86b36f0d098823960621998c868fa9775926b1d360cf1"}};
static const LanguagePackage kPortuguesePackage={"portuguese-b14rev2.bt4lang",59065456ull,"100f751998cfcf1687e30c9444e13360757cc8fb2fff0d940aa673b5fe9cc8cf"};
