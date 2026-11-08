"""Prepare R5 result reporting on both backends without opening Ranked admission."""
import json
import os
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError

ROOT = Path(__file__).resolve().parents[2]
PROJECTS = (("primary", "mhgksaiitqoijnatpurf", "SUPABASE_PRIMARY_MANAGEMENT_TOKEN"),
            ("secondary", "houhjsfukkkicjejvett", "SUPABASE_SECONDARY_MANAGEMENT_TOKEN"))

def query(ref, secret, sql):
    request = Request("https://api.supabase.com/v1/projects/" + ref + "/database/query",
        data=json.dumps({"query": sql}).encode(),
        headers={"Authorization": "Bearer " + secret, "Content-Type": "application/json"}, method="POST")
    try:
        with urlopen(request, timeout=60) as response:
            result = json.load(response)
    except HTTPError as exc:
        raise RuntimeError("Management query failed with HTTP " + str(exc.code)) from None
    if not isinstance(result, list):
        raise RuntimeError("Unexpected Management response")
    return result

for name, ref, key in PROJECTS:
    secret = os.environ.get(key)
    if not secret:
        raise RuntimeError("Missing management credential for " + name)
    state = query(ref, secret, "select exists(select 1 from information_schema.columns where table_schema='bt4_private' and table_name='ranked_config' and column_name='authority_backend') as ready, to_regclass('bt4_private.ranked_players') is not null as foundation")
    if not state[0]["foundation"]:
        raise RuntimeError("Ranked foundation not installed on " + name)
    if not state[0]["ready"]:
        query(ref, secret, (ROOT / "supabase/migrations/202611080005_ranked_admission_reports.sql").read_text())
    result = query(ref, secret, "select enabled,authority_backend,to_regclass('bt4_private.ranked_reports') is not null as reports,has_function_privilege('authenticated','public.bt4_ranked_api(text,jsonb)','execute') as authenticated_api from bt4_private.ranked_config where singleton")
    if len(result) != 1 or result[0]["authority_backend"] != 1 or not result[0]["reports"] or not result[0]["authenticated_api"] or result[0]["enabled"]:
        raise RuntimeError("Unexpected staged Ranked state on " + name)
    print(name + ": R5 reports ready, authority backend 1, admission disabled", flush=True)
