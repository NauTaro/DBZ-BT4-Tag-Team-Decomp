import {fulfillProfilePass,profilePassPrice,revokeProfilePass} from './online_profile_cards.mjs';

const product='bt4_profile_30_days';
const encoder=new TextEncoder();

export function profileCheckoutEnabled(env) {
    const key=env.STRIPE_PROFILE_SECRET_KEY;
    return typeof key==='string'&&/^(sk|rk)_(live|test)_/.test(key)&&
        typeof env.STRIPE_PROFILE_WEBHOOK_SECRET==='string'&&env.STRIPE_PROFILE_WEBHOOK_SECRET.startsWith('whsec_');
}

export async function profileCheckout(env,user,request=fetch) {
    if (!profileCheckoutEnabled(env)) throw new Error('PROFILE_CHECKOUT_UNAVAILABLE');
    const returnUrl=env.STRIPE_PROFILE_RETURN_URL;
    if (typeof returnUrl!=='string'||!returnUrl.startsWith('https://')) throw new Error('PROFILE_CHECKOUT_UNAVAILABLE');
    const body=new URLSearchParams({
        mode:'payment',success_url:returnUrl,cancel_url:returnUrl,client_reference_id:user,
        'adaptive_pricing[enabled]':'false','payment_method_types[0]':'card',
        'line_items[0][price_data][currency]':'usd','line_items[0][price_data][unit_amount]':String(profilePassPrice),
        'line_items[0][price_data][product_data][name]':'BT4 profile pass (30 days)',
        'line_items[0][quantity]':'1','metadata[product]':product,
        'payment_intent_data[metadata][product]':product
    });
    const response=await request('https://api.stripe.com/v1/checkout/sessions',{
        method:'POST',headers:{Authorization:'Bearer '+env.STRIPE_PROFILE_SECRET_KEY},body,signal:AbortSignal.timeout(10000)
    });
    if (!response.ok) throw new Error('PROFILE_CHECKOUT_UNAVAILABLE');
    const {url}=await response.json();
    if (typeof url!=='string'||!url.startsWith('https://checkout.stripe.com/'))
        throw new Error('PROFILE_CHECKOUT_UNAVAILABLE');
    return {url};
}

export async function verifyProfileWebhook(secret,header,raw,now) {
    const fields=(header||'').split(',').map(part=>part.trim());
    const timestamps=fields.filter(part=>part.startsWith('t='));
    const timestamp=timestamps[0]?.slice(2);
    if (timestamps.length!==1||!/^\d{1,12}$/.test(timestamp)||Math.abs(now/1000-Number(timestamp))>300)
        throw new Error('BAD_PROFILE_SIGNATURE');
    const key=await crypto.subtle.importKey('raw',encoder.encode(secret),{name:'HMAC',hash:'SHA-256'},false,['verify']);
    const message=encoder.encode(timestamp+'.'+raw);
    for (const part of fields) {
        if (!/^v1=[a-f0-9]{64}$/.test(part)) continue;
        const signature=Uint8Array.from(part.slice(3).match(/../g),hex=>parseInt(hex,16));
        if (await crypto.subtle.verify('HMAC',key,signature,message)) return JSON.parse(raw);
    }
    throw new Error('BAD_PROFILE_SIGNATURE');
}

export function applyProfileWebhook(core,env,event) {
    const live=env.STRIPE_PROFILE_SECRET_KEY.includes('_live_');
    const payment=event?.data?.object;
    if (!payment||event.livemode!==live||payment.livemode!==live) return false;
    if (event.type==='charge.refunded') {
        if (payment.refunded!==true||payment.metadata?.product!==product||
            !/^pi_[a-zA-Z0-9]+$/.test(payment.payment_intent)) return false;
        revokeProfilePass(core,payment.payment_intent);
        return true;
    }
    if (!['checkout.session.completed','checkout.session.async_payment_succeeded'].includes(event.type)||
        payment.object!=='checkout.session'||payment.mode!=='payment'||payment.payment_status!=='paid'||
        payment.amount_total!==profilePassPrice||payment.currency!=='usd'||
        payment.metadata?.product!==product||!/^pi_[a-zA-Z0-9]+$/.test(payment.payment_intent)) return false;
    fulfillProfilePass(core,{user_id:payment.client_reference_id,payment_id:payment.payment_intent,
        amount_minor:payment.amount_total,currency:payment.currency,paid_at:event.created*1000});
    return true;
}
