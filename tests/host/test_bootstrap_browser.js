'use strict';
// Execute the page actually served by main.cpp, with DOM fakes and real HTTP.
// The VM intentionally has no Node process/require/module or crypto.subtle.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const http = require('node:http');
const vm = require('node:vm');
const fs = require('node:fs');
const [port, key] = fs.readFileSync(0, 'utf8').trim().split(' '); // Synthetic fixture only.
const sha = value => crypto.createHash('sha256').update(value).digest('hex');
let checks = 0;
const check = (actual, expected) => { ++checks; assert.deepEqual(actual, expected); };

function wire(path, options = {}) {
  return new Promise((resolve, reject) => {
    const body = options.body || '';
    const request = http.request({host: '127.0.0.1', port, path, method: options.method || 'GET',
      headers: {Host: '192.168.4.1', ...(body ? {'Content-Length': Buffer.byteLength(body),
        Origin: 'http://192.168.4.1'} : {}), ...options.headers}}, response => {
      let text = '';
      response.setEncoding('utf8');
      response.on('data', chunk => { text += chunk; });
      response.on('end', () => resolve({status: response.statusCode, ok: response.statusCode < 300,
        headers: response.headers, text, json: async () => JSON.parse(text)}));
    });
    request.on('error', reject);
    request.end(body);
  });
}

function browser(html, fetcher, random = crypto.webcrypto.getRandomValues.bind(crypto.webcrypto)) {
  const elements = new Map(), events = {};
  for (const match of html.matchAll(/<[^>]+\bid="([^"]+)"[^>]*>/g)) {
    const tag = match[0], id = match[1];
    elements.set(id, {value: '', textContent: '', hidden: /\bhidden\b/.test(tag),
      disabled: /\bdisabled\b/.test(tag), listeners: {},
      addEventListener(type, callback) { this.listeners[type] = callback; }});
  }
  const context = vm.createContext({document: {getElementById: id => {
    assert(elements.has(id), 'Page is missing element ' + id); return elements.get(id);
  }}, crypto: {getRandomValues: random}, TextEncoder, AbortController,
    setTimeout: (fn, ms) => setTimeout(fn, ms === 1500 ? 0 : ms), clearTimeout,
    fetch: fetcher, addEventListener: (type, callback) => { events[type] = callback; }});
  context.window = context;
  const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(match => match[1]);
  check(scripts.length, 1);
  scripts.forEach(script => vm.runInContext(script, context, {timeout: 1000}));
  return {context, element: id => elements.get(id), events,
    async event(id, type) {
      let prevented = false;
      await elements.get(id).listeners[type]({preventDefault: () => { prevented = true; }});
      if (type === 'submit') check(prevented, true);
    },
    async login(value = key) { elements.get('install-key').value = value; await this.event('login', 'submit'); }};
}

async function main() {
  const page = await wire('/');
  check(page.status, 200);
  check(page.headers['www-authenticate'], undefined);
  check(page.headers['cache-control'], 'no-store');
  check(page.text.includes(key), false);
  check(/<(?:script|link)[^>]+(?:src|href)=/i.test(page.text), false);
  // Secrets have no HTML names: even a missing script cannot submit credentials.
  check(/id="(?:install-key|password)"[^>]*\bname=/.test(page.text), false);
  const calls = [];
  const real = async (path, options) => {
    check(options.credentials, 'omit'); check(options.redirect, 'error');
    calls.push({path, options}); return wire(path, options);
  };
  const app = browser(page.text, real);
  check(app.element('login').hidden, false);
  check(app.element('setup').hidden, true);
  check(app.element('login-button').disabled, false);
  check(calls.length, 0); // No anonymous state fetch, native login or auto-login.
  for (const value of ['', 'abc', '한글😀', 'a'.repeat(55), 'a'.repeat(56), 'a'.repeat(64),
    'a'.repeat(65), 'a'.repeat(1000), 'installer:DM-BRIDGE-USB:' + key]) {
    check(app.context.sha256(value), sha(value));
  }
  check((await wire('/api/v1/status')).status, 401);
  await app.login('A'.repeat(20));
  check(app.element('login').hidden, false);
  check(app.element('management').hidden, true);
  check(app.element('install-key').value, '');
  check(app.element('message').textContent.includes('설치 키'), true);
  await app.login();
  check(app.element('login').hidden, true);
  check(app.element('setup').hidden, false);
  check(app.element('message').textContent.includes('시간 제한 없음'), true);
  check(app.element('message').textContent.includes('http://dm-bridge-112233.local'), true);
  check(app.element('message').textContent.includes('Wi-Fi 연결 후 사용'), true);
  check(app.element('install-key').value, '');
  const signed = calls.at(-1);
  check((await wire(signed.path, signed.options)).status, 401); // Counter replay fails.
  // Challenge rotation by a rejected request must not strand an open form.
  await app.event('refresh', 'click');
  check(app.element('setup').hidden, false);
  await app.event('logout', 'click');
  check(app.element('login').hidden, false);
  check(app.element('setup').hidden, true);
  check(app.element('password').value, '');
  const countBefore = calls.length;
  await app.event('setup', 'submit'); // Hidden/unauthenticated submits cannot write.
  check(calls.length, countBefore);
  await app.login();
  app.element('ssid').value = '가'.repeat(11);
  app.element('password').value = 'browser-test-password';
  const countValid = calls.length;
  await app.event('setup', 'submit');
  check(calls.length, countValid);
  app.element('ssid').value = 'browser-test';
  await app.event('setup', 'submit');
  check(app.element('message').textContent.includes('설정 저장 완료'), true);
  check(app.element('message').textContent.includes('http://dm-bridge-112233.local'), true);
  check(app.element('message').textContent.includes('VPN'), true);
  check(app.element('password').value, '');
  check(app.element('setup').hidden, true);
  check(calls.filter(call => call.path === '/api/v1/config').length, 1);
  check(JSON.stringify(calls).includes(key), false);
  check(JSON.stringify(calls).includes(sha('installer:DM-BRIDGE-USB:' + key)), false);

  // Deterministic browser failure paths against an independent Digest verifier.
  let nonce = '1'.repeat(32), puts = 0, failMode = '', nc = 0, prepares = 0, uploads = 0, uploaded = false;
  const reply = (status, data) => ({status, ok: status >= 200 && status < 300, json: async () => data});
  const fake = async (path, options) => {
    if (path === '/api/v1/auth') {
      if (failMode === 'rate') return reply(429, {error: 'AUTH_RATE_LIMIT'});
      return reply(200, {realm: 'DM-BRIDGE-USB', nonce,
        algorithm: failMode === 'downgrade' ? 'MD5' : 'SHA-256', qop: 'auth'});
    }
    const authorization = options.headers.Authorization;
    const fields = Object.fromEntries([...authorization.matchAll(/(\w+)=(?:"([^"]*)"|([^,\s]+))/g)]
      .map(match => [match[1], match[2] || match[3]]));
    check(fields.algorithm, 'SHA-256'); check(fields.qop, 'auth');
    check(fields.nonce, nonce); check(fields.uri, path);
    check(Number.parseInt(fields.nc, 16) > nc, true); nc = Number.parseInt(fields.nc, 16);
    check(fields.response, sha(sha('installer:DM-BRIDGE-USB:' + key) + ':' + nonce + ':' + fields.nc + ':' +
      fields.cnonce + ':auth:' + sha((options.method || 'GET') + ':' + path)));
    if (path === '/api/v1/config') {
      ++puts;
      check(options.headers['X-CSRF-Token'], '2'.repeat(32));
      if (failMode === 'lost') throw Error('Test connection lost after possible commit');
      if (failMode === 'csrf') return reply(403, {error: 'CSRF_REJECTED'});
      return reply(202, {rebootScheduled: true});
    }
    if (path === '/api/v1/ota/file/prepare') {
      ++prepares; check(options.body.byteLength, 192);
      check(options.headers['X-CSRF-Token'], '2'.repeat(32));
      check(options.redirect, 'error');
      return reply(200, {uploadToken: 'b'.repeat(32)});
    }
    if (path === '/api/v1/ota/upload') {
      ++uploads; uploaded = true;
      check(options.headers['X-OTA-Token'], 'b'.repeat(32));
      check(options.redirect, 'error'); check(options.credentials, 'omit');
      check(options.body.size, 64);
      if (failMode === 'ota-lost') throw Error('Upload response lost after possible commit');
      return reply(202, {rebootScheduled: true, localHealthRequired: true});
    }
    if (path === '/api/v1/ota/status' && uploaded) return reply(200, {
      buildId: 'web-test-next', otaVersion: 301, otaTransaction: 'b'.repeat(32),
      otaOrigin: 'web-file', otaBootState: 'VALID', otaPhase: 'IDLE'});
    return reply(200, {mode: failMode === 'station' ? 'STA' : 'AP', ip: '192.168.4.1',
      buildId: 'test', canConfigure: failMode !== 'station', csrfToken: '2'.repeat(32),
      configRevision: 0, apTimeoutEnabled: false, apRemainingMs: null,
      otaVersion: 300, configSchema: 1, otaProtocol: 2, otaSupported: true, otaHealthy: true,
      otaBootState: 'VALID', otaPhase: 'IDLE', otaReason: 'NONE', otaOrigin: 'legacy-push',
      mdnsUrl: 'http://dm-bridge-112233.local', mdnsActive: failMode === 'station'});
  };
  for (const mode of ['lost', 'csrf']) {
    failMode = ''; puts = nc = 0;
    const ui = browser(page.text, fake);
    await ui.login();
    nonce = '3'.repeat(32); // Models idle beyond nonce TTL; new metadata used before PUT.
    failMode = mode;
    ui.element('ssid').value = 'test'; ui.element('password').value = 'test-password';
    await ui.event('setup', 'submit');
    check(puts, 1);
    check(ui.element('message').textContent.includes('설정 저장 완료'), false);
    check(ui.element('message').textContent.includes('자동 재전송하지 않습니다'), true);
    check(ui.element('setup').hidden, true);
    check(ui.element('password').value, '');
    await ui.event('setup', 'submit'); check(puts, 1);
  }
  for (const mode of ['rate', 'downgrade']) {
    failMode = mode; nc = 0;
    const ui = browser(page.text, fake); await ui.login();
    check(ui.element('management').hidden, true);
    check(ui.element('install-key').value, '');
    check(ui.element('login-button').disabled, false);
  }
  failMode = 'station'; nc = 0;
  const station = browser(page.text, fake); await station.login();
  check(station.element('setup').hidden, true);
  check(station.element('management').hidden, false);
  check(station.element('save').disabled, true);
  check(station.element('message').textContent.includes('http://dm-bridge-112233.local (활성)'), true);
  check(station.element('ota-panel').hidden, false);
  const header = Buffer.alloc(192);
  header.write('DMOTA2\r\n'); header.writeUInt32LE(301, 8); header.writeUInt32LE(64, 12);
  header.writeUInt32LE(1, 16); header.write('m5stack-atom', 20); header.write('web-test-next', 36);
  header.writeUInt32LE(2, 116); header.write('stable', 120);
  const file = new Blob([header, Buffer.alloc(64)]); file.name = 'test.dmota';
  for (const mode of ['', 'ota-lost']) {
    failMode = 'station'; nc = prepares = uploads = 0; uploaded = false;
    const ui = browser(page.text, fake); await ui.login();
    ui.element('ota-file').files = [{name: 'raw.bin', size: 256}];
    await ui.event('ota-file', 'change'); check(ui.element('ota-start').disabled, true);
    ui.element('ota-file').files = [file]; await ui.event('ota-file', 'change');
    check(prepares, 0); check(uploads, 0); check(ui.element('ota-start').disabled, false);
    // Retain STA on the preflight response; simulate loss only at upload itself.
    const uploadFake = async (path, options) => {
      if (mode && path === '/api/v1/ota/upload') failMode = mode;
      return fake(path, options);
    };
    ui.context.fetch = uploadFake;
    await ui.event('ota-start', 'click');
    check(prepares, 1); check(uploads, 1);
    check(ui.element('ota-message').textContent.includes('정상 확정 완료'), true);
    await ui.event('ota-start', 'click'); check(uploads, 1);
  }
  station.events.pagehide();
  check(station.element('management').hidden, true);
  station.events.pageshow({persisted: true});
  check(station.element('login').hidden, false);
  const noRandom = browser(page.text, () => { throw Error('Unsupported browser must not fetch'); }, null);
  check(noRandom.element('login-button').disabled, true);
  check(noRandom.element('management').hidden, true);
  console.log('Compiled page JS + real C++ HTTP/DOM fakes: ' + checks + ' assertions passed (not Safari hardware proof)');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
