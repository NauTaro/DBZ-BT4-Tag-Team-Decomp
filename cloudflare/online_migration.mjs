const TABLES=new Set(['users','online_nicknames','ranked_config','ranked_seasons','ranked_players','ranked_profiles','ranked_matches','ranked_rating_history','ranked_reports','ranked_snapshots','online_nickname_config','banned_users']);
export function importRows(core,body,sync=false){
        if(!sync&&core.one("SELECT value FROM online_meta WHERE key='admission'")?.value==='open')throw new Error('IMPORT_REQUIRES_CLOSED_ADMISSION');
        const {source,table,rows,batch,digest}=body;
        if(!['primary','secondary'].includes(source)||!TABLES.has(table)||!Array.isArray(rows)||rows.length>200||typeof batch!=='string'||batch.length>128)throw new Error('BAD_REQUEST');
        const existing=core.one('SELECT digest FROM online_import_batches WHERE id=?',batch);
        if(existing){if(existing.digest!==digest)throw new Error('IMPORT_CONFLICT');return {state:'already_imported'};}
        const put=(q,...a)=>core.run(q,...a);
        for(const row of rows){
            const key=table==='users'?row.id:JSON.stringify([row.id??null,row.user_id??null,row.ruleset??null,row.season??null,row.match_id??null]);
            const prior=core.one('SELECT body FROM online_legacy WHERE source=? AND table_name=? AND row_key=?',source,table,key);
            const before=prior?JSON.parse(prior.body):null;
            if(sync&&table==='ranked_players'&&before?.updated_at&&row.updated_at&&Date.parse(before.updated_at)>Date.parse(row.updated_at))continue;
            put('INSERT INTO online_legacy VALUES(?,?,?,?) ON CONFLICT(source,table_name,row_key) DO UPDATE SET body=excluded.body',source,table,key,JSON.stringify(row));
            if(table==='users'){
                put('INSERT OR IGNORE INTO online_users VALUES(?,?)',row.id,Date.parse(row.created_at));
                put('INSERT INTO online_aliases VALUES(?,?,?) ON CONFLICT(source,subject) DO UPDATE SET user_id=excluded.user_id',source,row.id,row.id);
            }
            if(source==='secondary'&&table==='online_nicknames'){
                const name=row.name??row.nickname??row.display_name;
                if(typeof name!=='string'||typeof row.user_id!=='string')throw new Error('UNSUPPORTED_NAME_ROW');
                const norm=name.normalize('NFKC').trim().toLocaleLowerCase('en-US');
                const owner=core.one('SELECT user_id FROM online_names WHERE normalized=?',norm);
                if(owner&&owner.user_id!==row.user_id)throw new Error('IMPORTED_NICKNAME_CONFLICT');
                const current=core.name(row.user_id);
                if(!sync||!current||current===before?.name)put('INSERT INTO online_names VALUES(?,?,?) ON CONFLICT(user_id) DO UPDATE SET name=excluded.name,normalized=excluded.normalized',row.user_id,name,norm);
            }
            if(source==='secondary'&&table==='ranked_players'){
                const current=core.one('SELECT * FROM ranked_players WHERE user_id=? AND ruleset=?',row.user_id,row.ruleset);
                const rating=sync&&current?Math.max(0,Math.min(100000,current.rating+row.rating-(before?.rating??1000))):row.rating;
                const wins=sync&&current?Math.max(0,current.wins+row.wins-(before?.wins??0)):row.wins;
                const losses=sync&&current?Math.max(0,current.losses+row.losses-(before?.losses??0)):row.losses;
                put('INSERT INTO ranked_players VALUES(?,?,?,?,?,?) ON CONFLICT(user_id,ruleset) DO UPDATE SET name=excluded.name,rating=excluded.rating,wins=excluded.wins,losses=excluded.losses',row.user_id,row.ruleset,core.name(row.user_id)||row.name,rating,wins,losses);
            }
            if(source==='secondary'&&table==='ranked_profiles'&&row.ruleset==='solo'){
                const current=core.one('SELECT * FROM ranked_weekly WHERE season=? AND user_id=?',row.season,row.user_id);
                const wins=sync&&current?Math.max(0,current.wins+row.wins-(before?.wins??0)):row.wins;
                const losses=sync&&current?Math.max(0,current.losses+row.losses-(before?.losses??0)):row.losses;
                put('INSERT INTO ranked_weekly VALUES(?,?,?,?) ON CONFLICT(season,user_id) DO UPDATE SET wins=excluded.wins,losses=excluded.losses',row.season,row.user_id,wins,losses);
            }
        }
        put('INSERT INTO online_import_batches VALUES(?,?,?,?)',batch,source,digest,Date.now());return {state:'imported',rows:rows.length};
    }
