#pragma once

namespace bootstrap {
constexpr char kPage[] = R"HTML(<!doctype html>
<html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ATOM Lite Wi-Fi 설정</title>
<style>
:root{font:17px system-ui;color:#172b35;background:#eff4f5}body{max-width:36rem;margin:5vh auto;padding:1.4rem}
main{background:white;border:1px solid #cbd8db;border-radius:16px;padding:1.5rem}h1{font-size:1.65rem;margin-top:0}
label{display:block;margin-top:1.2rem}input,button{font:inherit;box-sizing:border-box;width:100%;padding:.8rem;border-radius:7px}
input{margin-top:.35rem;border:1px solid #607680}button{margin-top:1.5rem;border:0;background:#14536a;color:white;cursor:pointer}
:focus-visible{outline:3px solid #b56300;outline-offset:3px}button:disabled{background:#627882;cursor:default}
small{display:block;line-height:1.6;color:#415862}#message{white-space:pre-wrap;line-height:1.6}code{overflow-wrap:anywhere}
[hidden]{display:none!important}.secondary{background:#e6eef0;color:#172b35}input[readonly]{background:#eff4f5}
</style><main><small>USB / OTA 관리 · RS485 비활성</small><h1>ATOM Lite Wi-Fi 설정</h1>
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
<button id="logout" type="button" class="secondary">로그아웃</button></section>
<noscript><p>로그인하려면 Safari 설정에서 JavaScript를 켜세요.</p></noscript>
<small>설정 AP는 시간 제한 없이 유지되며, 저장 후 재부팅하여 Wi-Fi 연결로 전환합니다.
설치 키와 Wi-Fi 비밀번호를 로그·채팅에 공유하지 마세요. 보호된 AP·내부망·VPN에서만 접속하세요.</small></main>
<script>)HTML"
#include "web_sha256.inc"
#include "web_client.inc"
R"HTML(</script></html>)HTML";
}
