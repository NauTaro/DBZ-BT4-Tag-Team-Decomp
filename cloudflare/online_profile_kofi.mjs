import {fulfillProfilePass,profilePassPrice} from './online_profile_cards.mjs';

const recipient = 'https://ko-fi.com/nautaro';
const codePattern = /^BT4-[A-F0-9]{24}$/;
const checkoutLifetime = 24 * 60 * 60 * 1000;

export function kofiEnabled(env) {
    return typeof env.KOFI_PROFILE_TOKEN === 'string' && env.KOFI_PROFILE_TOKEN.length >= 16 &&
        /^https:\/\/[a-z0-9-]+(?:\.[a-z0-9-]+)+$/i.test(env.KOFI_PROFILE_ORIGIN ?? '');
}

export function kofiCheckout(core, env, user) {
    const now = core.now();
    core.run('DELETE FROM online_profile_kofi_orders WHERE expires < ?', now - checkoutLifetime);
    const existing = core.one('SELECT code FROM online_profile_kofi_orders WHERE user_id=? AND expires>=? AND payment_id IS NULL ORDER BY made DESC LIMIT 1', user, now);
    const code = existing?.code ?? 'BT4-' + crypto.randomUUID().replaceAll('-', '').slice(0, 24).toUpperCase();
    if (!existing) core.run('INSERT INTO online_profile_kofi_orders VALUES(?,?,?,?,NULL)', code, user, now, now + checkoutLifetime);
    return {url: env.KOFI_PROFILE_ORIGIN + '/v5/profile/kofi/' + code};
}

export function kofiPage(core, code, language) {
    if (!codePattern.test(code) || !core.one('SELECT code FROM online_profile_kofi_orders WHERE code=? AND expires>=? AND payment_id IS NULL', code, core.now())) return null;
    const es = /^es\b/i.test(language);
    const title = es ? 'Donador oficial de BT4' : 'BT4 Official Donor';
    const price = profilePassPrice / 100;
    const instructions = es
        ? `Copiá este código en el mensaje privado de tu donación única de ${price} USD. Tendrás acceso permanente a GIFs, banners y decoraciones. Después, volvé al juego y elegí Actualizar estado.`
        : `Copy this code into the private message of your one-time USD ${price} donation. You'll get lifetime access to GIFs, banners, and decorations. Then return to the game and select Refresh status.`;
    const label = es ? 'Abrir Ko-fi de NauTaro' : 'Open NauTaro on Ko-fi';
    return new Response(`<!doctype html><html lang="${es?'es':'en'}"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>${title}</title><style>body{font:1rem/1.6 system-ui;background:#12202e;color:#f4f6f8;margin:0;padding:2rem}main{max-width:34rem;margin:8vh auto}input,a{font:inherit;padding:.7rem;color:#161616;background:#fff;border:0;border-radius:.3rem}input{width:17rem;max-width:90%}a{display:inline-block;margin-top:1rem;background:#ffd783;text-decoration:none}</style><main><h1>${title}</h1><p>${instructions}</p><input aria-label="${es?'Código de compra':'Purchase code'}" readonly value="${code}"><br><a href="${recipient}" rel="noreferrer">${label}</a></main></html>`, {
        headers: {'Content-Type':'text/html; charset=utf-8', 'Cache-Control':'no-store', 'Referrer-Policy':'no-referrer',
            'Content-Security-Policy':"default-src 'none'; style-src 'unsafe-inline'; frame-ancestors 'none'; base-uri 'none'"}
    });
}

export function applyKofiPayment(core, event) {
    const amount = String(event.amount);
    if (event.type !== 'Donation' || event.is_subscription_payment !== false ||
        event.currency !== 'USD' || !/^\d+(?:\.\d{1,2})?$/.test(amount) || Math.round(Number(amount) * 100) !== profilePassPrice ||
        typeof event.kofi_transaction_id !== 'string' || !/^[0-9a-z_-]{6,100}$/i.test(event.kofi_transaction_id))
        return false;
    const paymentId = 'kofi_' + event.kofi_transaction_id.toLowerCase();
    if (core.one('SELECT payment_id FROM online_profile_payments WHERE payment_id=?', paymentId)) return true;
    const code = typeof event.message === 'string' ? event.message.trim().toUpperCase() : '';
    if (!codePattern.test(code)) return false;
    const order = core.one('SELECT user_id,made,expires,payment_id FROM online_profile_kofi_orders WHERE code=?', code);
    const paidAt = typeof event.timestamp === 'string' ? Date.parse(event.timestamp) : NaN;
    if (!order || order.payment_id || !Number.isSafeInteger(paidAt) || paidAt < order.made - 1000 ||
        paidAt > order.expires || paidAt > core.now()) return false;
    fulfillProfilePass(core,{user_id:order.user_id,payment_id:paymentId,paid_at:paidAt,amount_minor:profilePassPrice,currency:'usd'});
    core.run('UPDATE online_profile_kofi_orders SET payment_id=? WHERE code=?',paymentId,code);
    return true;
}
