// Browser code is kept in a raw literal so the compiled page is the test fixture.
R"CLIENT(
;(() => {
  'use strict';
  const byId = id => document.getElementById(id);
  const login = byId('login'), management = byId('management'), form = byId('setup');
  const msg = byId('message'), keyInput = byId('install-key'), passwordInput = byId('password');
  const loginButton = byId('login-button'), save = byId('save'), refresh = byId('refresh');
  const otaPanel = byId('ota-panel'), otaFile = byId('ota-file'), otaStart = byId('ota-start'), otaMsg = byId('ota-message');
  let ha1 = '', cnonce = '', count = 0, state = null, busy = false;
  let selected = null, session = 0;
  const supported = window.crypto && typeof window.crypto.getRandomValues === 'function' &&
    typeof sha256 === 'function' && typeof fetch === 'function' &&
    typeof AbortController === 'function' && typeof TextEncoder === 'function';

  function forget() {
    ++session; selected = null; otaFile.value = ''; otaPanel.hidden = true;
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
    otaFile.disabled = locked;
    otaStart.disabled = locked || !selected || !otaReady(state);
    byId('github-check').disabled = locked || !otaReady(state);
  }
  function otaReady(value) {
    return value && value.mode === 'STA' && value.otaProtocol === 2 && value.otaSupported === true &&
      value.otaHealthy === true && ['VALID', 'USB_BASELINE', 'ROLLED_BACK_OR_INTERRUPTED'].includes(value.otaBootState) &&
      ['IDLE', 'FAILED'].includes(value.otaPhase);
  }
  async function http(path, options = {}) {
    const {fileUpload, ...requestOptions} = options;
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), fileUpload ? 125000 : 7000);
    try {
      const response = await fetch(path, {...requestOptions, cache: 'no-store', credentials: 'omit',
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
      (state.canConfigure ? '설정 AP 시간 제한 없음. 2.4 GHz Wi-Fi 정보를 입력하세요.' : 'Wi-Fi 연결 후 상태 확인과 서명된 펌웨어 업데이트가 가능합니다.') +
      (state.mdnsUrl ? '\n같은 내부망 접속 주소: ' + state.mdnsUrl + (state.mdnsActive ? ' (활성)' : ' (Wi-Fi 연결 후 사용)') : '');
    otaPanel.hidden = state.mode !== 'STA' || state.otaProtocol !== 2;
    if (!otaPanel.hidden) otaMsg.textContent = '현재: ' + state.buildId + ' · ' + state.otaBootState + '\n' +
      '업데이트 상태: ' + state.otaPhase + ' / ' + state.otaReason + ' · 방식: ' + state.otaOrigin;
    byId('github-message').textContent = '자동 확인: ' + (state.githubAutomatic ? '켜짐 (약 5분마다)' : '꺼짐 (검증 후 별도 활성화)') +
      ' · ' + (state.githubState || '') + ' · 최근 결과: ' + (state.githubResult || '확인 전');
  }
  byId('github-check').addEventListener('click', async () => {
    if (busy || !otaReady(state)) return;
    lock(true);
    try {
      const result = await signed('/api/v1/ota/github/check', {method: 'POST',
        headers: {'X-CSRF-Token': state.csrfToken}, body: ''});
      if (result.checkScheduled !== true) throw Error('조회 접수 확인이 필요합니다.');
      byId('github-message').textContent = 'GitHub 확인을 시작했습니다. 새 서명 버전이 있으면 설치합니다. 잠시 후 상태를 새로고침하세요.';
    } catch (error) { byId('github-message').textContent = error.message + ' 자동 재요청하지 않습니다.'; }
    finally { lock(false); }
  });
  async function inspectFile(file) {
    if (!file || !/\.dmota$/i.test(file.name) || file.size < 224 || file.size > 1310720 + 192)
      throw Error('192바이트 서명 헤더를 포함한 올바른 .dmota 파일이 필요합니다.');
    const header = await file.slice(0, 192).arrayBuffer(), bytes = new Uint8Array(header), view = new DataView(header);
    const raw = (start, size) => Array.from(bytes.slice(start, start + size), value => String.fromCharCode(value)).join('');
    const field = (start, size) => {
      const value = raw(start, size), end = value.indexOf('\0');
      if (end < 1 || !/^[A-Za-z0-9_.-]+$/.test(value.slice(0, end)) || !/^\0+$/.test(value.slice(end)))
        throw Error('파일 헤더의 문자열 형식이 잘못되었습니다.');
      return value.slice(0, end);
    };
    if (bytes.length !== 192 || raw(0, 8) !== 'DMOTA2\r\n') throw Error('DMOTA2 파일이 아닙니다.');
    const version = view.getUint32(8, true), size = view.getUint32(12, true), schema = view.getUint32(16, true);
    const board = field(20, 16), build = field(36, 48);
    if (!version || !schema || board !== 'm5stack-atom' || field(120, 8) !== 'stable' ||
        view.getUint32(116, true) !== 2 || size + 192 !== file.size)
      throw Error('대상 보드·채널·프로토콜·크기를 확인하세요.');
    return {file, header, version, size, schema, board, build};
  }
  otaFile.addEventListener('change', async () => {
    if (busy) return;
    selected = null; lock(true);
    const activeSession = session;
    try {
      const candidate = await inspectFile(otaFile.files[0]);
      if (activeSession !== session) return;
      if (!state || candidate.version <= state.otaVersion || candidate.schema > state.configSchema ||
          candidate.version <= (state.otaAttemptVersion || 0)) throw Error('현재/실패한 버전보다 높은 호환 버전이 필요합니다.');
      selected = candidate;
      otaMsg.textContent = candidate.build + ' · 버전 ' + candidate.version + ' · ' + candidate.size + ' bytes\n' +
        '파일 선택만으로 설치하지 않습니다. 시작 버튼을 누르면 ATOM이 서명을 검증합니다.';
    } catch (error) { otaMsg.textContent = error.message; }
    finally { lock(false); }
  });
  async function observeUpdate(candidate, token, activeSession) {
    for (let attempt = 0; attempt < 30 && session === activeSession; ++attempt) {
      await new Promise(resolve => setTimeout(resolve, 1500));
      try {
        const current = await signed('/api/v1/ota/status');
        if (session !== activeSession) return;
        state = current;
        if (current.otaTransaction === token && current.buildId === candidate.build &&
            current.otaVersion === candidate.version && current.otaOrigin === 'web-file') {
          if (current.otaBootState === 'VALID') { otaMsg.textContent = '정상 확정 완료: ' + current.buildId; return; }
          otaMsg.textContent = '재부팅 완료 · 기기가 연속 5초 정상 동작을 확인하는 중…';
        } else if ((current.otaTransaction === token && current.otaBootState === 'ROLLED_BACK_OR_INTERRUPTED') ||
                   current.otaPhase === 'FAILED') {
          otaMsg.textContent = '업데이트 실패·복구 상태: ' + current.otaBootState + ' / ' + current.otaReason +
            '\n동일 버전을 자동 재전송하지 않습니다.'; return;
        }
      } catch (_) { /* Read-only reconciliation; never repeat prepare/upload. */ }
    }
    if (session === activeSession) otaMsg.textContent = '정상 확정을 아직 확인하지 못했습니다. 상태를 새로고침하세요. 자동 재전송하지 않습니다.';
  }
  otaStart.addEventListener('click', async () => {
    if (busy || !selected || !otaReady(state)) return;
    const candidate = selected, activeSession = session;
    selected = null; lock(true);
    let token = '';
    try {
      await status();
      if (session !== activeSession || !otaReady(state) || candidate.version <= state.otaVersion ||
          candidate.version <= (state.otaAttemptVersion || 0)) throw Error('기기가 업데이트 가능한 상태가 아닙니다.');
      otaMsg.textContent = '서명·보드·버전 확인 중…';
      const prepared = await signed('/api/v1/ota/file/prepare', {method: 'POST',
        headers: {'Content-Type': 'application/octet-stream', 'X-CSRF-Token': state.csrfToken}, body: candidate.header});
      if (!/^[0-9a-f]{32}$/.test(prepared.uploadToken)) throw Error('업로드 토큰이 잘못되었습니다.');
      token = prepared.uploadToken;
      if (session !== activeSession) return;
      // Safari fetch has no reliable upload progress event. Do not invent a
      // percentage, use XHR auto-redirects, or call 100% a successful update.
      otaMsg.textContent = '펌웨어 전송·기기 이미지 검증 중…';
      const result = await signed('/api/v1/ota/upload', {method: 'POST', fileUpload: true,
        headers: {'Content-Type': 'application/octet-stream', 'X-CSRF-Token': state.csrfToken, 'X-OTA-Token': token},
        body: candidate.file.slice(192)});
      if (result.rebootScheduled !== true || result.localHealthRequired !== true) throw Error('재부팅 응답 확인 필요');
      otaMsg.textContent = '검증 완료 · 재부팅 중…';
    } catch (error) {
      otaMsg.textContent = error.message + '\n자동 재전송하지 않고 실제 기기 상태를 확인합니다.';
    } finally {
      if (token && session === activeSession) await observeUpdate(candidate, token, activeSession);
      if (session === activeSession) { otaFile.value = ''; lock(false); }
    }
  });
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
