"""Configure the optional room relay without changing live routing or match RPCs.

Credentials arrive only through an Actions secret and are stored in Supabase
Vault. Default mode inspects readiness without changing either project.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
from urllib.request import Request, urlopen


PROJECTS = (
    ('primary', 'mhgksaiitqoijnatpurf', 'SUPABASE_PRIMARY_MANAGEMENT_TOKEN'),
    ('secondary', 'houhjsfukkkicjejvett', 'SUPABASE_SECONDARY_MANAGEMENT_TOKEN'),
)
CORE = """select p.oid::regprocedure::text as function,
 md5(pg_get_functiondef(p.oid)) as definition from pg_proc p
 join pg_namespace n on n.oid=p.pronamespace where n.nspname='public'
 and p.proname in ('bt4_api','bt4_ranked_api','bt4_route_health') order by 1"""
SCHEMA = """select to_regclass('bt4_private.rooms') is not null as rooms,
 to_regclass('bt4_private.members') is not null as members,
 to_regprocedure('bt4_private.budget(text,integer)') is not null as budget,
 to_regclass('vault.decrypted_secrets') is not null as vault,
 to_regprocedure('vault.create_secret(text,text,text,uuid)') is not null as create_secret,
 to_regprocedure('vault.update_secret(uuid,text,text,text,uuid)') is not null as update_secret"""
STATUS = """select to_regprocedure('public.bt4_turn_credentials(uuid)') is not null as rpc,
 to_regclass('bt4_private.turn_config') is not null as config"""


def query(ref, token, sql, read_only=False):
    data = json.dumps({'query': sql, 'read_only': read_only}).encode()
    request = Request(f'https://api.supabase.com/v1/projects/{ref}/database/query',
        data=data, headers={'Authorization': 'Bearer '+token, 'Content-Type': 'application/json'})
    try:
        with urlopen(request, timeout=40) as response:
            result = json.load(response)
        if not isinstance(result, list):
            raise ValueError('Unexpected management response')
        return result
    except Exception as error:
        # SQL text and server errors may contain Vault input. Never log them.
        raise RuntimeError(f'{ref}: database request failed ({type(error).__name__})') from None


def literal(value):
    return "'" + value.replace("'", "''") + "'"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    config = None
    if args.apply:
        config = json.loads(os.environ['BT4_TURN_MANAGED_CONFIG'])
        assert re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9.-]{0,251}[a-zA-Z0-9]', config['hostname'])
        assert type(config['udp_port']) is int and 1 <= config['udp_port'] <= 65535
        for key in ('username', 'credential'):
            assert isinstance(config[key], str) and 1 <= len(config[key]) <= 128
            if os.environ.get('GITHUB_ACTIONS') == 'true':
                masked = config[key].replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
                print('::add-mask::'+masked, flush=True)
        assert 'expires_at' not in config or (type(config['expires_at']) is int and config['expires_at'] > 0)
    migration = Path(__file__).resolve().parents[2] / 'supabase/migrations/20261110043000_room_turn_credentials.sql'
    source = migration.read_text(encoding='utf-8')
    print('Migration SHA256', hashlib.sha256(source.encode()).hexdigest(), flush=True)
    # Inspect both projects before attempting either mutation.
    before = {}
    for name, ref, key in PROJECTS:
        token = os.environ[key]
        dependencies, = query(ref, token, SCHEMA, True)
        assert all(dependencies.values()), f'{name}: relay prerequisites unavailable'
        before[name] = query(ref, token, CORE, True)
        assert len(before[name]) == 3, f'{name}: unexpected online RPC set'
        print(name, 'prerequisites verified', query(ref, token, STATUS, True), flush=True)
    for name, ref, key in PROJECTS:
        token = os.environ[key]
        if args.apply:
            # No DELETE, user/rating writes, route switches or online RPC replacements.
            query(ref, token, source)
            payload = {k: config[k] for k in ('username', 'credential', 'expires_at') if k in config}
            sql = f"""begin;
                set local lock_timeout='3s'; set local statement_timeout='20s';
                do $bt4$ declare existing uuid; begin
                  select id into existing from vault.secrets
                    where name='bt4_turn_managed_credentials';
                  if existing is null then
                    perform vault.create_secret({literal(json.dumps(payload))},
                      'bt4_turn_managed_credentials','Managed room TURN credentials');
                  else
                    perform vault.update_secret(existing,{literal(json.dumps(payload))},
                      'bt4_turn_managed_credentials','Managed room TURN credentials');
                  end if;
                end $bt4$;
                insert into bt4_private.turn_config(singleton,hostname,udp_port,provider,enabled)
                  values(true,{literal(config['hostname'])},{config['udp_port']},'managed',true)
                  on conflict(singleton) do update set hostname=excluded.hostname,
                    udp_port=excluded.udp_port,provider=excluded.provider,enabled=true;
                commit;"""
            query(ref, token, sql)
        status, = query(ref, token, STATUS, True)
        if status['rpc'] and status['config']:
            ready = query(ref, token, """select provider,enabled,
                exists(select 1 from vault.decrypted_secrets
                  where name='bt4_turn_managed_credentials') as credentials_present,
                (select relrowsecurity from pg_class where oid='bt4_private.turn_config'::regclass) as rls,
                not has_table_privilege('authenticated','bt4_private.turn_config','SELECT') as private_config,
                not has_function_privilege('anon','public.bt4_turn_credentials(uuid)','EXECUTE') as signed_in_only
                from bt4_private.turn_config where singleton""", True)
            print(name, 'relay status', ready, flush=True)
            if args.apply:
                assert len(ready) == 1 and all(ready[0][key] for key in
                    ('enabled','credentials_present','rls','private_config','signed_in_only'))
        assert query(ref, token, CORE, True) == before[name], f'{name}: online RPC definitions changed'
        print(name, 'existing online RPCs unchanged', flush=True)


if __name__ == '__main__':
    main()
