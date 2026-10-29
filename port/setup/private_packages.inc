/* Private release downloads use the official GitHub CLI and its own browser
 * authorization flow. No account token or password is shipped with BT4. */
#include "language_packages.h"
static int sInstallMask=1;
static std::string cli_run(const std::string &root,std::vector<std::string> args,
                           const std::string &progressFile="",Uint64 expected=0,bool login=false){
    std::string gh=root+"tbo/setup/gh.exe";
#ifdef _WIN32
    if(!SDL_GetPathInfo(gh.c_str(),NULL))throw Stop{"GITHUB_HELPER_MISSING"};
#else
    gh="gh"; // optional language downloads use the user's system GitHub CLI
#endif
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
                int pct=(int)(info.size*55/expected);if(pct>55)pct=55;if(pct!=last){last=pct;say("@package "+std::to_string(pct));}}}
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
    SDL_IOStream *f=SDL_IOFromFile(path.c_str(),"rb");
    if(!f)throw Stop{"LANGUAGE_DOWNLOAD_INCOMPLETE"};
    struct Cleanup {SDL_IOStream *file;~Cleanup(){SDL_CloseIO(file);}} cleanup{f};
    UpdSha256 hash;UpdSha256_Init(&hash);
    std::vector<Uint8> buf(1024*1024);size_t n;
    Uint64 read=0;Sint64 expected=SDL_GetIOSize(f);int last=-1;
    say("@activity verify-package");
    while((n=SDL_ReadIO(f,buf.data(),buf.size()))){
        check_cancel();UpdSha256_Add(&hash,buf.data(),n);read+=n;
        int pct=expected>0?55+(int)(read*10/(Uint64)expected):55;
        if(pct!=last){last=pct;say("@package "+std::to_string(pct));}
    }
    if(expected<0||read!=(Uint64)expected)throw Stop{"LANGUAGE_VERIFY_FAILED"};
    Uint8 digest[32];UpdSha256_End(&hash,digest);
    char out[65];for(int i=0;i<32;++i)snprintf(out+i*2,3,"%02x",digest[i]);return out;
}
static std::string download_language(const std::string &root,bool english){
    const LanguagePackage &package=kLanguagePackages[english?1:0];
    const std::string folder=root+"tbo/setup/cache/",path=folder+package.name;
    step("Language package");say("@package 0");say("@activity download");
    try{cli_run(root,{"auth","status","--hostname","github.com"});}
    catch(const Stop &){
        check_cancel();
        say("@github login");
        cli_run(root,{"auth","login","--web","--hostname","github.com","--git-protocol","https","--scopes","repo"},"",0,true);
    }
    SDL_CreateDirectory(folder.c_str());
    SDL_PathInfo info;bool good=SDL_GetPathInfo(path.c_str(),&info)&&info.size==package.bytes;
    if(good)good=sha256_file(path)==package.sha256;
    if(!good){
        SDL_RemovePath(path.c_str());say("@activity download");say("@package 0");
        try{cli_run(root,{"release","download",kLanguageRelease,"--repo",kLanguageRepository,"--pattern",package.name,"--dir",folder,"--clobber"},path,package.bytes);}
        catch(...){SDL_RemovePath(path.c_str());throw;}
        if(!SDL_GetPathInfo(path.c_str(),&info)||info.size!=package.bytes||sha256_file(path)!=package.sha256){SDL_RemovePath(path.c_str());throw Stop{"LANGUAGE_VERIFY_FAILED"};}
    }
    say("@package 65");return path;
}
