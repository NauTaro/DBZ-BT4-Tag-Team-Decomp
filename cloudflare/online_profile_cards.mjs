export const profilePassPrice = 300;
export const officialDonorLifetime = Number.MAX_SAFE_INTEGER;
const formats = {avatar:[2048,1],motion:[2048,6],banner:[8192,1],banner_motion:[2048,6]};
export const profileAnimationRequestLimit = 2400000;
const matchId = /^[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}$/;

export function profileMedia(core, user, premium) {
    const media = {};
    for (const {kind,id} of core.rows('SELECT kind,id FROM online_profile_media WHERE user_id=?',user))
        if (kind==='avatar'||premium) media[kind]=id;
    if (premium) {
        const animation=core.one('SELECT id FROM online_profile_banner_motion WHERE user_id=?',user);
        if(animation)media.banner_motion=animation.id;
    }
    return media;
}

export function profileCard(row, now) {
    const premium = (row?.premium_until ?? 0) > now;
    const name = row?.name_color ?? 0;
    const background = row?.background ?? 0;
    return {
        name_color: name < 8 ? name : 0,
        ...(name >= 0x1000000 ? {name_rgb:name - 0x1000000} : {}),
        background: background < 8 ? background : 0,
        ...(background >= 0x1000000 ? {banner_rgb:background - 0x1000000} : {}),
        decoration: premium ? (row?.decoration ?? 0) : 0,
        premium
    };
}

export function ownProfileCard(core, user, changes) {
    const row = core.one('SELECT * FROM online_profile_cards WHERE user_id=?', user);
    const premium = (row?.premium_until ?? 0) > core.now();
    if (changes !== undefined) {
        if (!changes || typeof changes !== 'object' || Array.isArray(changes) ||
            Object.keys(changes).some(key => !['name_color','name_rgb','background','banner_rgb','decoration'].includes(key)))
            throw new Error('BAD_PROFILE_CARD');
        for (const [key, limit] of [['name_color', 7], ['name_rgb', 0xffffff], ['background', 7], ['banner_rgb', 0xffffff], ['decoration', 6]]) {
            if (Object.hasOwn(changes, key) &&
                (!Number.isInteger(changes[key]) || changes[key] < 0 || changes[key] > limit))
                throw new Error('BAD_PROFILE_CARD');
        }
        if (changes.decoration > 0 && !premium) throw new Error('PROFILE_PASS_REQUIRED');
        core.run(`INSERT INTO online_profile_cards(user_id,name_color,background,decoration) VALUES(?,?,?,?)
            ON CONFLICT(user_id) DO UPDATE SET name_color=excluded.name_color,
            background=excluded.background,decoration=excluded.decoration`, user,
            changes.name_rgb===undefined ? (changes.name_color ?? row?.name_color ?? 0) : 0x1000000+changes.name_rgb,
            changes.banner_rgb===undefined ? (changes.background ?? row?.background ?? 0) : 0x1000000+changes.banner_rgb,
            changes.decoration ?? row?.decoration ?? 0);
        return ownProfileCard(core, user);
    }
    return {...profileCard(row, core.now()), media:profileMedia(core,user,premium),premium_until: row?.premium_until ?? 0};
}

function validAvatarDetail(encoded,maxSide=768,maxBytes=96000) {
    if(typeof encoded!=='string'||!encoded.length||encoded.length>4*Math.ceil(maxBytes/3)||encoded.length%4||
       !/^[A-Za-z0-9+/]+={0,2}$/.test(encoded))return false;
    const bytes=atob(encoded);
    if(bytes.length>maxBytes)return false;
    const square=(w,h)=>w===h&&w>=1&&w<=maxSide;
    const byte=i=>bytes.charCodeAt(i);
    if(bytes.length>=33&&bytes.startsWith('\x89PNG\r\n\x1a\n')&&bytes.slice(12,16)==='IHDR')
        return square(byte(16)*16777216+byte(17)*65536+byte(18)*256+byte(19),byte(20)*16777216+byte(21)*65536+byte(22)*256+byte(23));
    const jpeg=jpegDimensions(bytes);
    return jpeg!==null&&square(...jpeg);
}

function jpegDimensions(bytes) {
    const byte=i=>bytes.charCodeAt(i);
    if(byte(0)!==0xff||byte(1)!==0xd8)return null;
    for(let cursor=2;cursor+4<=bytes.length;){
        if(byte(cursor++)!==0xff)return null;
        const marker=byte(cursor++), length=byte(cursor)*256+byte(cursor+1);
        if(length<2||cursor+length>bytes.length)return null;
        if(marker===0xc0&&length>=8)
            return [byte(cursor+5)*256+byte(cursor+6),byte(cursor+3)*256+byte(cursor+4)];
        if(marker===0xda||marker===0xd9)return null;
        cursor+=length;
    }
    return null;
}

function validProfileAnimation(kind,animation) {
    if(!animation||typeof animation!=='object'||Array.isArray(animation))return false;
    const {width,height,delays,atlas}=animation;
    if(kind!=='motion'&&kind!=='banner_motion')return false;
    if(![96,128,160,192,224,256].includes(kind==='motion'?width:width/2)||
       height!==(kind==='motion'?width:width/4))return false;
    if(!Array.isArray(delays)||delays.length<2||delays.length>120||
       delays.some(delay=>!Number.isInteger(delay)||delay<20||delay>240000)||
       delays.reduce((sum,delay)=>sum+delay,0)>240000)return false;
    if(typeof atlas!=='string'||!atlas.length||atlas.length>2320000||atlas.length%4||
       !/^[A-Za-z0-9+/]+={0,2}$/.test(atlas))return false;
    const dimensions=jpegDimensions(atob(atlas));
    return dimensions!==null&&dimensions[0]===width*Math.min(8,delays.length)&&
        dimensions[1]===height*Math.ceil(delays.length/8);
}

export function saveProfileMedia(core,user,{kind,frames,delays,remove,detail,animation}) {
    if (typeof kind!=='string'||!Object.hasOwn(formats,kind)) throw new Error('BAD_PROFILE_MEDIA');
    const animatedBanner=kind==='banner_motion';
    if (remove===true) {
        if(animatedBanner)core.run('DELETE FROM online_profile_banner_motion WHERE user_id=?',user);
        else if(kind==='avatar')core.run("DELETE FROM online_profile_media WHERE user_id=? AND kind IN ('avatar','motion')",user);
        else core.run('DELETE FROM online_profile_media WHERE user_id=? AND kind=?',user,kind);
        return ownProfileCard(core,user);
    }
    const card=ownProfileCard(core,user);
    if (kind!=='avatar'&&!card.premium) throw new Error('PROFILE_PASS_REQUIRED');
    const [standardBytes,maxFrames]=formats[kind];
    if (!Array.isArray(frames)||frames.length<1||frames.length>maxFrames||
        !Array.isArray(delays)||delays.length!==frames.length)
        throw new Error('BAD_PROFILE_MEDIA');
    for (let i=0;i<frames.length;i++) {
        const frame=frames[i],delay=delays[i];
        const bytes=kind==='avatar'&&typeof frame==='string'&&frame.length===17068?12800:standardBytes;
        if (typeof frame!=='string'||frame.length!==4*Math.ceil(bytes/3)||!/^[A-Za-z0-9+/]+={1,2}$/.test(frame)||
            atob(frame).length!==bytes||!Number.isInteger(delay)||delay<100||delay>1000)
            throw new Error('BAD_PROFILE_MEDIA');
    }
    if (animatedBanner&&frames.length<2) throw new Error('BAD_PROFILE_MEDIA');
    if(detail!==undefined&&(kind!=='avatar'||!validAvatarDetail(detail)))throw new Error('BAD_PROFILE_MEDIA');
    if(animation!==undefined&&!validProfileAnimation(kind,animation))throw new Error('BAD_PROFILE_MEDIA');
    const now=core.now(),id=core.uuid();
    const old=animatedBanner?core.one('SELECT updated FROM online_profile_banner_motion WHERE user_id=?',user):
        core.one('SELECT updated FROM online_profile_media WHERE user_id=? AND kind=?',user,kind);
    if (old && now-old.updated<5000) throw new Error('RATE_LIMITED');
    const body=JSON.stringify({kind,frames,delays,...(detail?{detail}:{}),
        ...(animation?{animation:{width:animation.width,height:animation.height,delays:animation.delays,atlas:animation.atlas}}:{})});
    if(animatedBanner)core.run(`INSERT INTO online_profile_banner_motion VALUES(?,?,?,?)
        ON CONFLICT(user_id) DO UPDATE SET id=excluded.id,body=excluded.body,updated=excluded.updated`,user,id,body,now);
    else core.run(`INSERT INTO online_profile_media VALUES(?,?,?,?,?)
        ON CONFLICT(user_id,kind) DO UPDATE SET id=excluded.id,body=excluded.body,updated=excluded.updated`,
        user,kind,id,body,now);
    if(kind==='avatar')core.run("DELETE FROM online_profile_media WHERE user_id=? AND kind='motion'",user);
    return ownProfileCard(core,user);
}

export function getProfileMedia(core,user,{id,hd,detail,animation}) {
    if (typeof id!=='string'||!matchId.test(id)) throw new Error('BAD_PROFILE_MEDIA');
    const row=core.one(`SELECT media.user_id,media.kind,media.body,c.premium_until FROM (
            SELECT user_id,kind,body FROM online_profile_media WHERE id=?
            UNION ALL SELECT user_id,'banner_motion',body FROM online_profile_banner_motion WHERE id=?
        ) media LEFT JOIN online_profile_cards c ON c.user_id=media.user_id
        WHERE (media.user_id=? OR EXISTS(
            SELECT 1 FROM online_members a JOIN online_members b ON a.room=b.room
            JOIN online_rooms r ON a.room=r.id
            WHERE a.user_id=? AND b.user_id=media.user_id AND r.expires>?
        ))`,id,id,user,user,core.now());
    if (!row || (row.kind!=='avatar' && !(row.premium_until>core.now()))) throw new Error('PROFILE_MEDIA_UNAVAILABLE');
    const media=JSON.parse(row.body);
    if(detail!==2&&(detail!==true||!validAvatarDetail(media.detail,320,11200)))delete media.detail;
    if(animation!==2&&(animation!==true||media.animation?.atlas.length>1160000||
       media.animation?.width>(row.kind==='motion'?192:384)))delete media.animation;
    if(row.kind==='avatar'&&hd!==true&&media.frames[0].length===17068){
        const high=atob(media.frames[0]);
        let low='';
        for(let y=0;y<32;y++){
            const sy=Math.floor((y+0.5)*80/32);
            for(let x=0;x<32;x++){
                const sx=Math.floor((x+0.5)*80/32), offset=(sy*80+sx)*2;
                low+=high.slice(offset,offset+2);
            }
        }
        media.frames[0]=btoa(low);
    }
    return {id,...media};
}

// This operation is called only by the administrator after verifying a paid purchase.
// The payment reference is unique even when fulfillment is retried.
export function fulfillProfilePass(core, purchase) {
    const {user_id, payment_id, amount_minor, currency, paid_at} = purchase ?? {};
    if (typeof user_id !== 'string' || !core.one('SELECT id FROM online_users WHERE id=?', user_id) ||
        typeof payment_id !== 'string' || !/^[a-zA-Z0-9_-]{6,120}$/.test(payment_id) ||
        amount_minor !== profilePassPrice || currency !== 'usd' ||
        !Number.isSafeInteger(paid_at) || paid_at <= 0 || paid_at > core.now())
        throw new Error('BAD_PROFILE_PAYMENT');
    const prior = core.one('SELECT user_id,paid_at FROM online_profile_payments WHERE payment_id=?', payment_id);
    if (prior && prior.user_id !== user_id)
        throw new Error('BAD_PROFILE_PAYMENT');
    if (!prior) {
        core.run('INSERT INTO online_profile_payments VALUES(?,?,?)', payment_id, user_id, paid_at);
        refreshPassExpiry(core,user_id);
    }
    return ownProfileCard(core, user_id);
}

function refreshPassExpiry(core,user) {
    const paid=core.one(`SELECT 1 FROM online_profile_payments p WHERE p.user_id=?
        AND NOT EXISTS(SELECT 1 FROM online_profile_refunds r WHERE r.payment_id=p.payment_id) LIMIT 1`,user);
    const grant=core.one('SELECT until_ms FROM online_profile_grants WHERE user_id=?',user)?.until_ms ?? 0;
    core.run(`INSERT INTO online_profile_cards(user_id,premium_until) VALUES(?,?)
        ON CONFLICT(user_id) DO UPDATE SET premium_until=excluded.premium_until`,user,paid?officialDonorLifetime:grant);
}

export function activateExistingDonors(core) {
    core.run(`INSERT INTO online_profile_cards(user_id,premium_until)
        SELECT DISTINCT p.user_id,? FROM online_profile_payments p
        WHERE NOT EXISTS(SELECT 1 FROM online_profile_refunds r WHERE r.payment_id=p.payment_id)
        ON CONFLICT(user_id) DO UPDATE SET premium_until=excluded.premium_until
        WHERE online_profile_cards.premium_until<>excluded.premium_until`,officialDonorLifetime);
}

export function grantProfilePass(core,{user_id,until_ms}={}) {
    if(typeof user_id!=='string'||!core.one('SELECT id FROM online_users WHERE id=?',user_id)||
       !Number.isSafeInteger(until_ms)||until_ms<=core.now())throw new Error('BAD_PROFILE_GRANT');
    core.run(`INSERT INTO online_profile_grants(user_id,until_ms) VALUES(?,?)
        ON CONFLICT(user_id) DO UPDATE SET until_ms=MAX(until_ms,excluded.until_ms)`,user_id,until_ms);
    refreshPassExpiry(core,user_id);
    return ownProfileCard(core,user_id);
}

export function revokeProfilePass(core,payment) {
    core.run('INSERT OR IGNORE INTO online_profile_refunds VALUES(?)',payment);
    const receipt=core.one('SELECT user_id FROM online_profile_payments WHERE payment_id=?',payment);
    if (receipt) refreshPassExpiry(core,receipt.user_id);
}
