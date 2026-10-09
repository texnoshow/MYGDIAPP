#include <windows.h>
#include <mmsystem.h>
#include <ctime>
#include <atomic> // Используем для безопасной синхронизации между потоками

// Линковка мультимедийной библиотеки для воспроизведения звука
#pragma comment(lib, "winmm.lib")

// Конфигурация аудиопотока
const int SAMPLE_RATE = 8000; 
const int BUFFER_SIZE = 4000; 

// Атомарные переменные для предотвращения Data Race (состояния гонки)
std::atomic<DWORD> globalT{0};
std::atomic<bool> isRunning{true};

// Новая адаптированная формула Bytebeat (эффект глитча / зависания звука)
inline BYTE GenerateBytebeat(DWORD t) {
    return static_cast<BYTE>(((t % 64) ^ (t >> 3)) * 4);
}

// -------------------------------------------------------------
// АУДИОПОТОК: Фоновая генерация звуковой волны в реальном времени
// -------------------------------------------------------------
DWORD WINAPI AudioThreadFunc(LPVOID lpParam) {
    HWAVEOUT hWaveOut = (HWAVEOUT)lpParam;
    
    char* audioBuf1 = new char[BUFFER_SIZE];
    char* audioBuf2 = new char[BUFFER_SIZE];

    WAVEHDR waveHdr1 = { audioBuf1, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };
    WAVEHDR waveHdr2 = { audioBuf2, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };

    waveOutPrepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutPrepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    // Первичное заполнение буферов
    for (int i = 0; i < BUFFER_SIZE; ++i) audioBuf1[i] = GenerateBytebeat(globalT.fetch_add(1));
    for (int i = 0; i < BUFFER_SIZE; ++i) audioBuf2[i] = GenerateBytebeat(globalT.fetch_add(1));

    waveOutWrite(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    WAVEHDR* currentHeader = &waveHdr1;

    while (isRunning.load()) {
        // Ожидаем, пока звуковая карта освободит текущий буфер
        while (!(currentHeader->dwFlags & WHDR_DONE) && isRunning.load()) {
            Sleep(1); 
        }

        if (!isRunning.load()) break;

        // Заполняем освободившийся буфер новыми значениями из формулы
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            DWORD t = globalT.fetch_add(1);
            currentHeader->lpData[i] = GenerateBytebeat(t);
        }

        waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));
        currentHeader = (currentHeader == &waveHdr1) ? &waveHdr2 : &waveHdr1;
    }

    // Корректная очистка аудио-ресурсов
    waveOutUnprepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));
    delete[] audioBuf1;
    delete[] audioBuf2;

    return 0;
}

// -------------------------------------------------------------
// ГЛАВНЫЙ ПОТОК: Отрисовка графики GDI на экране
// -------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    int msgBoxResponse = MessageBoxA(
        NULL, 
        "WARNING!\n\nThis application contains intense flashing visual effects, rapid screen flashing, and loud 8-bit audio.\n\nPress ESCAPE key during execution to stop the demo.\n\nDo you want to run the demonstration?", 
        "Demo Scene Warning", 
        MB_YESNO | MB_ICONWARNING | MB_TOPMOST
    );

    if (msgBoxResponse == IDNO) {
        return 0; 
    }

    srand(static_cast<unsigned int>(time(0)));

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Настройка формата аудио 8-бит Моно
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.nAvgBytesPerSec = SAMPLE_RATE;
    wfx.nBlockAlign = 1;
    wfx.wBitsPerSample = 8;

    HWAVEOUT hWaveOut = nullptr;
    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return 1;
    }

    // Запуск фонового аудиопотока
    HANDLE hAudioThread = CreateThread(NULL, 0, AudioThreadFunc, hWaveOut, 0, NULL);

    // Главный цикл теперь проверяет флаг isRunning и нажатие клавиши ESCAPE
    while (isRunning.load()) {
        if (GetAsyncKeyState(VK_ESCAPE)) {
            isRunning.store(false); // Штатный выход по нажатию ESC
            break;
        }

        DWORD t = globalT.load(); 
        BYTE soundValue = GenerateBytebeat(t);

        HDC hdcScreen = GetDC(0);
        if (!hdcScreen) continue;

        int payloadMode = (t >> 14) % 6; 

        if (payloadMode == 0) {
            int w = (soundValue % 200) + 50;  
            int h = ((soundValue >> 2) % 200) + 50;  
            int x = rand() % (screenWidth - w);
            int y = rand() % (screenHeight - h);

            COLORREF syncColor = RGB(soundValue, (soundValue ^ (t >> 4)) & 0xFF, (t >> 8) & 0xFF);
            HBRUSH hBrush = CreateSolidBrush(syncColor);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcScreen, hBrush);

            PatBlt(hdcScreen, x, y, w, h, PATINVERT);

            SelectObject(hdcScreen, hOldBrush);
            DeleteObject(hBrush);
        } 
        else if (payloadMode == 1) {
            int x1 = rand() % screenWidth;
            int y1 = rand() % screenHeight;
            int x2 = rand() % screenWidth;
            int y2 = rand() % screenHeight;

            COLORREF lineColor = RGB((t >> 5) & 0xFF, soundValue, 255 - soundValue);
            HPEN hPen = CreatePen(PS_SOLID, 2, lineColor);
            HPEN hOldPen = (HPEN)SelectObject(hdcScreen, hPen);

            MoveToEx(hdcScreen, x1, y1, NULL);
            LineTo(hdcScreen, x2, y2);

            SelectObject(hdcScreen, hOldPen);
            DeleteObject(hPen);
        } 
        else if (payloadMode == 2) {
            int radius = (soundValue % 100) + 20;
            int x = rand() % screenWidth;
            int y = rand() % screenHeight;

            COLORREF circleColor = RGB(255 - soundValue, (t >> 6) & 0xFF, soundValue);
            HBRUSH hBrush = CreateSolidBrush(circleColor);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcScreen, hBrush);

            Ellipse(hdcScreen, x - radius, y - radius, x + radius, y + radius);

            SelectObject(hdcScreen, hOldBrush);
            DeleteObject(hBrush);
        }
        else if (payloadMode == 3) {
            int x = rand() % screenWidth;
            int y = rand() % screenHeight;
            int w = rand() % 300 + 50;
            int h = rand() % 300 + 50;
            
            BitBlt(hdcScreen, x + (soundValue % 10) - 5, y + (soundValue % 10) - 5, w, h, hdcScreen, x, y, SRCCOPY);
        }
        else if (payloadMode == 4) {
            int x = rand() % screenWidth;
            int y = rand() % screenHeight;
            
            LPCSTR iconType = IDI_APPLICATION;
            if (soundValue % 3 == 0) iconType = IDI_WARNING;
            else if (soundValue % 3 == 1) iconType = IDI_ERROR;
            
            HICON hIcon = LoadIconA(NULL, iconType);
            if (hIcon) {
                DrawIcon(hdcScreen, x, y, hIcon);
                DestroyIcon(hIcon); // ИСПРАВЛЕНО: Уничтожаем дескриптор, чтобы избежать GDI Leak
            }
        }
        else if (payloadMode == 5) {
            int w = rand() % 400 + 200;  
            int h = rand() % 400 + 200;  
            int x = rand() % (screenWidth - w);
            int y = rand() % (screenHeight - h);
            
            PatBlt(hdcScreen, x, y, w, h, DSTINVERT);
        }

        ReleaseDC(0, hdcScreen);
        Sleep(2); 
    }

    // Корректное завершение потоков и освобождение аудиокарты Windows
    isRunning.store(false);
    if (hAudioThread != NULL) {
        WaitForSingleObject(hAudioThread, INFINITE);
        CloseHandle(hAudioThread);
    }
    waveOutReset(hWaveOut);
    waveOutClose(hWaveOut);

    return 0;
}
