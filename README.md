# System_Programming_Stock_Server_Project

시스템프로그래밍 수업에서 진행한 동시성 기반 주식 서버 프로젝트입니다. 고전적인 echo server 구조를 확장해 여러 클라이언트가 동시에 접속할 수 있는 주식 매매 서비스를 구현했습니다.

개인 정보가 포함된 원본 제출 보고서는 공개 저장소에서 제외했습니다.

## 개요

이 프로젝트는 동일한 문제를 두 가지 동시성 모델로 구현합니다.

- `task1`: `select()` 기반 이벤트 드리븐 서버
- `task2`: 마스터 스레드 + 공유 버퍼 + 워커 스레드 풀 기반 서버

두 구현 모두 다음 기능을 제공합니다.

- `stock.txt`의 데이터를 메모리 이진 탐색 트리로 로드
- TCP 소켓을 통한 클라이언트 명령 처리
- `show`, `buy`, `sell`, `exit` 명령 지원
- 매수/매도 요청에 따라 수량 갱신
- `SIGINT` 발생 시 변경 내용을 `stock.txt`에 저장

## 주요 구현 포인트

- `select()`를 이용한 I/O multiplexing
- pthread 기반 스레드 풀 동시성 처리
- 생산자-소비자 구조의 공유 연결 버퍼
- 주식 트리 노드 단위 readers-writers 동기화
- 파일 기반 영속 상태 관리
- 클라이언트 수와 부하 패턴에 따른 성능 비교

## 프로젝트 구조

```text
.
|-- task1
|   |-- stockserver.c
|   |-- stockclient.c
|   |-- multiclient.c
|   |-- echo.c
|   |-- csapp.c
|   |-- csapp.h
|   |-- stock.txt
|   `-- Makefile
`-- task2
    |-- stockserver.c
    |-- stockclient.c
    |-- multiclient.c
    |-- echo.c
    |-- csapp.c
    |-- csapp.h
    |-- stock.txt
    `-- Makefile
```

## Task 1: 이벤트 드리븐 서버

`task1`은 단일 프로세스 기반 서버입니다. `fd_set`으로 listening socket과 여러 client descriptor를 함께 관리하고, 준비된 소켓만 처리하는 방식으로 동작합니다.

스레드 생성 비용 없이 여러 클라이언트를 동시에 처리하는 구조를 직접 구현하는 데 초점을 두었습니다.

## Task 2: 스레드 기반 서버

`task2`는 워커 스레드 풀 기반 서버입니다. 메인 스레드는 연결을 accept한 뒤 descriptor를 공유 버퍼에 넣고, 워커 스레드가 이를 꺼내 각 클라이언트를 처리합니다.

공유 버퍼에는 세마포어 기반 producer-consumer 동기화를 적용했고, 주식 데이터 접근에는 readers-writers 방식의 세밀한 동기화를 적용했습니다.

## 실행 방법

각 task는 독립적으로 빌드할 수 있습니다.

```bash
cd task1
make
./stockserver <port>
```

스레드 버전:

```bash
cd task2
make
./stockserver <port>
```

클라이언트 예시:

```bash
./stockclient <host> <port>
./multiclient <host> <port> <client_num>
```

## 메모

- Linux 계열 환경을 기준으로 작성했습니다.
- 두 구현을 분리해 두어 이벤트 드리븐 방식과 스레드 기반 방식을 직접 비교할 수 있도록 구성했습니다.
- 보고서 자체는 공개하지 않았지만, 저장소 구조와 구현은 실제 과제 실험 환경을 반영합니다.
