#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR args,int show){
    wchar_t path[32768],root[32768];(void)instance;(void)previous;
    if(!GetModuleFileNameW(NULL,path,32768))return 2;
    wchar_t *end=wcsrchr(path,L'\\');if(!end)return 2;end[1]=0;
    if(wcslen(path)+80>=32768)return 2;
    wcscpy(root,path);wcscat(path,L"tbo\\setup\\BT4-Setup.exe");
    if(args&&!wcscmp(args,L"--check"))return GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES?0:3;
    if((INT_PTR)ShellExecuteW(NULL,L"open",path,args,root,show)<=32){
        MessageBoxW(NULL,L"Extrae el ZIP completo antes de abrir Install.exe.\nExtract the complete ZIP before opening Install.exe.",L"BT4",MB_OK|MB_ICONERROR);return 3;
    }
    return 0;
}
