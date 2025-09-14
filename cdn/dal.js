// cdn/dal.js  — WebSocket-first DAL (HTTP vervangen door WS-RPC + streams)
// ESM module
// Gebruik: import { DAL } from './dal.js';  DAL.init();  DAL.ws.on('now', cb);  await DAL.getNow();

export const DAL = (function DALFactory(){
  // =========================
  // Event-bus (per "type")
  // =========================
  const listeners = new Map(); // type -> Set<fn>
  function on(type, fn){ if(!listeners.has(type)) listeners.set(type, new Set()); listeners.get(type).add(fn); }
  function off(type, fn){ const set = listeners.get(type); if(set){ set.delete(fn); if(!set.size) listeners.delete(type); } }
  function emit(type, payload){ const set = listeners.get(type); if(set){ for(const fn of set) { try{ fn(payload);}catch(e){ console.error('[DAL] handler error', e);} } } }

  // =========================
  // WebSocket state
  // =========================
  let ws = null;
  let wantOpen = false;
  let reconnectAttempts = 0;
  let pingTimer = null, watchdogTimer = null;
  let lastPong = 0;
  let wsUrl = null;
  const sendQueue = [];

  function makeUrl(custom){
    if (custom) return custom;
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${proto}//${location.host}/ws`;
  }

  function startHeartbeat(){
    stopHeartbeat();
    lastPong = Date.now();
    // ping elke 20s, failover als >40s geen pong
    pingTimer = setInterval(()=> { trySend({type:'ping', data:{ts: Date.now()}}); }, 20000);
    watchdogTimer = setInterval(()=>{
      if (Date.now() - lastPong > 40000){
        console.warn('[DAL] WS heartbeat timeout, reconnecting');
        safeClose(true);
      }
    }, 10000);
  }
  function stopHeartbeat(){
    if (pingTimer) clearInterval(pingTimer), pingTimer=null;
    if (watchdogTimer) clearInterval(watchdogTimer), watchdogTimer=null;
  }

  function scheduleReconnect(){
    if (!wantOpen) return;
    const base = 500, max = 15000, jitter = Math.random()*200;
    const backoff = Math.min(max, base * (2 ** reconnectAttempts)) + jitter;
    setTimeout(connect, backoff);
  }

  function flushQueue(){
    while (ws && ws.readyState === WebSocket.OPEN && sendQueue.length){
      ws.send(sendQueue.shift());
    }
  }

  function safeClose(reconnect=false){
    stopHeartbeat();
    if (ws){
      try { ws.close(); } catch(_) {}
      ws = null;
    }
    if (reconnect) scheduleReconnect();
  }

  function handleMessage(ev){
    let msg;
    try { msg = JSON.parse(ev.data); }
    catch { return console.warn('[DAL] non-JSON WS payload'); }

    const { type, data, id } = msg || {};

    // Heartbeat
    if (type === 'pong'){ lastPong = Date.now(); return; }

    // RPC result / error
    if (type === 'rpc_result' || type === 'rpc_error'){
      resolveRpc(id, type === 'rpc_result', data);
      return;
    }

    // Broadcast events ("now", "raw_telegram", "log", ...)
    if (type) emit(type, data);
    emit('message', msg);
  }

  function trySend(obj){
    const raw = JSON.stringify(obj);
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(raw);
    else sendQueue.push(raw);
  }

  function connect(customUrl){
    wantOpen = true;
    wsUrl = makeUrl(customUrl || wsUrl);
    try {
      ws = new WebSocket(wsUrl);
      ws.addEventListener('open', ()=>{
        reconnectAttempts = 0;
        startHeartbeat();
        flushQueue();
        emit('open');
      });
      ws.addEventListener('message', handleMessage);
      ws.addEventListener('close', ()=>{
        emit('close');
        stopHeartbeat();
        if (wantOpen){
          reconnectAttempts++;
          scheduleReconnect();
        }
      });
      ws.addEventListener('error', (e)=>{
        emit('error', e);
        safeClose(true);
      });
    } catch(e){
      emit('error', e);
      scheduleReconnect();
    }
  }

  function disconnect(){
    wantOpen = false;
    safeClose(false);
  }

  // =========================
  // RPC laag (vervanging HTTP)
  // =========================
  let nextRpcId = 1;
  const pending = new Map(); // id -> {resolve, reject, tmr}

  function resolveRpc(id, ok, data){
    const slot = pending.get(id);
    if (!slot) return;
    clearTimeout(slot.tmr);
    pending.delete(id);
    ok ? slot.resolve(data) : slot.reject(data);
  }

  function rpc(method, params = {}, { timeoutMs = 15000 } = {}){
    const id = nextRpcId++;
    return new Promise((resolve, reject)=>{
      const tmr = setTimeout(()=>{
        pending.delete(id);
        reject({ error: 'timeout', method, params });
      }, timeoutMs);
      pending.set(id, { resolve, reject, tmr });
      trySend({ type: 'rpc', id, method, params });
    });
  }

  // =========================
  // Publieke API (compat)
  // =========================

  // Init: open WS. Je kunt een custom WS-pad meegeven.
  function init({ url } = {}){
    connect(url);
    return new Promise((res)=> {
      if (ws && ws.readyState === WebSocket.OPEN) return res();
      const onOpen = ()=>{ off('open', onOpen); res(); };
      on('open', onOpen);
    });
  }

  // ---- Streams (server broadcast) ----
  // Voorheen werd /now periodiek via HTTP gepolled. Nu stream:
  function subscribeNow(handler){ on('now', handler); return ()=>off('now', handler); }
  function subscribeRaw(handler){ on('raw_telegram', handler); return ()=>off('raw_telegram', handler); }
  function subscribeLog(handler){ on('log', handler); return ()=>off('log', handler); }

  // ---- Vervangers voor oude HTTP endpoints (RPC) ----
  // Namen hieronder zijn generiek; pas evt. server side mapping aan.
  function getNow(){ return rpc('get_now'); }
  function getRng(kind, { from=null, to=null } = {}){ return rpc('get_rng', { kind, from, to }); } // kind: 'hours'|'days'|'months'
  function getConfig(){ return rpc('get_config'); }
  function setConfig(key, value){ return rpc('set_config', { key, value }); }
  function setConfigs(obj){ return rpc('set_configs', { entries: obj }); } // bulk
  function remoteUpdate(version){ return rpc('remote_update', { version }); }
  function reboot(){ return rpc('reboot'); }
  function getStatus(){ return rpc('get_status'); }
  function doAction(name, params={}){ return rpc('action', { name, ...params }); }

  // ---- Directe WS-zending (bijv. subscribe/command) ----
  function wsSend(type, data){ trySend({ type, data }); }

  const wsApi = {
    connect, disconnect, on, off,
    send: wsSend,
    get isOpen(){ return !!ws && ws.readyState === WebSocket.OPEN; },
    get url(){ return wsUrl; }
  };

  // =========================
  // Export
  // =========================
  return {
    // lifecycle
    init,
    // websocket
    ws: wsApi,
    // streams
    subscribeNow,
    subscribeRaw,
    subscribeLog,
    // rpc
    rpc,
    // compat "HTTP" -> RPC
    getNow,
    getRng,
    getConfig,
    setConfig,
    setConfigs,
    remoteUpdate,
    reboot,
    getStatus,
    doAction,
  };
})();

/* ======= KORTE HOWTO =======
1) Start in de app:
   import { DAL } from './dal.js';
   await DAL.init();                 // opent WS (ws(s)://host/ws)

2) Streams:
   const unsub = DAL.subscribeNow(updateUi);
   // later: unsub();

3) Calls die eerst HTTP waren:
   const now = await DAL.getNow();
   const cfg = await DAL.getConfig();
   await DAL.setConfig('price_tier', 'low');
   const rng = await DAL.getRng('hours', {from: 1726200000, to: 1726286400});

4) Custom command:
   DAL.ws.send('subscribe', { topic: 'now' });

5) Server RPC-contract:
   Client -> {type:'rpc', id, method:'get_now', params:{}}
   Server -> {type:'rpc_result', id, data:{ ... }}  // of {type:'rpc_error', id, data:{message:'...'}}
   Broadcasts blijven {type:'now'|'raw_telegram'|'log', data:{...}}
================================ */
