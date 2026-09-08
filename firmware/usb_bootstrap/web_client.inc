// Browser code is kept in a raw literal so the compiled page is the test fixture.
R"CLIENT(
;(() => {
  'use strict';
  const byId = id => document.getElementById(id);
  const login = byId('login'), management = byId('management'), form = byId('setup');
  const msg = byId('message'), keyInput = byId('install-key'), passwordInput = byId('password');
  const loginButton = byId('login-button'), save = byId('save'), refresh = byId('refresh');
  let ha1 = '', cnonce = '', count = 0, state = null, busy = false;
  const supported = window.crypto && typeof window.crypto.getRandomValues === 'function' &&
    typeof sha256 === 'function' && typeof fetch === 'function' &&
    typeof AbortController === 'function' && typeof TextEncoder === 'function';

  function forget() {
    ha1 = cnonce = ''; count = 0; state = null;
    keyInput.value = passwordInput.value = '';
    login.hidden = false; management.hidden = form.hidden = true;
    save.disabled = true;
  }
  function lock(locked) {
    busy = locked;
    loginButton.disabled = !supported || locked;
    refresh.disabled = byId('logout').disabled = locked;
    save.disabled = locked || !state || !state.canConfigure;
  }
  async function http(path, options = {}) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 7000);
    try {
      const response = await fetch(path, {...options, cache: 'no-store', credentials: 'omit',
        redirect: 'error', signal: controller.signal});
      // Consume the bounded JSON inside the deadline too.
      const data = await response.json();
      if (!response.ok) {
        const error = Error(response.status === 401 ? '설치 키가 맞지 않거나 인증 정보가 만료되었습니다. 다시 로그인하세요.' :
          response.status === 429 ? '요청이 많습니다. 10초 후 다시 시도하세요.' : '요청 실패: HTTP ' + response.status);
        error.status = response.status;
        throw error;
      }
      return data;
    } finally { clearTimeout(timer); }
  }
  async function signed(path, options = {}) {
    if (!ha1) throw Error('먼저 로그인하세요.');
    // Refresh the public challenge, not the write itself. No automatic PUT retry.
    const challenge = await http('/api/v1/auth');
    if (challenge.realm !== 'DM-BRIDGE-USB' || challenge.algorithm !== 'SHA-256' ||
        challenge.qop !== 'auth' || !/^[0-9a-f]{32}$/.test(challenge.nonce)) {
      throw Error('지원하지 않는 인증 응답입니다.');
    }
    if (!ha1 || ++count > 0xffffffff) throw Error('다시 로그인하세요.');
    const method = options.method || 'GET', nc = ('00000000' + count.toString(16)).slice(-8);
    const response = sha256(ha1 + ':' + challenge.nonce + ':' + nc + ':' + cnonce + ':auth:' + sha256(method + ':' + path));
    const authorization = 'Digest username="installer", realm="DM-BRIDGE-USB", nonce="' + challenge.nonce +
      '", uri="' + path + '", response="' + response + '", algorithm=SHA-256, qop=auth, nc=' + nc + ', cnonce="' + cnonce + '"';
    return http(path, {...options, headers: {...options.headers, Authorization: authorization}});
  }
  async function status() {
    state = await signed('/api/v1/status');
    login.hidden = true; management.hidden = false; form.hidden = !state.canConfigure;
    msg.textContent = '모드: ' + state.mode + ' · IP: ' + state.ip + ' · 버전: ' + state.buildId + '\n' +
      (state.canConfigure ? '설정 AP 시간 제한 없음. 2.4 GHz Wi-Fi 정보를 입력하세요.' : '이 단계에서는 상태 조회만 가능합니다.') +
      (state.mdnsUrl ? '\n같은 내부망 접속 주소: ' + state.mdnsUrl + (state.mdnsActive ? ' (활성)' : ' (Wi-Fi 연결 후 사용)') : '');
  }
  login.addEventListener('submit', async event => {
    event.preventDefault();
    if (busy || !supported) return;
    lock(true);
    try {
      const bytes = new Uint8Array(16);
      window.crypto.getRandomValues(bytes);
      cnonce = Array.from(bytes, byte => ('0' + byte.toString(16)).slice(-2)).join('');
      ha1 = sha256('installer:DM-BRIDGE-USB:' + keyInput.value);
      keyInput.value = ''; count = 0;
      msg.textContent = '로그인 확인 중…';
      await status();
    } catch (error) { forget(); msg.textContent = error.message; }
    finally { lock(false); }
  });
  refresh.addEventListener('click', async () => {
    if (busy) return;
    lock(true);
    try { await status(); }
    catch (error) { forget(); msg.textContent = error.message; }
    finally { lock(false); }
  });
  form.addEventListener('submit', async event => {
    event.preventDefault();
    if (busy || !state || !state.canConfigure) return;
    const ssid = byId('ssid').value;
    if (!ssid || new TextEncoder().encode(ssid).length > 32) {
      msg.textContent = 'SSID는 UTF-8 기준 1–32바이트여야 합니다.'; return;
    }
    lock(true);
    try {
      const data = await signed('/api/v1/config', {method: 'PUT',
        headers: {'Content-Type': 'application/json', 'X-CSRF-Token': state.csrfToken},
        body: JSON.stringify({ssid, password: passwordInput.value, configRevision: state.configRevision})});
      if (data.rebootScheduled !== true) throw Error('재부팅 응답을 확인하지 못했습니다.');
      const mdnsUrl = state.mdnsUrl;
      forget(); login.hidden = true;
      msg.textContent = '설정 저장 완료. ATOM이 재부팅하여 Wi-Fi에 연결합니다.\n' +
        'iPad를 기존 Wi-Fi로 되돌린 뒤 ' + (mdnsUrl || '장비 IP 주소') + ' 로 접속하세요.\n' +
        'VPN에서 이름이 안 열리면 USB의 STA_IP 또는 공유기의 DHCP 목록에서 확인한 IP로 접속하세요.\n' +
        'ATOM이 저장한 Wi-Fi에 최초 60초 안에 연결하지 못하면 같은 설정 AP로 돌아옵니다. ' +
        '사용자가 웹페이지에 접속해야 하는 제한 시간이 아닙니다. 설치 키는 유지됩니다.';
    } catch (error) {
      // A transport failure might follow a durable commit. Require observation,
      // never resend a mutation or leave a one-click duplicate-save button.
      forget();
      msg.textContent = error.message + '\n응답이 끊겼다면 저장 여부를 단정하지 말고 USB 상태를 확인하세요. 자동 재전송하지 않습니다.';
    } finally { lock(false); }
  });
  byId('logout').addEventListener('click', () => {
    if (busy) return;
    forget(); msg.textContent = '로그아웃했습니다. 설치 키로 다시 로그인할 수 있습니다.';
  });
  window.addEventListener('pagehide', forget);
  window.addEventListener('pageshow', event => {
    if (event.persisted) { forget(); lock(false); msg.textContent = '설치 키로 다시 로그인하세요.'; }
  });
  if (!supported) {
    msg.textContent = '이 브라우저는 필요한 보안 기능을 지원하지 않습니다. 최신 Safari로 접속하세요.';
  } else { lock(false); }
})();
)CLIENT"
