-- The TURN provider carries game traffic. Supabase only serves room credentials.
begin;

create schema if not exists extensions;
create extension if not exists pgcrypto with schema extensions;

create table if not exists bt4_private.turn_config (
  singleton boolean primary key default true check (singleton),
  hostname text not null check (hostname ~ '^[a-zA-Z0-9][a-zA-Z0-9.-]{0,251}[a-zA-Z0-9]$'),
  udp_port integer not null default 3478 check (udp_port between 1 and 65535),
  enabled boolean not null default false
);
alter table bt4_private.turn_config add column if not exists provider text not null default 'coturn'
  check (provider in ('coturn','managed'));
alter table bt4_private.turn_config enable row level security;
revoke all on bt4_private.turn_config from public, anon, authenticated;

create or replace function public.bt4_turn_credentials(p_generation uuid) returns jsonb
language plpgsql security definer set search_path='' as $$
declare
  who uuid := auth.uid();
  config bt4_private.turn_config%rowtype;
  shared_secret text;
  managed jsonb;
  expires_at bigint;
  login text;
begin
  if who is null or not exists (
    select 1 from bt4_private.rooms r
      join bt4_private.members m on m.room_id=r.id and m.user_id=who
    where r.generation=p_generation and r.expires_at>clock_timestamp()
      and exists (select 1 from bt4_private.members other where other.room_id=r.id and other.user_id<>who)
  ) then
    return jsonb_build_object('error','NOT_A_MEMBER');
  end if;

  select * into config from bt4_private.turn_config where singleton and enabled;
  if not found then return jsonb_build_object('state','disabled'); end if;

  begin
    perform bt4_private.budget('turn_credentials', 6);
  exception when sqlstate '54000' then
    return jsonb_build_object('error','RATE_LIMITED');
  end;

  if to_regclass('vault.decrypted_secrets') is null then
    return jsonb_build_object('state','disabled');
  end if;

  if config.provider='managed' then
    begin
      select decrypted_secret::jsonb into managed from vault.decrypted_secrets
        where name='bt4_turn_managed_credentials' limit 1;
      expires_at := (managed->>'expires_at')::bigint;
    exception when invalid_text_representation or numeric_value_out_of_range then
      return jsonb_build_object('state','disabled');
    end;

    if expires_at is null or expires_at<=extract(epoch from clock_timestamp())+120
      or expires_at>4294967295
      or coalesce(jsonb_typeof(managed->'username'),'')<>'string'
      or coalesce(jsonb_typeof(managed->'credential'),'')<>'string'
      or length(managed->>'username') not between 1 and 128
      or length(managed->>'credential') not between 1 and 128 then
      return jsonb_build_object('state','disabled');
    end if;

    return jsonb_build_object('state','ready','hostname',config.hostname,'udp_port',config.udp_port,
      'username',managed->>'username','credential',managed->>'credential','expires_at',expires_at);
  end if;

  select decrypted_secret into shared_secret from vault.decrypted_secrets
    where name='bt4_turn_shared_secret' limit 1;
  if coalesce(length(shared_secret),0)<32 then return jsonb_build_object('state','disabled'); end if;

  -- Stable during the same hour so rejoining doesn't allocate a new quota.
  expires_at := (floor(extract(epoch from clock_timestamp())/3600)::bigint+6)*3600;
  login := expires_at::text || ':' || who::text;
  return jsonb_build_object('state','ready','hostname',config.hostname,'udp_port',config.udp_port,
    'username',login,'credential',encode(extensions.hmac(login,shared_secret,'sha1'),'base64'),
    'expires_at',expires_at);
end $$;
revoke all on function public.bt4_turn_credentials(uuid) from public, anon, authenticated;
grant execute on function public.bt4_turn_credentials(uuid) to authenticated;

notify pgrst,'reload schema';
commit;
