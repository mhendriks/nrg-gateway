/* cdn/dal.js — WebSocket-first DAL zonder ES modules (maakt window.DAL)
   Features:
   - WS met backoff + heartbeat
   - Pub/Sub: DAL.ws.on/off + broadcasts per type
   - RPC: DAL.rpc(method, params) => Promise
   - Compat-calls: getNow/getRng/getConfig/setConfig/setConfigs/remoteUpdate/reboot/getStatus/doAction
*/

(function(global){
  'use strict';

  // ===== Event-bus =====
  const listeners = new Map(); // type -> Set<fn>
  function on(type, fn){ if(!listeners.has(type)) listeners.set(type, new Set()); listeners.get(type).add(fn); }
  function off(type, fn){ const set = listeners.get(type); if(set){ set.delete(fn); if(!set.size) listeners.delete(type); } }
  function emit(type, payload){ const set = listeners.get(type); if(set){ set.forEach(fn=>{ try{ fn(payload); }catch(e){ console.error('[DAL] handler error', e); } }); } }

  // ===== WebSocket state =====
  let ws = null;
  let wantOpen = false;
  let reconnectAttempts = 0;
  let reconnectTimer = null;
  let connectLock = false;
  let pingTimer = null, watchdogTimer = null;
  let lastPong = 0;
  let wsUrl = null;
  const sendQueue = [];

  function makeUrl(custom){
    if (custom) return custom;
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${proto}//${location.host}/ws`;
  }

  function isVisibleOnline(){
    return document.visibilityState === 'visible' && navigator.onLine;
  }

  function startHeartbeat(){
    stopHeartbeat();
    lastPong = Date.now();
    // ping elke 20s; dongle antwoordt met "pong"
    // NB: stuur top-level ts_client zodat de dongle 'm kan echo’en
    pingTimer = setInterval(()=> {
      trySend({ type:'ping', ts_client: Date.now() });  // <— aangepast
    }, 20000);
    // watchdog: als >40s geen pong -> herconnect
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
    if (!isVisibleOnline()) return;      // <— alleen als tab zichtbaar & online
    if (reconnectTimer) return;          // <— al ingepland
    const base = 500, max = 15000, jitter = Math.random()*200;
    const backoff = Math.min(max, base * (2 ** reconnectAttempts)) + jitter;
    reconnectTimer = setTimeout(()=>{
      reconnectTimer = null;
      connect();                         // probeert singleton-connect
      reconnectAttempts = Math.min(reconnectAttempts + 1, 10);
    }, backoff);
  }

  function flushQueue(){
    while (ws && ws.readyState === WebSocket.OPEN && sendQueue.length){
      ws.send(sendQueue.shift());
    }
  }

  function safeClose(reconnect=false){
    stopHeartbeat();
    if (ws){
      try { ws.close(); } catch(_){}
      ws = null;
    }
    connectLock = false;                 // <— vrijgeven
    if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer=null; }
    if (reconnect) scheduleReconnect();
  }

  function handleMessage(ev){
    let msg;
    try { msg = JSON.parse(ev.data); }
    catch { return console.warn('[DAL] non-JSON WS payload'); }

    const { type, data, id } = msg || {};
	
	lastPong = Date.now(); //aways reset pong when event is received
    
    if (type === 'pong'){ 
    console.log("pong received");
//     lastPong = Date.now(); 
    return; 
    }

    // RPC result / error
    if (type === 'rpc_result' || type === 'rpc_error'){
      resolveRpc(id, type === 'rpc_result', data);
      return;
    }

    // Broadcast events (bijv. 'now', 'raw_telegram', 'log', ...)
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

    // --- singleton & lifecycle guards ---
    if (connectLock) return;                                                // <—
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return; // <—
    if (!isVisibleOnline()) return;                                         // <—

    connectLock = true;                                                     // <—
    try {
      const socket = new WebSocket(wsUrl);

      socket.addEventListener('open', ()=>{
        ws = socket;
        connectLock = false;
        reconnectAttempts = 0;
        startHeartbeat();
        flushQueue();
        emit('open');

//         // (re)subscribe basis / zware topics alleen als zichtbaar
//         trySend({ cmd: 'subscribe', topic: 'now' });
//         if (document.visibilityState === 'visible'){
//           trySend({ cmd: 'subscribe', topic: 'insights' });
//           trySend({ cmd: 'subscribe', topic: 'raw_telegram' });
//         }
      });

      socket.addEventListener('message', handleMessage);

      socket.addEventListener('close', ()=>{
        if (ws === socket) ws = null;
        connectLock = false;
        emit('close');
        stopHeartbeat();
        if (wantOpen){
          scheduleReconnect();
        }
      });

      socket.addEventListener('error', (e)=>{
        emit('error', e);
        // forceer snelle sluiting → 'close' plant reconnect
        try { socket.close(); } catch(_) {}
      });

    } catch(e){
      connectLock = false;
      emit('error', e);
      scheduleReconnect();
    }
  }

  function disconnect(){
    wantOpen = false;
    safeClose(false);
  }

  // ===== RPC laag =====
  let nextRpcId = 1;
  const pending = new Map(); // id -> {resolve, reject, tmr}

  function resolveRpc(id, ok, data){
    const slot = pending.get(id);
    if (!slot) return;
    clearTimeout(slot.tmr);
    pending.delete(id);
    ok ? slot.resolve(data) : slot.reject(data);
  }

  function rpc(method, params = {}, opts){
    const id = nextRpcId++;
    const timeoutMs = (opts && opts.timeoutMs) || 15000;
    return new Promise((resolve, reject)=>{
      const tmr = setTimeout(()=>{
        pending.delete(id);
        reject({ error: 'timeout', method, params });
      }, timeoutMs);
      pending.set(id, { resolve, reject, tmr });
      trySend({ type: 'rpc', id, method, params });
    });
  }

  // ===== Publieke API (compat) =====
  function init(options){
    options = options || {};
    connect(options.url);
    // return een mini-waiter die resolve't bij open
    return new Promise((res)=>{
      if (ws && ws.readyState === WebSocket.OPEN) return res();
      const onOpen = ()=>{ off('open', onOpen); res(); };
      on('open', onOpen);
    });
  }

  // Streams (broadcasts)
  function subscribeNow(handler){ on('now', handler); return ()=>off('now', handler); }
  function subscribeRaw(handler){ on('raw_telegram', handler); return ()=>off('raw_telegram', handler); }
  function subscribeLog(handler){ on('log', handler); return ()=>off('log', handler); }

  // “HTTP” → RPC vervangers
  function getNow(){ return rpc('get_now'); }
  function getRng(kind, range){ range = range || {}; return rpc('get_rng', { kind, from: range.from ?? null, to: range.to ?? null }); }
  function getConfig(){ return rpc('get_config'); }
  function setConfig(key, value){ return rpc('set_config', { key, value }); }
  function setConfigs(obj){ return rpc('set_configs', { entries: obj }); }
  function remoteUpdate(version){ return rpc('remote_update', { version }); }
  function reboot(){ return rpc('reboot'); }
  function getStatus(){ return rpc('get_status'); }
  function doAction(name, params){ return rpc('action', Object.assign({ name }, params||{})); }

  // Directe WS send (niet-RPC)
  function wsSend(type, data){ trySend({ cmd: type, topic: data }); }

  const DAL = {
    // lifecycle
    init,
    // ws
    ws: {
      connect, disconnect, on, off, send: wsSend,
      get isOpen(){ return !!ws && ws.readyState === WebSocket.OPEN; },
      get url(){ return wsUrl; }
    },
    // streams
    subscribeNow, subscribeRaw, subscribeLog,
    // rpc
    rpc,
    // compat
    getNow, getRng, getConfig, setConfig, setConfigs, remoteUpdate, reboot, getStatus, doAction,
  };

  // Zet in global scope
  global.DAL = DAL;

  // Auto-connect (uit te zetten via window.DAL_AUTO_CONNECT=false vóór deze script)
  if (global.DAL_AUTO_CONNECT !== false){
    try { connect(); } catch(e){ console.warn('[DAL] auto-connect failed', e); }
  }

  // ==== NIEUW: lifecycle hooks ====
  // Sluit WS zodra tab verborgen is; herverbind bij zichtbaar + online
  document.addEventListener('visibilitychange', ()=>{
    if (document.visibilityState === 'visible'){
    console.log("visible");
      wantOpen = true;
      reconnectAttempts = 0;
      scheduleReconnect();       // of direct connect(); maar schedule is rustiger
    } else {
    console.log("INvisible");
      disconnect();              // sluit direct → voorkomt spook-sessies
    }
  });

  window.addEventListener('online',  ()=>{ if (wantOpen) { reconnectAttempts=0; scheduleReconnect(); } });
  window.addEventListener('offline', ()=>{ safeClose(false); });

  window.addEventListener('beforeunload', ()=>{ disconnect(); });

})(window);
