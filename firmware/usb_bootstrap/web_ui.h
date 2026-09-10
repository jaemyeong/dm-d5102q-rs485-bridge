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
.view-nav,.metrics{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:.7rem}.view-nav button{margin-top:.7rem}
.view-nav [aria-pressed="true"]{background:#14536a;color:white}.metric{background:#eff4f5;border-radius:8px;padding:.8rem;overflow-wrap:anywhere}
.metric strong{display:block;font-size:1.15rem;margin-top:.3rem}progress{width:100%;height:1rem}#system-details{white-space:pre-line;line-height:1.7;overflow-wrap:anywhere}
#heap-chart{width:100%;height:120px;background:#eff4f5;border-radius:8px}#system-message{line-height:1.5}
@media(max-width:380px){.metrics{grid-template-columns:1fr}main{padding:1rem}}
</style><main><small>USB / OTA 관리 · RS485 비활성</small><h1>ATOM Lite 관리</h1>
<p id="message" role="status" aria-live="polite">설치 키로 로그인하세요. 브라우저 인증 팝업은 사용하지 않습니다.</p>
<form id="login" action="/" method="post" autocomplete="off">
<label for="username">사용자</label><input id="username" value="installer" readonly autocomplete="off">
<label for="install-key">설치 키</label><input id="install-key" type="password" required minlength="20" maxlength="20" autocomplete="off" autocapitalize="none" spellcheck="false">
<small>ATOM 설정 AP에 접속할 때 사용한 20자리 비밀번호입니다. 연결할 공유기의 Wi-Fi 비밀번호와 다릅니다.</small>
<button id="login-button" disabled>로그인</button></form>
<section id="management" hidden>
<nav class="view-nav" aria-label="관리 화면"><button id="control-view" type="button" class="secondary" aria-pressed="true">설정 · OTA</button><button id="system-view" type="button" class="secondary" aria-pressed="false">시스템</button></nav>
<button id="refresh" type="button" class="secondary">상태 새로고침</button>
<section id="control-panel">
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
<button id="ota-start" type="button" disabled>업데이트 시작</button></section></section>
<section id="system-panel" hidden aria-labelledby="system-heading"><h2 id="system-heading">시스템 · 하드웨어</h2>
<p id="system-message" role="status" aria-live="polite">시스템 화면에서만 5초 간격으로 조회합니다.</p>
<div class="metrics">
<div class="metric">현재 여유 힙<strong id="system-heap">—</strong></div>
<div class="metric">부팅 이후 최저 여유 힙<strong id="system-min-heap">—</strong></div>
<div class="metric">최대 연속 할당 가능<strong id="system-block">—</strong></div>
<div class="metric">펌웨어 추가 가능 공간<strong id="system-headroom">—</strong></div></div>
<p id="system-image">펌웨어 공간: —</p><progress id="system-image-bar" max="1" value="0" aria-label="현재 업로드 제한 대비 펌웨어 사용량"></progress>
<h3>메모리 추이</h3><small>파랑: 여유 힙 · 주황: 최대 연속 블록. 최근 최대 60개 샘플, 브라우저에만 보관됩니다. 가로축은 수집 순서입니다.</small>
<svg id="heap-chart" viewBox="0 0 300 100" preserveAspectRatio="none" role="img" aria-label="메모리 추이"><polyline id="heap-line" fill="none" stroke="#14536a" stroke-width="2" points=""/><polyline id="block-line" fill="none" stroke="#b56300" stroke-width="2" points=""/></svg>
<small id="system-chart-scale">아직 샘플 없음</small><p id="system-details"></p>
<small>힙은 실행 중 할당 가능한 메모리이며 전체 SRAM 용량과 다릅니다. NVS는 엔트리 단위이며 파일 저장 공간이 아닙니다.
이 파티션 구성에는 파일시스템이 없습니다. 전압·전류 계측은 제공하지 않습니다. OTA 작업 중에는 시스템 조회를 쉽니다.</small></section>
<button id="logout" type="button" class="secondary">로그아웃</button></section>
<noscript><p>로그인하려면 Safari 설정에서 JavaScript를 켜세요.</p></noscript>
<small>설정 AP는 시간 제한 없이 유지되며, 저장 후 재부팅하여 Wi-Fi 연결로 전환합니다.
설치 키와 Wi-Fi 비밀번호를 로그·채팅에 공유하지 마세요. 보호된 AP·내부망·VPN에서만 접속하세요.</small></main>
<script>)HTML"
#include "web_sha256.inc"
#include "web_client.inc"
R"HTML(</script></html>)HTML";
}
