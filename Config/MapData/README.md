# Main 맵 오브젝트 데이터

`Main_LevelObjects.json`은 저장된 `/Game/Maps/Main`에서 추출한 데이터입니다. 좌표는 Unreal 월드 좌표(cm), 회전은 도(degree) 단위입니다. 에디터에서 아직 저장하지 않은 변경은 포함되지 않습니다.

- `treasure_box.spawn_points`: `Arena/Floor/Tiles` 소속 타일 중앙의 스폰 후보입니다. 현재 199개 타일 중 112개를 포함합니다. `position`은 기본 크기, 기본 회전의 TreasureBox 액터를 배치할 좌표이며, 타일 윗면에서 상자 밑면까지 2cm 여유를 둡니다. `surface_position`은 타일 윗면 좌표입니다.
- `treasure_box.spawn_rules`: 후보 목록에서 중복 없이 선택하며, 목록 외의 임의 좌표는 허용하지 않습니다. 유효한 후보가 없으면 스폰을 건너뜁니다. 타일당 상자 한 개만 허용합니다.
- `treasure_box.excluded_tiles`: 타일 중앙에 상자를 놓을 때 다른 오브젝트의 충돌 바운딩 박스와 겹치는 타일 87개와 차단 액터 이름을 기록합니다. 보수적인 바운딩 박스 검사이므로 실제 메시 사이에는 공간이 있어도 제외될 수 있습니다.
- `crystal_towers`: 현재 배치된 타워 3개의 액터 식별 정보, 위치, 회전, 스케일, 메시, 머티리얼, 피격 발광 및 조명 설정입니다.

이 파일은 데이터이며 현재 게임/서버 스폰 코드에 자동으로 적용되지 않습니다. 소비하는 코드에서 `spawn_points`만 사용하고, 생성 직전에 해당 타일의 지지면과 상자 충돌 공간을 다시 검사해야 합니다. `folder`와 `actor_label`은 에디터용 식별 정보입니다. 런타임에서는 후보 좌표와 액터 참조 또는 이름을 사용하고, 유효하지 않은 후보는 건너뜁니다. 현재 추출 과정은 물리 트레이스 대신 수직으로 배치된 육각형 `SM_Ground`의 윗면과 장애물 바운딩 박스를 사용합니다.

맵을 수정했다면 저장한 다음 프로젝트 루트의 PowerShell에서 다시 추출할 수 있습니다. 맵이나 에셋을 수정하지 않고 JSON만 갱신합니다.

```powershell
$projectRoot = (Get-Location).Path.Replace('\', '/')
& 'C:/Program Files/Epic Games/UE_5.4/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
    "$projectRoot/UE_StickArenaDemo.uproject" `
    -run=pythonscript "-script=$projectRoot/Tools/ExportLevelObjects.py" `
    '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' `
    -unattended -nullrhi -nosound
```
