# UDP 클라이언트 연결

프로젝트의 기본 GameInstance는 `USAUdpClientGameInstance`입니다.

- GameInstance: 소켓 생성·종료, 패킷 송수신, 서버 주소 검사, 세션 ID, 등록/재등록, 패킷 및 요청 시퀀스를 관리합니다. 수신과 등록 재시도는 게임 스레드의 Core Ticker에서 처리합니다.
- `USAUdpClientComponent`: 현재 로컬 Pawn의 상태와 외형을 수집하고 수신 데이터를 원격 캐릭터, 상자, 크리스탈, HUD에 반영합니다. 소켓을 소유하지 않습니다.

Pawn의 `EndPlay`는 컴포넌트 연결과 해당 월드의 오브젝트만 정리합니다. GameInstance의 소켓과 세션은 Pawn 교체 및 레벨 이동 동안 유지합니다. 새 Pawn은 기존 연결에 붙고 외형 등록을 갱신합니다. Pawn이 없는 동안에도 GameInstance가 등록을 재전송해 서버의 세션 만료를 방지합니다.

명시적 `DisconnectFromServer`, GameInstance `Shutdown`, 프로그램 종료 시에는 LEAVE를 보낸 뒤 소켓을 닫습니다. 명시적 재접속은 새 세션 ID를 사용합니다.

## 접속 설정

서버 주소와 포트는 캐릭터 컴포넌트 대신 `Config/DefaultGame.ini`에서 설정합니다.

```ini
[/Script/UE_StickArenaDemo.SAUdpClientGameInstance]
ServerAddress=127.0.0.1
ServerPort=7777
```

실행 인수 `-SAServerAddress=127.0.0.1 -SAServerPort=7777`로 덮어쓸 수 있습니다. Blueprint에서는 Get Game Instance를 `SAUdpClientGameInstance`로 캐스팅한 뒤 `ConnectToServer`, `DisconnectFromServer`, `IsConnected`, `IsRegistered`, `GetSessionId`를 사용할 수 있습니다. UDP에서 `IsConnected`는 소켓이 열린 상태이며, 서버의 등록 ACK 수신 여부는 `IsRegistered`로 확인합니다.

클래스와 속성이 변경됐으므로 기존 에디터는 재시작해야 합니다. 패키징할 때에는 프로젝트의 기존 패키징 스크립트로 정식 Editor/Game 타깃을 빌드합니다.
