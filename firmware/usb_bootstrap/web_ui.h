#pragma once

namespace bootstrap {
constexpr char kPage[] = R"HTML(<!doctype html>
<html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ATOM Lite 관리</title>
<style>
:root{font:17px system-ui;color:#172b35;background:#eff4f5}body{max-width:36rem;margin:5vh auto;padding:1.4rem}
main{background:white;border:1px solid #cbd8db;border-radius:16px;padding:1.5rem}h1{font-size:1.65rem;margin-top:0}
label{display:block;margin-top:1.2rem}input,button{font:inherit;box-sizing:border-box;width:100%;padding:.8rem;border-radius:7px}
input{margin-top:.35rem;border:1px solid #607680}button{margin-top:1.5rem;border:0;background:#14536a;color:white;cursor:pointer}
:focus-visible{outline:3px solid #b56300;outline-offset:3px}button:disabled{background:#627882;cursor:default}
small{display:block;line-height:1.6;color:#415862}#message,#ota-message{white-space:pre-wrap;line-height:1.6;overflow-wrap:anywhere}code{overflow-wrap:anywhere}
[hidden]{display:none!important}.secondary{background:#e6eef0;color:#172b35}input[readonly]{background:#eff4f5}
</style><main><small>USB / OTA 관리 · RS485 비활성</small><h1>ATOM Lite 관리</h1>
<p id="message" role="status" aria-live="polite">설치 키로 로그인하세요. 브라우저 인증 팝업은 사용하지 않습니다.</p>
<form id="login" action="/" method="post" autocomplete="off">
<label for="username">사용자</label><input id="username" value="installer" readonly autocomplete="off">
<label for="install-key">설치 키</label><input id="install-key" type="password" required minlength="20" maxlength="20" autocomplete="off" autocapitalize="none" spellcheck="false">
<small>ATOM 설정 AP에 접속할 때 사용한 20자리 비밀번호입니다. 연결할 공유기의 Wi-Fi 비밀번호와 다릅니다.</small>
<button id="login-button" disabled>로그인</button></form>
<section id="management" hidden>
<button id="refresh" type="button" class="secondary">상태 새로고침</button>
<form id="setup" action="/" method="post" hidden><label for="ssid">Wi-Fi 이름 (SSID)</label><input id="ssid" autocomplete="off" required maxlength="32">
<small>UTF-8 기준 최대 32바이트. 네트워크 검색 기능은 아직 없습니다.</small>
<label for="password">Wi-Fi 비밀번호</label><input id="password" type="password" autocomplete="new-password" required minlength="8" maxlength="64">
<small>8–63자 또는 64자리 16진수 PSK. 공개 Wi-Fi는 지원하지 않습니다.</small>
<button id="save" disabled>저장 후 재부팅</button></form>
<section id="ota-panel" hidden><h2>펌웨어 업데이트</h2>
<p id="ota-message" role="status" aria-live="polite">서명된 .dmota 파일을 선택하세요.</p>
<p id="github-message" role="status" aria-live="polite">GitHub 확인 상태</p>
<button id="github-automatic" type="button" class="secondary" aria-pressed="false" disabled>자동 업데이트 켜기</button>
<small>설정은 재부팅 후에도 유지됩니다. 켜면 새 정식 서명 버전을 자동 설치할 수 있습니다.
끄기는 이후 자동 조회만 막으며, 이미 시작한 조회·설치는 중단하지 않습니다. 버튼 초기화 시 꺼짐으로 돌아갑니다.</small>
<button id="github-check" type="button" class="secondary">GitHub 확인 후 업데이트</button>
<small>새 정식 Release가 있으면 서명 검증 후 설치합니다. 장비에서 인터넷에 연결할 수 있어야 합니다.</small>
<label for="ota-file">펌웨어 파일 (.dmota)</label><input id="ota-file" type="file" accept=".dmota">
<small>같은 LAN/VPN에서 인터넷 없이 수동 업데이트할 수 있습니다. 전체 플래시 덤프나 .bin 원본은 받지 않습니다.
업데이트 중 전원을 끄지 마세요. 전송 완료 후에도 재부팅과 정상 확인까지 기다리세요.</small>
<button id="ota-start" type="button" disabled>업데이트 시작</button></section>
<button id="logout" type="button" class="secondary">로그아웃</button></section>
<noscript><p>로그인하려면 Safari 설정에서 JavaScript를 켜세요.</p></noscript>
<small>설정 AP는 시간 제한 없이 유지되며, 저장 후 재부팅하여 Wi-Fi 연결로 전환합니다.
설치 키와 Wi-Fi 비밀번호를 로그·채팅에 공유하지 마세요. 보호된 AP·내부망·VPN에서만 접속하세요.</small></main>
<script>)HTML"
#include "web_sha256.inc"
#include "web_client.inc"
R"HTML(</script></html>)HTML";
}
