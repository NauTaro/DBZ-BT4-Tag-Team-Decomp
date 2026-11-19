// Positive client identification only. IP address, ASN, iCloud, Darwin and
// CFNetwork alone cannot distinguish an iPhone port from supported desktops.
export function disallowedClient(client){
  if(!client||typeof client!=='object')return false;
  const platform=typeof client.platform==='string'?client.platform.toLowerCase().trim():'';
  const app=typeof client.app==='string'?client.app.toLowerCase().trim():'';
  return ['ios','ipados','iphone','ipad','ipod'].includes(platform)||
    /^(?:tenkaichi3decompios|bt3onlineios|bt3online-ios|sliu772\/tenkaichi3decompios)$/.test(app);
}
export function disallowedRequest(request){
  const path=new URL(request.url).pathname;
  if(!['/v5/api','/v5/socket','/v5/auth/start','/v5/auth/refresh','/v5/auth/exchange'].includes(path))return false;
  const ua=request.headers.get('User-Agent')||'';
  if(/\b(?:iPhone|iPad|iPod|Tenkaichi3DecompIOS|BT3OnlineIOS|BT3Online-iOS)\b/i.test(ua))return true;
  const platform=request.headers.get('Sec-CH-UA-Platform')?.replaceAll('"','').toLowerCase().trim();
  return platform==='ios'||platform==='ipados'||disallowedClient({platform:request.headers.get('X-BT4-Client-Platform')||''});
}
