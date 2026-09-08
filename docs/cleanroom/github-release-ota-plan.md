# GitHub Release 자동 OTA + 웹 수동 업로드 계획

작성: 2026-09-09 KST. 최초 계획은 아래에 보존한다. 이후 사용자의 `승인`에 따라
**G1–G3 소스·호스트 테스트·빌드와 서명 패키지를 구현했다.** 실제 G2 Safari,
G3 장치 TLS/heap, G4–G6 설치·복구·자동 활성화는 아직 미검증이다.
현재 구현의 정확한 계약과 차이는 [DMOTA2 updater](dmota2-updater.md)를 따른다.

대상은 ATOM Lite C008 / ESP32-PICO-D4이다. 공개 저장소는
[jaemyeong/dm-d5102q-rs485-bridge](https://github.com/jaemyeong/dm-d5102q-rs485-bridge).
사용자는 공개 저장소 생성·소스 푸시, Release 기반 자동 업데이트 계획과
웹에서 펌웨어 파일을 직접 올리는 수동 업데이트를 요청했다. 최초 계획 작업은
소스 공개와 계획까지이며 Release 발행, 장치 업데이트, CI 서명 키 등록,
자동 실행 활성화나 현장 배선은 포함하지 않는다.

## 1. 목표와 현재 구현의 차이

- 목표: Release를 발행하면 ATOM이 직접 확인·다운로드·검증·재부팅한다.
  발행 후에는 개발 Mac, Codex 세션이나 현장으로 들어오는 VPN 연결이 필요 없다.
  대신 ATOM의 Wi-Fi에서 GitHub로 나가는 인터넷/HTTPS와 시간 동기화가 필요하다.
- 추가 목표: 인터넷이나 GitHub가 없어도 같은 LAN/VPN의 웹 대시보드에 로그인해
  파일 하나를 선택하고 업데이트할 수 있다. USB 복구는 별도로 유지한다.
- 현재: Mac push OTA 200 → 201 한 번과 정상 확인은 로컬 벤치에서 통과했다.
  `ota_controller.py`의 Mac 감시 방식은 구현되어 있으나 상시 실행하지 않았다.
  GitHub pull, 웹 파일 업로드 화면, 실제 실패 부팅 롤백은 아직 검증되지 않았다.
- 기존 Wi-Fi, 설치 키, 부팅 버튼 초기화, mDNS, 초기 Wi-Fi 실패 60초 AP 복귀,
  TX 차단, 파티션과 1310720-byte application 한도는 유지한다.
  GitHub는 공개 펌웨어 배포에만 사용하며 월패드 패킷·운영 데이터는 전송하지 않는다.

## 2. 하나의 서명 패키지를 두 경로에서 사용

```text
고정 도구로 빌드 → 테스트 → 개인키로 서명 → 단일 .dmota 패키지
                                   ├─ GitHub Release → ATOM 자동 확인
                                   └─ 대시보드 파일 선택 → 수동 업로드
                                                 ↓
                           공통 서명 검증 → 비활성 슬롯 기록 → 재부팅/정상 확인
```

제안하는 Release 파일은 `dmbridge-atom-lite-<version>.dmota` 하나이다.
압축 ZIP이나 전체 플래시 덤프가 아니라 **고정 크기 서명 헤더 + application.bin**으로
구성해 RAM/파일시스템에 통째로 보관하지 않고 스트리밍한다. 수동 화면에서도 같은
파일을 선택한다. `.bin` 원본만 넣는 무서명 우회 모드는 제공하지 않는다.
선택적으로 `SHA256SUMS`, 빌드 정보와 변경 설명을 함께 공개하되 신뢰 판단은
패키지의 서명된 메타데이터를 기준으로 한다.

현재 DMOTA1의 180-byte envelope는 channel을 서명하지 않는다. 새 형식은
DMOTA2 도메인과 고정 bounded binary layout으로 channel, boardId, buildId,
단조 증가 정수 version, 최소 config schema, 최소 updater protocol,
imageSize와 image SHA-256를 서명해야 한다. 전체 헤더 상한은 512 bytes로 잡고
정확한 필드/오프셋·서명 벡터를 구현 첫 단계에서 고정한다. 정규화가 불분명한
JSON을 그대로 서명하지 않는다. 기존 DMOTA1 push 호환 경로는 별도로 보존한다.

기존 Ed25519 개인키는 개발 호스트의 비공개 저장소에 유지한다. 공개 저장소와
ATOM에는 검증용 공개키만 두며, 공개키를 포함한 새 이미지가 기존 신뢰 키와
일치하는지 발행 전에 확인한다. 키 교체는 별도 마이그레이션이다.
GitHub 해시, 파일명, Release 설명이나 최신 표시만으로 설치를 허용하지 않는다.

## 3. GitHub 자동 확인 정책

초기 제안값: STA 연결 안정화 후 첫 조회에 30–60초 지연, 이후 **5분 ±30초**.
로컬 인증 화면의 '지금 확인'은 별도 속도 제한을 적용한다.

1. 고정된 `/repos/jaemyeong/dm-d5102q-rs485-bridge/releases/latest`를 HTTPS로 조회한다.
   기본 채널은 정식 `stable`; draft/prerelease는 자동 설치에서 제외한다.
   개발용 prerelease 채널은 향후 명시적인 opt-in 기능으로 분리한다.
2. Release ID/tag와 기대하는 asset 이름을 고정한 뒤 그 자산만 다운로드한다.
   중간에 latest가 바뀌어도 다른 Release의 파일과 섞지 않는다.
3. 서명 채널·보드·프로토콜·schema와 정수 버전을 검사한다. GitHub의 latest
   정렬이나 tag 문자열 비교는 anti-downgrade 판단을 대신하지 않는다.
4. 같거나 낮은 버전, 현재 pending 업데이트, 실패로 격리된 동일 version/hash는
   설치하지 않는다. 아무 Release도 없는 404는 '배포 없음'이며 재부팅하지 않는다.

공개 리소스의 Release/asset 읽기는 인증 없이 가능하므로 **GitHub PAT를 펌웨어나
NVS에 넣지 않는다**. API 버전 헤더는 현재 문서의 `2026-03-10`으로 고정하고
호환성 테스트에 포함한다. 발행자는 파일 검증이 끝난 draft를 정식으로 publish하고
latest를 명시적으로 지정한다. 자료:
[GitHub Releases API](https://docs.github.com/en/rest/releases/releases#get-the-latest-release).

비인증 REST API 기본 한도는 출발지 IP당 시간당 60회다. 5분 주기는 장치 한 대의
기본 조회 약 12회/시간이며 자산 요청과 같은 NAT의 다른 장치도 함께 계산한다.
ETag/304로 본문 전송량을 줄이되 비인증 304가 쿼터에서 면제된다고 가정하지 않는다.
403/429의 `Retry-After`/rate-limit reset을 지키고 네트워크 오류에는 지수 backoff와
jitter를 적용한다. polling 상태/ETag는 RAM 위주로 보관해 주기마다 NVS를 쓰지 않는다.
[GitHub rate limits](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api),
[REST API 권장 사용법](https://docs.github.com/en/rest/using-the-rest-api/best-practices-for-using-the-rest-api).

## 4. HTTPS, 전송과 자원 제한

- 올바른 시간 확보 후 CA 체인/호스트 이름을 검증한다. `setInsecure()`나 인증서
  오류 무시를 사용하지 않는다. GitHub 장애/인증서 만료/시간 미확보는 업데이트만
  보류하고 기존 Wi-Fi·관리 기능을 유지한다. CA 갱신과 USB 복구 계획도 남긴다.
- GitHub asset은 200 또는 302로 내려올 수 있다. HTTPS만 허용하며 hop 상한 3,
  URL 길이 상한, 검토된 정확한 GitHub/CDN 호스트 allowlist를 적용한다. 실제 자산
  경로로 목록을 확인하기 전에는 와일드카드 호스트를 허용하지 않는다.
  외부 임의 URL·HTTP downgrade·루프는 거부하고 redirect query는 로그에 남기지 않는다.
  [GitHub asset API](https://docs.github.com/en/rest/releases/assets#get-a-release-asset).
- JSON/헤더/Release 설명의 크기를 제한한다. 큰 메타데이터는 bounded streaming
  parser로 필요한 필드만 읽거나 명시적으로 중단하며 무제한 버퍼 확장은 금지한다.
- 패키지 헤더와 서명을 먼저 확인한 후에만 flash 쓰기를 시작한다. application을
  작은 고정 버퍼로 비활성 슬롯에 기록하고 전체 길이와 SHA-256가 맞을 때만 선택한다.
  filesystem, bootloader, partition table, Wi-Fi/enrollment NVS는 갱신하지 않는다.
- 연결/TLS/본문 idle 및 전체 전송 deadline을 각각 둔다. 현재 8초 idle/120초 전송,
  10초 watchdog을 검토 출발점으로 삼고 TLS 작업이 loop/watchdog/web 처리를 막지
  않도록 분리한다. 실제 HTTPS 최소 free heap, largest block, stack과 지연을 측정한다.
  기존 이미지 832080 bytes/여유 478640 bytes는 TLS 추가가 통과했다는 증거가 아니다.
- 자동 pull·웹 수동·기존 Mac push는 단일 OTA coordinator/잠금을 공유한다.
  하나가 진행 중이면 다른 시작은 BUSY로 응답하고 두 writer를 동시에 열지 않는다.

## 5. 웹 대시보드 수동 업로드

기존 HTML 로그인 뒤 '펌웨어 업데이트' 화면에 현재 버전/상태, 자동 확인 상태,
마지막 확인 결과, '지금 확인', '파일 선택', '업데이트 시작'을 제공한다.

1. `.dmota` 파일 선택 → 헤더를 읽어 대상 보드·버전·크기를 미리 표시한다.
   파일명/설명은 text로 렌더링하며 업로드 전 명시적인 시작 버튼이 필요하다.
2. 현재 SHA-256 Digest 로그인, same-origin, CSRF를 유지한다. 준비 요청에서
   기기가 서명을 검증한 후 발급하는 단기 일회용 토큰으로만 본문을 보낸다.
   브라우저의 사전 검사는 보조일 뿐이며 기기가 모든 검사를 다시 수행한다.
3. 파일 전체 multipart 파서를 새로 만들기보다 File/Blob slice와 bounded binary
   prepare/upload를 사용한다. 큰 application을 기기 RAM에 모으지 않는다.
4. UI는 '전송 중 / 기기 검증 중 / 재부팅 중 / 정상 확정 / 실패·복구'를 구분한다.
   전송률 100%만으로 성공을 표시하지 않는다. 응답이 끊겨도 재업로드하지 않고
   재접속 후 인증된 실제 build/version/transaction 결과를 확인한다.

사용자가 브라우저를 닫거나 iPad Safari가 백그라운드로 가더라도 **완전한 파일 검증
후의 재부팅 확정은 기기가 담당**하도록 설계한다. 전송 도중 브라우저가 종료되면
미완료 이미지로 전환하지 않는다. 인터넷이 없는 기존 LAN에서도 이 경로를 시험한다.
AP 최초 온보딩 상태까지 수동 OTA를 허용하는 변경은 이번 계획에 포함하지 않는다.

## 6. 가장 중요한 변경: 부팅 확정과 실패 격리

현재 `Runtime::confirm`은 외부 Mac의 인증된 ACK가 있어야 30초 이내에 VALID로
확정된다. 이대로는 Mac 없는 pull OTA가 완료되지 않는다.

- `legacy-push`는 기존 외부 ACK 정책을 유지한다. 새 `github-pull`/`web-file`은
  승인된 진입점에서만 생성되는 영속 update-origin/context에 따라 로컬 건강 확인을
  사용한다. 인증 없는 플래그나 임의 요청으로 push의 ACK를 우회하면 안 된다.
- 로컬 확정은 예상 서명 이미지/build/version, 유효한 설정, STA IPv4, 웹/OTA 서비스,
  TX 차단, heap 한도와 연속 loop 정상 동작을 확인한 뒤에만 수행한다. 제안 안정
  관찰 5초를 포함하되 기존 부팅 30초 deadline 안에 끝내며 즉시 무조건 확정하지 않는다.
  GitHub에 다시 접속하는 것 자체를 부팅 성공 조건으로 두지 않는다.
- 실패/timeout/watchdog 시 검증된 이전 OTA 이미지로 돌아가고 동일 실패 release의
  재설치를 막는다. 정상 이미지로 확정하기 전 새 버전의 high-water mark를 올려
  bootloader 복귀를 막지 않는다. 실패 후보는 version/hash로 영속 격리하며
  새로운 버전이나 인증된 명시적 해제 없이는 재시도하지 않는다.
- **하위 호환 주의:** 현재 `dmboot/ota`는 길이와 CRC가 고정되어 다른 길이를 쓰면
  이전 펌웨어가 OTA_JOURNAL_ERROR가 된다. 기존 blob을 바꾸지 않고 추가 키에
  버전 있는 origin/실패 정보를 기록하는 호환 설계를 먼저 시험한다. 구버전으로
  rollback해도 설정·원격 복구가 살아 있어야 한다. 원자성/전원 차단 지점을 테스트한다.
- 당장은 원격 downgrade 우회를 만들지 않는다. 과거 코드를 복구 배포해야 하면
  더 높은 버전으로 재빌드·서명한다. bootloader의 직전 정상 슬롯 rollback은 별개다.

이는 self-test 후 VALID/INVALID를 기록하는 SDK 기능을 이용하는 **예정 설계**이며,
실제 전원 차단·반복 슬롯·rollback 시험을 통과하기 전 복구를 보장하지 않는다.
[고정 IDF 4.4.7 OTA 계약](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/ota.html#app-rollback).

## 7. 구현 순서와 통과 기준

| 단계 | 작업 | 통과 증거 |
| --- | --- | --- |
| G0 | 공개 저장소/배포 제외 목록과 본 계획 | exact owner/name/visibility, staged secret 검사, 원격 commit 일치 |
| G1 | signed single-file 형식, 공통 coordinator, 호환 journal, 로컬 확정 | host 변조/권한/동시성/timeout/전원 중단 모델 테스트, 기존 회귀 유지 |
| G2 | 웹 수동 업로드 UI/API | 크기 제한·CSRF·서명·재전송 방지, 실제 iPad Safari와 오프라인 LAN 테스트 |
| G3 | GitHub HTTPS 조회/다운로드 state machine | 404/304/403/429, redirect/TLS/잘못된 시간, oversize, 끊김 테스트 및 image/heap gate |
| G4 | 기존 Mac push로 신규 updater를 한 번 설치 | 정확한 상위 버전 이미지, Wi-Fi/키 유지, 구 push 확정과 USB 복구 경로 유지 |
| G5 | 새 이미지의 수동/자동 OTA와 실패 복구 | 두 슬롯 순환, 실제 rollback·watchdog·전원 차단, 실패 release 반복 방지 |
| G6 | 검증된 자동 업데이트 활성화 | Mac 없이 GitHub Release 발견·설치·자체 VALID, 운영 기록·중단 정책·최종 승인 |

현재 0.2.1은 아직 GitHub나 새 패키지/UI를 이해하지 못한다. G4 전환 이미지는
기존 DMOTA1 Mac push 형식으로 배포하며 최초 전환 중에는 Mac ACK를 유지한다.
G4/G5의 이미지는 각각 별도 버전·서명·증거로 분리한다. Release 파일을 올리는 것만으로
현재 설치된 0.2.1이 자동 갱신된다고 안내하지 않는다.

발행 초기에는 로컬에서 테스트·서명 후 draft에 자산을 올리고 검증한 뒤 publish한다.
추후 CI 빌드/서명 자동화는 별도 단계다. 비공개 키를 workflow 파일에 쓰거나
이번 작업에서 GitHub Actions secrets에 복사하지 않는다. 인터넷 pull은 업데이트
시점의 Mac 의존성만 제거하며 모든 장치의 성공을 GitHub가 자동 보고받는 것은 아니다.

현장 전원/극성/분리/수동 복구, Tail485 passivity/RX 및 72시간 soak는 별도 gate다.
OTA와 재부팅으로 생기는 서비스 공백을 무손실 RS485 증거로 계산하지 않는다.
