# UE_StickArenaDemo

Unreal Engine 기반 Stick Arena 게임 클라이언트입니다. 멀티플레이를 위해 별도의 UDP 게임 서버를 먼저 실행해야 합니다.

## 개발 환경

- 개발에 사용한 엔진: **Unreal Engine 5.4.4**
- `UE_StickArenaDemo.uproject`의 엔진 연결 버전: `5.4`
- 실행 및 패키징 대상: Windows

## 게임 실행 시 서버 IP 주소 설정

기본 서버 주소는 `127.0.0.1`, 포트는 `7777`입니다. `127.0.0.1`은 게임을 실행하는 PC 자신을 가리킵니다. 서버가 다른 PC에 있다면 해당 서버 PC의 IP 주소로 변경해야 합니다.

### 실행 인수로 설정

패키징된 게임의 실행 파일이 있는 폴더에서 PowerShell을 열고 아래와 같이 실행합니다. `192.168.0.10`은 실제 서버 PC의 IP 주소로 바꾸고, 포트도 서버 설정에 맞춥니다.

```powershell
.\UE_StickArenaDemo.exe -SAServerAddress=192.168.0.10 -SAServerPort=7777
```

같은 PC에서 서버와 게임을 실행하는 경우:

```powershell
.\UE_StickArenaDemo.exe -SAServerAddress=127.0.0.1 -SAServerPort=7777
```

바로가기로 실행하려면 바로가기의 **속성 → 대상**에서 실행 파일 경로 뒤에 인수를 추가합니다.

```text
"C:\Games\StickArena\UE_StickArenaDemo.exe" -SAServerAddress=192.168.0.10 -SAServerPort=7777
```

실행 인수는 아래 설정 파일의 기본값보다 우선하며, IP를 바꿀 때 게임을 다시 패키징할 필요가 없습니다. 서버 주소에는 숫자 형식의 IP 주소를 입력합니다.

### 프로젝트 기본값 설정

에디터에서 실행하거나 패키징할 때 사용할 기본값은 `Config/DefaultGame.ini`에서 변경합니다.

```ini
[/Script/UE_StickArenaDemo.SAUdpClientGameInstance]
ServerAddress=192.168.0.10
ServerPort=7777
```

설정 변경 후 게임을 다시 실행합니다. 에디터에서 플레이 중이었다면 플레이를 종료하고 다시 시작합니다. 이 파일을 수정해 배포할 기본값을 바꾸려면 다시 패키징합니다.

서버가 다른 PC에 있는 경우 서버 PC에서 `ipconfig`로 IPv4 주소를 확인할 수 있습니다. 같은 네트워크에서는 서버 PC의 내부 IP를 사용하며, 서버의 방화벽에서 설정한 UDP 포트의 수신을 허용해야 합니다. 외부 네트워크에서 접속할 경우에는 서버의 외부 접속 주소와 라우터의 UDP 포트 포워딩 설정도 필요합니다.

연결 구조와 Blueprint API는 [UDP 클라이언트 문서](Docs/UdpClient.md)를 참고하세요.
