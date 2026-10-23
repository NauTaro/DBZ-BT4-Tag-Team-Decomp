/* Windows TLS WebSocket transport. All OS handles stay outside the PS2 heap. */
#ifdef _WIN32
#include <winhttp.h>
#include <SDL3/SDL.h>
#define TT_WS_RING 1024
static struct {
    CRITICAL_SECTION lock;int lockReady;HANDLE rx,tx;
    HINTERNET session,connection,request,websocket;
    volatile LONG stop,status;
    unsigned inRead,inWrite,outRead,outWrite;
    unsigned char incoming[TT_WS_RING][512],outgoing[TT_WS_RING][512];
    char url[512];
} sTagWs;
/* Closing may race a connection handshake. Publish every OS handle under the
 * same lock, so a cancelled handshake cannot leak or revive a closed session. */
static int TagWs_Publish(HINTERNET *slot,HINTERNET handle){
    int ok=0;if(!handle)return 0;
    EnterCriticalSection(&sTagWs.lock);
    if(!InterlockedCompareExchange(&sTagWs.stop,0,0)){*slot=handle;ok=1;}
    LeaveCriticalSection(&sTagWs.lock);
    if(!ok)WinHttpCloseHandle(handle);return ok;
}
static DWORD WINAPI TagWs_Rx(LPVOID unused){
    wchar_t url[512],host[256],path[256];URL_COMPONENTS parts;
    HINTERNET request=NULL;DWORD code=0,size=sizeof(code);
    (void)unused;if(InterlockedCompareExchange(&sTagWs.stop,0,0))return 0;memset(&parts,0,sizeof(parts));parts.dwStructSize=sizeof(parts);
    parts.lpszHostName=host;parts.dwHostNameLength=256;parts.lpszUrlPath=path;parts.dwUrlPathLength=256;
    MultiByteToWideChar(CP_UTF8,0,sTagWs.url,-1,url,512);
    if(!WinHttpCrackUrl(url,0,0,&parts))goto failed;
    if(!TagWs_Publish(&sTagWs.session,WinHttpOpen(L"BT4-Tag-Team/0.1.38.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)))goto failed;
    WinHttpSetTimeouts(sTagWs.session,5000,5000,5000,10000);
    if(!TagWs_Publish(&sTagWs.connection,WinHttpConnect(sTagWs.session,host,parts.nPort,0)))goto failed;
    request=WinHttpOpenRequest(sTagWs.connection,L"GET",path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,parts.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0);
    if(!TagWs_Publish(&sTagWs.request,request))goto failed;
    if(!WinHttpSetOption(request,WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,NULL,0) || !WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request,NULL))goto failed;
    WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX);
    if(code!=101)goto failed;
    if(!TagWs_Publish(&sTagWs.websocket,WinHttpWebSocketCompleteUpgrade(request,0)))goto failed;
    if(InterlockedCompareExchange(&sTagWs.stop,0,0))return 0;
    InterlockedExchange(&sTagWs.status,1);
    while(!InterlockedCompareExchange(&sTagWs.stop,0,0)){
        unsigned char packet[512];unsigned used=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE kind;DWORD got;
        do{
            if(WinHttpWebSocketReceive(sTagWs.websocket,packet+used,sizeof(packet)-used,&got,&kind)!=NO_ERROR)goto failed;
            used+=got;if(kind==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)goto failed;
            if(used>512 || (used==512 && kind!=WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE))goto failed;
        }while(kind==WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE);
        if(used!=512 || kind!=WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE)goto failed;
        EnterCriticalSection(&sTagWs.lock);
        if(sTagWs.inWrite-sTagWs.inRead>=TT_WS_RING){LeaveCriticalSection(&sTagWs.lock);goto failed;}
        memcpy(sTagWs.incoming[sTagWs.inWrite++%TT_WS_RING],packet,512);LeaveCriticalSection(&sTagWs.lock);
    }
    return 0;
failed:
    if(!InterlockedCompareExchange(&sTagWs.stop,0,0))InterlockedExchange(&sTagWs.status,-1);
    return 0;
}
static DWORD WINAPI TagWs_Tx(LPVOID unused){
    (void)unused;
    while(!InterlockedCompareExchange(&sTagWs.stop,0,0)){
        unsigned char packet[512];int pending=0;
        if(InterlockedCompareExchange(&sTagWs.status,0,0)<0)break;
        if(InterlockedCompareExchange(&sTagWs.status,0,0)==1){
            EnterCriticalSection(&sTagWs.lock);
            if(sTagWs.outRead<sTagWs.outWrite){memcpy(packet,sTagWs.outgoing[sTagWs.outRead++%TT_WS_RING],512);pending=1;}
            LeaveCriticalSection(&sTagWs.lock);
            if(pending && WinHttpWebSocketSend(sTagWs.websocket,WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,packet,512)!=NO_ERROR){InterlockedExchange(&sTagWs.status,-1);break;}
        }
        if(!pending)Sleep(1);
    }return 0;
}
static void TagWs_Close(void){
    HINTERNET websocket=NULL,request=NULL;
    InterlockedExchange(&sTagWs.stop,1);
    if(sTagWs.lockReady){
        EnterCriticalSection(&sTagWs.lock);
        websocket=sTagWs.websocket;request=sTagWs.request;
        LeaveCriticalSection(&sTagWs.lock);
    }
    if(websocket)WinHttpCloseHandle(websocket);
    if(request)WinHttpCloseHandle(request);
    if(sTagWs.tx){WaitForSingleObject(sTagWs.tx,INFINITE);CloseHandle(sTagWs.tx);}
    if(sTagWs.rx){WaitForSingleObject(sTagWs.rx,INFINITE);CloseHandle(sTagWs.rx);}
    if(sTagWs.connection)WinHttpCloseHandle(sTagWs.connection);
    if(sTagWs.session)WinHttpCloseHandle(sTagWs.session);
    if(sTagWs.lockReady)DeleteCriticalSection(&sTagWs.lock);
    memset(&sTagWs,0,sizeof(sTagWs));
}
static int TagWs_Open(const char *root,const char *code){
    TagWs_Close();InitializeCriticalSection(&sTagWs.lock);sTagWs.lockReady=1;
    snprintf(sTagWs.url,sizeof(sTagWs.url),"%s/v2/room/%s",root,code);
    sTagWs.rx=CreateThread(NULL,0,TagWs_Rx,NULL,0,NULL);sTagWs.tx=CreateThread(NULL,0,TagWs_Tx,NULL,0,NULL);
    if(!sTagWs.rx||!sTagWs.tx){TagWs_Close();return 0;}return 1;
}
static int TagWs_Send(const unsigned char *packet){
    int ok=1;if(!sTagWs.lockReady || InterlockedCompareExchange(&sTagWs.status,0,0)<0)return 0;
    EnterCriticalSection(&sTagWs.lock);
    if(sTagWs.outWrite-sTagWs.outRead>=TT_WS_RING)ok=0;
    else memcpy(sTagWs.outgoing[sTagWs.outWrite++%TT_WS_RING],packet,512);
    LeaveCriticalSection(&sTagWs.lock);return ok;
}
static int TagWs_Read(unsigned char *packet){
    int result=0;if(!sTagWs.lockReady)return 0;
    EnterCriticalSection(&sTagWs.lock);
    if(sTagWs.inRead<sTagWs.inWrite){memcpy(packet,sTagWs.incoming[sTagWs.inRead++%TT_WS_RING],512);result=1;}
    LeaveCriticalSection(&sTagWs.lock);
    return result?1:InterlockedCompareExchange(&sTagWs.status,0,0)<0?-1:0;
}
#endif
