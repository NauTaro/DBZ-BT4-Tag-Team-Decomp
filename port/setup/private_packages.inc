/* Private release downloads use the official GitHub CLI and its own browser
 * authorization flow. No account token or password is shipped with BT4. */
#include <wincrypt.h>
#include "language_packages.h"
static int sInstallMask=1;
static std::string cli_run(const std::string &root,std::vector<std::string> args,
                           const std::string &progressFile="",Uint64 expected=0,bool login=false){
    const std::string gh=root+"tbo/setup/gh.exe";
    if(!SDL_GetPathInfo(gh.c_str(),NULL))throw Stop{"GITHUB_HELPER_MISSING"};
    args.insert(args.begin(),gh);std::vector<const char*> argv;for(auto &a:args)argv.push_back(a.c_str());argv.push_back(NULL);
    SDL_PropertiesID props=SDL_CreateProperties();
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,(void*)argv.data());
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props,SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN,true);
    SDL_SetStringProperty(props,SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,root.c_str());
    SDL_Process *p=SDL_CreateProcessWithProperties(props);SDL_DestroyProperties(props);
    if(!p)throw Stop{"GITHUB_HELPER_FAILED"};
    std::string output;int code=0,last=-1;bool sentEnter=false,shownCode=false;
    try{
        for(;;){
            char buf[1024];size_t n=SDL_ReadIO(SDL_GetProcessOutput(p),buf,sizeof(buf));
            if(n&&output.size()<32768)output.append(buf,n);
            if(login&&!shownCode){
                for(size_t i=0;i+9<=output.size();++i){
                    bool valid=output[i+4]=='-';for(int j=0;j<9&&valid;++j)if(j!=4)valid=(output[i+j]>='A'&&output[i+j]<='Z')||(output[i+j]>='0'&&output[i+j]<='9');
                    if(valid){say("@github "+output.substr(i,9));shownCode=true;break;}
                }
            }
            if(login&&shownCode&&!sentEnter){SDL_WriteIO(SDL_GetProcessInput(p),"\n",1);SDL_FlushIO(SDL_GetProcessInput(p));sentEnter=true;}
            if(expected){SDL_PathInfo info;if(SDL_GetPathInfo(progressFile.c_str(),&info)){
                int pct=(int)(info.size*70/expected);if(pct>70)pct=70;if(pct!=last){last=pct;say("@package "+std::to_string(pct));}}}
            if(SDL_WaitProcess(p,false,&code))break;
            check_cancel();SDL_Delay(80);
        }
        for(;;){char buf[1024];size_t n=SDL_ReadIO(SDL_GetProcessOutput(p),buf,sizeof(buf));if(!n)break;if(output.size()<32768)output.append(buf,n);}
        SDL_DestroyProcess(p);
    }catch(...){SDL_KillProcess(p,true);SDL_WaitProcess(p,true,&code);SDL_DestroyProcess(p);throw;}
    if(code)throw Stop{login?"GITHUB_LOGIN_FAILED":"GITHUB_ACCESS_REQUIRED"};
    return output;
}
static std::string sha256_file(const std::string &path){
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;SDL_IOStream *f=SDL_IOFromFile(path.c_str(),"rb");
    if(!f)throw Stop{"LANGUAGE_DOWNLOAD_INCOMPLETE"};
    struct Cleanup {
        HCRYPTPROV &provider; HCRYPTHASH &hash; SDL_IOStream *file;
        ~Cleanup(){if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);SDL_CloseIO(file);}
    } cleanup{provider,hash,f};
    bool ok=CryptAcquireContext(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
    std::vector<Uint8> buf(1024*1024);size_t n;
    while(ok&&(n=SDL_ReadIO(f,buf.data(),buf.size()))){check_cancel();ok=CryptHashData(hash,buf.data(),(DWORD)n,0)!=0;}
    Uint8 digest[32];DWORD size=32;ok=ok&&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0);
    if(!ok)throw Stop{"LANGUAGE_VERIFY_FAILED"};
    char out[65];for(int i=0;i<32;++i)snprintf(out+i*2,3,"%02x",digest[i]);return out;
}
static std::string download_language(const std::string &root,bool english){
    const LanguagePackage &package=kLanguagePackages[english?1:0];
    const std::string folder=root+"tbo/setup/cache/",path=folder+package.name;
    step("Language package");say("@package 0");
    try{cli_run(root,{"auth","status","--hostname","github.com"});}
    catch(const Stop &){
        check_cancel();
        say("@github login");
        cli_run(root,{"auth","login","--web","--hostname","github.com","--git-protocol","https","--scopes","repo"},"",0,true);
    }
    const std::string visibility=cli_run(root,{"api","repos/"+std::string(kLanguageRepository),"--jq",".private"});
    if(visibility.find("true")==std::string::npos)throw Stop{"GITHUB_ACCESS_REQUIRED"};
    SDL_CreateDirectory(folder.c_str());
    SDL_PathInfo info;bool good=SDL_GetPathInfo(path.c_str(),&info)&&info.size==package.bytes;
    if(good)good=sha256_file(path)==package.sha256;
    if(!good){
        SDL_RemovePath(path.c_str());
        try{cli_run(root,{"release","download",kLanguageRelease,"--repo",kLanguageRepository,"--pattern",package.name,"--dir",folder,"--clobber"},path,package.bytes);}
        catch(...){SDL_RemovePath(path.c_str());throw;}
        if(!SDL_GetPathInfo(path.c_str(),&info)||info.size!=package.bytes||sha256_file(path)!=package.sha256){SDL_RemovePath(path.c_str());throw Stop{"LANGUAGE_VERIFY_FAILED"};}
    }
    say("@package 75");return path;
}
