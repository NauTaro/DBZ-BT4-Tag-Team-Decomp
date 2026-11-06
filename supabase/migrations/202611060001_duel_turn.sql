begin;
-- Admission records only: provider keys and temporary TURN credentials never enter SQL.
create table bt4_private.turn_grants (
  generation uuid primary key,
  user_id uuid not null references auth.users(id) on delete cascade,
  issued_at timestamptz not null default clock_timestamp()
);
create index turn_grants_daily on bt4_private.turn_grants(issued_at,user_id);
alter table bt4_private.turn_grants enable row level security;
revoke all on bt4_private.turn_grants from public,anon,authenticated;

create function public.bt4_turn_admission(p_generation uuid) returns jsonb
language plpgsql security definer set search_path='' as $$
declare who uuid:=auth.uid(); rid uuid;
begin
  if who is null then return jsonb_build_object('error','AUTH_REQUIRED'); end if;
  select r.id into rid from bt4_private.rooms r
    where r.owner=who and r.generation=p_generation and r.mode in ('duel','lobby','tournament')
      and r.expires_at>clock_timestamp()
      and (select count(*) from bt4_private.members m where m.room_id=r.id)>1;
  if rid is null then return jsonb_build_object('error','NOT_OWNER'); end if;
  -- Serialize issuance so concurrent anonymous identities cannot race the global ceiling.
  perform pg_advisory_xact_lock(42440601);
  if exists(select 1 from bt4_private.turn_grants where generation=p_generation)
     or (select count(*) from bt4_private.turn_grants where issued_at>=date_trunc('day',clock_timestamp() at time zone 'UTC') at time zone 'UTC')>=100
     or (select count(*) from bt4_private.turn_grants where user_id=who and issued_at>=date_trunc('day',clock_timestamp() at time zone 'UTC') at time zone 'UTC')>=10 then
    return jsonb_build_object('error','RATE_LIMITED');
  end if;
  insert into bt4_private.turn_grants(generation,user_id) values(p_generation,who);
  return jsonb_build_object('allowed',true);
end $$;
revoke all on function public.bt4_turn_admission(uuid) from public,anon;
grant execute on function public.bt4_turn_admission(uuid) to authenticated;

-- Preserve all existing validation, including voice revisions, on the ordinary path.
alter function public.bt4_signal(uuid,integer,text,text,jsonb) rename to bt4_signal_without_turn;
create function public.bt4_signal(p_generation uuid,p_to integer,p_channel text,p_kind text,p_body jsonb) returns jsonb
language plpgsql security definer set search_path='' as $$
declare r bt4_private.rooms%rowtype; sender integer; recipient uuid; item jsonb; addr text; relays integer:=0;
begin
  if p_kind is distinct from 'udp' or p_channel is distinct from 'duel'
     or jsonb_typeof(p_body->'addresses') is distinct from 'array' then
    return public.bt4_signal_without_turn(p_generation,p_to,p_channel,p_kind,p_body);
  end if;
  if not exists(select 1 from jsonb_array_elements(p_body->'addresses') a where a#>>'{}' like 'relay=%') then
    return public.bt4_signal_without_turn(p_generation,p_to,p_channel,p_kind,p_body);
  end if;
  if octet_length(p_body::text)>16000 or jsonb_array_length(p_body->'addresses') not between 1 and 4
     or p_body-'addresses'-'nonce'<>'{}'::jsonb or p_body->>'nonce' is distinct from replace(p_generation::text,'-','') then
    raise exception 'BAD_SIGNAL';
  end if;
  for item in select value from jsonb_array_elements(p_body->'addresses') loop
    if jsonb_typeof(item)<>'string' or length(item#>>'{}')>=64 then raise exception 'BAD_SIGNAL'; end if;
    addr:=item#>>'{}';
    if addr like 'relay=%' then relays:=relays+1; addr:=substr(addr,7); end if;
    if addr !~ '^[0-9]{1,3}(\.[0-9]{1,3}){3}:[0-9]{1,5}$'
       or split_part(addr,':',2)::integer not between 1 and 65535
       or family(split_part(addr,':',1)::inet)<>4 then raise exception 'BAD_SIGNAL'; end if;
  end loop;
  if relays<>1 then raise exception 'BAD_SIGNAL'; end if;
  select r2.* into r from bt4_private.rooms r2 join bt4_private.members m on m.room_id=r2.id
    where m.user_id=auth.uid() and r2.generation=p_generation and r2.expires_at>clock_timestamp();
  if not found then raise exception 'NOT_A_MEMBER'; end if;
  select peer into sender from bt4_private.members where room_id=r.id and user_id=auth.uid();
  if sender<>1 then raise exception 'NOT_OWNER'; end if;
  select user_id into recipient from bt4_private.members where room_id=r.id and peer=p_to and user_id<>auth.uid();
  if recipient is null then raise exception 'BAD_RECIPIENT'; end if;
  perform bt4_private.inbox(recipient,'bt4.signal',jsonb_build_object('version',1,'room',r.id,'generation',r.generation,
    'from',sender,'to',p_to,'channel',p_channel,'kind',p_kind,'body',p_body));
  return jsonb_build_object('version',1,'state','sent');
end $$;
revoke all on function public.bt4_signal(uuid,integer,text,text,jsonb),public.bt4_signal_without_turn(uuid,integer,text,text,jsonb) from public,anon,authenticated;
commit;
