# Magikarp

ESP32 Dev Module로 240×240 원형 GC9A01 LCD에 잉어킹처럼 멍한 눈 하나를 그리는 ESP-IDF 프로젝트입니다. 화면 전체가 흰자위이고, 검은 눈동자가 커졌다 작아진 뒤 팽그르르 돕니다.

지금은 눈 1개만 동작합니다. 같은 LCD를 하나 더 달 자리는 핀만 잡아 두었습니다.

## 하드웨어

| 항목 | 값 |
| --- | --- |
| MCU | ESP32-D0WD-V3 (Dev Module) |
| LCD | 1.28인치, 240×240, GC9A01 |
| 버스 | SPI3, 모드 0, 40 MHz |
| 색 | RGB565, BGR, 색 반전 |
| 플래시 | 4 MB, DIO, 80 MHz |
| 백라이트 | 없음 (BLK 핀 없음) |

이 보드에서 확인한 포트는 Silicon Labs CP210x **COM3**입니다.

CS 패드가 PCB에서 GND에 붙어 있으면 GPIO5를 연결하지 않습니다. 그 상태에서는 GPIO를 CS에 물으면 핀이 접지로 단락됩니다. CS가 모듈 안에서 이미 GND라면 두 LCD는 한 SPI 버스를 나눌 수 없어서, 눈마다 SPI 호스트를 따로 씁니다.

## 핀 맵

### 눈 1 (현재 펌웨어, SPI3)

| LCD | ESP32 | 역할 |
| --- | --- | --- |
| GND | GND | 전원 |
| VCC | 3.3 V | 전원 |
| SCL | GPIO18 | SPI 클럭 |
| SDA | GPIO23 | SPI MOSI. I2C SDA가 아닙니다 |
| RES | GPIO22 | 리셋 |
| DC | GPIO21 | 데이터/명령 |
| CS | GPIO5 | 칩 선택. 패드가 GND에 단락돼 있으면 연결하지 않음 |
| BLK | 없음 | |

### 눈 2 (예정, SPI2, 미구현)

| LCD | ESP32 | 역할 |
| --- | --- | --- |
| SCL | GPIO14 | SPI 클럭 |
| SDA | GPIO13 | SPI MOSI |
| RES | GPIO26 | 리셋 |
| DC | GPIO27 | 데이터/명령 |
| CS | GPIO25 | 칩 선택 |

## 화면

패널을 시계 반대 방향으로 90도 돌린 뒤 출력합니다 (`swap_xy` + Y 미러). 하이라이트가 눈동자 왼쪽 아래에 보이면 회전이 맞습니다.

| 요소 | 값 |
| --- | --- |
| 흰자위 | 화면 전체, RGB(255, 252, 246) |
| 눈동자 | 검은색, 지름 16–48 px |
| 하이라이트 | 눈동자 안 작은 흰 점, 눈동자와 함께 이동 |
| 궤도 | 화면 중심에서 최대 40 px (가장자리까지 거리의 약 1/3) |

잔상을 줄이려고 처음 한 번만 화면을 채우고, 이후에는 이전 눈동자와 현재 눈동자가 걸친 사각형만 다시 그립니다.

## 동작

한 주기는 22.4초이고, 끝나면 처음부터 반복합니다.

| 순서 | 시간 | 내용 |
| --- | --- | --- |
| 1 | 5.0초 × 2 | 중앙에서 48 px → 16 px → 48 px. 두 번 |
| 2 | 2.0초 | 48 px로 중앙에서 정지 |
| 3 | 2.5초 | 48 px → 16 px로 줄어듦 |
| 4 | 5.4초 | 16 px를 유지한 채 세 바퀴. 앞 1.5바퀴는 밖으로 나가며, 뒤 1.5바퀴는 중앙으로 돌아오며 회전 |
| 5 | 2.5초 | 중앙에서 16 px → 48 px. 커지기 시작하는 크기는 회전이 끝난 16 px와 같음 |

타이밍 상수는 `main/pupil.h`에 있습니다.

## 빌드

ESP-IDF **5.5.2**로 빌드합니다. GC9A01 드라이버는 컴포넌트 `espressif/esp_lcd_gc9a01` 1.2.0입니다.

PowerShell:

```powershell
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:Path = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;" + $env:Path
. C:\Espressif\frameworks\esp-idf-v5.5.2\export.ps1
cd C:\Users\painnick\Documents\Projects\Magikarp-idf
idf.py build
idf.py -p COM3 flash
```

모니터는 포트를 잡습니다. 확인 후 닫아야 다음 플래시가 됩니다. 정상 부팅이면 로그에 `wide eye started`가 나옵니다.

기하 검사는 호스트에서 Python으로 돌립니다.

```powershell
python test\test_pupil.py
```

## 소스

| 파일 | 내용 |
| --- | --- |
| `main/main.c` | SPI, 패널 초기화, 부분 갱신 루프 |
| `main/pupil.c` | 눈동자 크기, 위치, 픽셀 색 |
| `main/pupil.h` | 크기, 시간, 궤도 상수 |
| `test/test_pupil.py` | 위 수식과 같은 호스트 검사 |
