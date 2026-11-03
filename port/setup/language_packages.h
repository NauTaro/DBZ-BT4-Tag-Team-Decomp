#pragma once
// Pinned optional packages in the BT4 project repository.
struct LanguagePackage{const char *name;Uint64 bytes;const char *sha256;};
static const char *kLanguageRepository="NauTaro/DBZ-BT4-Tag-Team-Decomp";
static const char *kLanguageRelease="bt4-language-packs-b14rev2-v1";
static const char *kTextureRelease="bt4-hd-textures-v1.07";
static const LanguagePackage kTexturePackage={"dbz bt4 HD v1.07.zip",3690378076ull,"f129248fcd0d508a5e901e6901109726fd2b38e1d57812b9f8ba743aafdd4129"};
static const LanguagePackage kTexturePieces[]={{"BT4-HD-v1.07.zip.001",1850000000ull,"d7e55aecaa05377eee7d46ff6f8dc34d8797fd5dbad49cbfc418f6b596b82763"},{"BT4-HD-v1.07.zip.002",1840378076ull,"8a1e24dbb333a220bb96d5b69c5daec670837238df1e960c204bb35afff55d2c"}};
static const LanguagePackage kLanguagePackages[]={{"spanish-b14rev2.bt4lang",1403690328ull,"8a23297ad0019e08837360b26da417f717b9dbd52042897bffb87044625579d5"},{"english-b14rev2.bt4lang",1907429425ull,"043dc5bdc88a60fc7af86b36f0d098823960621998c868fa9775926b1d360cf1"}};
static const LanguagePackage kPortuguesePackage={"portuguese-b14rev2.bt4lang",59065456ull,"100f751998cfcf1687e30c9444e13360757cc8fb2fff0d940aa673b5fe9cc8cf"};
