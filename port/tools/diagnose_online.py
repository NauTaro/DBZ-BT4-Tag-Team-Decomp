"""Read-only project status and connection diagnostics. Never prints API keys."""
import json,os
from urllib.request import Request,urlopen
from urllib.error import HTTPError

for name,ref,key in [('primary','mhgksaiitqoijnatpurf','SUPABASE_PRIMARY_MANAGEMENT_TOKEN'),
                     ('secondary','houhjsfukkkicjejvett','SUPABASE_SECONDARY_MANAGEMENT_TOKEN')]:
    headers={'Authorization':'Bearer '+os.environ[key],'Content-Type':'application/json'}
    for label,path,body in [('project','',None),('database','/database/query',{'query':"select state,wait_event_type,count(*) from pg_stat_activity group by state,wait_event_type;"})]:
        request=Request('https://api.supabase.com/v1/projects/'+ref+path,
                        data=json.dumps(body).encode() if body else None,headers=headers)
        try:
            with urlopen(request,timeout=40) as response:result=json.load(response)
            if label=='project':result={k:result.get(k) for k in ('id','name','status','region')}
            print(name,label,json.dumps(result),flush=True)
        except HTTPError as exc:
            try:error=json.load(exc);message={k:error.get(k) for k in ('message','error','error_code','code') if k in error}
            except Exception:message={'message':'Non-JSON error response'}
            print(name,label,'HTTP '+str(exc.code),json.dumps(message),flush=True)
        except Exception as exc:print(name,label,type(exc).__name__,flush=True)
