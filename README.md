# Trace Lab

Unreal Engine 5와 C++를 사용하여 개발하는 추격 AI 게임 프로젝트입니다.

플레이어가 남긴 흔적과 소리를 AI가 감지하고, 증거를 조사하거나 플레이어를 직접 추격하는 게임을 목표로 합니다.

## 주요 기능

* AI 무작위 순찰
* AI Perception 기반 시야 및 청각 감지
* 플레이어가 남긴 증거 탐색
* 증거 위치 조사
* 플레이어 추격
* 마지막 목격 위치 주변 수색
* 공격 및 게임 종료 처리

## 현재 구현 상태

* [x] AI Controller 기본 구조 생성
* [x] AI Perception 컴포넌트 설정
* [x] 플레이어 시야 감지 설정
* [x] 랜덤 순찰 위치 선택
* [x] NavMesh 기반 순찰 이동 확인
* [x] 순찰 위치 디버그 로그 출력
* [ ] 증거 시스템과 AI Controller 연동
* [ ] 플레이어 감지 시 추격 상태 전환
* [ ] 플레이어 마지막 목격 위치 저장
* [ ] 마지막 목격 위치 주변 수색
* [ ] 공격 상태 및 공격 판정
* [ ] 전체 상태 전환 테스트

## AI 상태 설계

현재 AI 행동은 다음 상태를 기준으로 구현할 예정입니다.

* `Patrol`: 맵을 무작위로 순찰
* `Investigate`: 발견한 증거 위치를 조사
* `Chase`: 감지한 플레이어를 추격
* `Search`: 플레이어를 놓친 위치 주변을 수색
* `Attack`: 공격 범위에 들어온 플레이어를 공격

## 기본 행동 흐름

```text
Patrol
  ├─ 증거 발견 → Investigate
  └─ 플레이어 발견 → Chase

Investigate
  ├─ 플레이어 발견 → Chase
  └─ 조사 완료 → Patrol

Chase
  ├─ 공격 거리 진입 → Attack
  └─ 플레이어 시야 상실 → Search

Search
  ├─ 플레이어 재발견 → Chase
  └─ 수색 실패 → Patrol

Attack
  ├─ 플레이어가 멀어짐 → Chase
  └─ 공격 성공 → 게임 종료
```

## 개발 환경

* Unreal Engine 5
* C++
