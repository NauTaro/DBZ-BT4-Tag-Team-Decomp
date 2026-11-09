"""Read-only project status and connection diagnostics. Never prints API keys."""
import json,os,re
from datetime import datetime,timedelta,timezone
from urllib.parse import urlencode
from urllib.request import Request,urlopen
from urllib.error import HTTPError

now=datetime.now(timezone.utc)
window={'iso_timestamp_start':(now-timedelta(minutes=40)).strftime('%Y-%m-%dT%H:%M:%SZ'),'iso_timestamp_end':now.strftime('%Y-%m-%dT%H:%M:%SZ')}
for name,ref,key in [('secondary','houhjsfukkkicjejvett','SUPABASE_SECONDARY_MANAGEMENT_TOKEN'),
                     ('primary','mhgksaiitqoijnatpurf','SUPABASE_PRIMARY_MANAGEMENT_TOKEN')]:
    headers={'Authorization':'Bearer '+os.environ[key],'Content-Type':'application/json'}
    logs="select source, event_message, count(*) as occurrences from logs where source in ('postgres_logs','auth_logs','realtime_logs','postgrest_logs') and (positionCaseInsensitive(event_message,'error')>0 or positionCaseInsensitive(event_message,'fatal')>0 or positionCaseInsensitive(event_message,'timeout')>0) group by source,event_message order by occurrences desc limit 25"
    for label,path,body in [('project','',None),('health','/health?services=auth,rest,realtime,db&timeout_ms=5000',None),('disk','/config/disk/util',None),('logs','/analytics/endpoints/logs?'+urlencode({**window,'sql':logs}),None),('database','/database/query',{'query':"select state,wait_event_type,count(*) from pg_stat_activity group by state,wait_event_type;"})]:
        request=Request('https://api.supabase.com/v1/projects/'+ref+path,
                        data=json.dumps(body).encode() if body else None,headers=headers)
        try:
            with urlopen(request,timeout=40) as response:result=json.load(response)
            if label=='project':result={k:result.get(k) for k in ('id','name','status','region')}
            safe=json.dumps(result)
            safe=re.sub(r'[\w.+-]+@[\w.-]+','[email]',safe)
            safe=re.sub(r'\b(?:\d{1,3}\.){3}\d{1,3}\b','[ip]',safe)
            safe=re.sub(r'eyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+','[jwt]',safe)
            print(name,label,safe[:18000],flush=True)
        except HTTPError as exc:
            try:error=json.load(exc);message={k:error.get(k) for k in ('message','error','error_code','code') if k in error}
            except Exception:message={'message':'Non-JSON error response'}
            print(name,label,'HTTP '+str(exc.code),json.dumps(message),flush=True)
        except Exception as exc:print(name,label,type(exc).__name__,flush=True)
