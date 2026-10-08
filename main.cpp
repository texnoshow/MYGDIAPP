#include <windows.h>
#include <mmsystem.h>
#include <ctime>

// Подключаем библиотеку для работы со звуком
#pragma comment(lib, "winmm.lib")

// Настройки аудио (классический Bytebeat)
const int SAMPLE_RATE = 8000; 
const int BUFFER_SIZE = 4000; 

volatile DWORD globalT = 0;
volatile bool isRunning = true;

// Первая оригинальная формула Bytebeat
inline BYTE GenerateBytebeat(DWORD t) {
    return static_cast<BYTE>((((t * (t >> 8 | t >> 9) & 46 & t >> 8)) ^ (t & t >> 13 | t >> 6)) & 0xFF);
}

// -------------------------------------------------------------
// ПОТОК ЗВУКА: Работает независимо в фоне
// -------------------------------------------------------------
DWORD WINAPI AudioThreadFunc(LPVOID lpParam) {
    HWAVEOUT hWaveOut = (HWAVEOUT)lpParam;
    
    char* audioBuf1 = new char[BUFFER_SIZE];
    char* audioBuf2 = new char[BUFFER_SIZE];

    WAVEHDR waveHdr1 = { audioBuf1, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };
    WAVEHDR waveHdr2 = { audioBuf2, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };

    waveOutPrepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutPrepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    for (int i = 0; i < BUFFER_SIZE; ++i) audioBuf1[i] = GenerateBytebeat(InterlockedIncrement(&globalT));
    for (int i = 0; i < BUFFER_SIZE; ++i) audioBuf2[i] = GenerateBytebeat(InterlockedIncrement(&globalT));

    waveOutWrite(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    WAVEHDR* currentHeader = &waveHdr1;

    while (isRunning) {
        while (!(currentHeader->dwFlags & WHDR_DONE) && isRunning) {
            Sleep(1); 
        }

        if (!isRunning) break;

        for (int i = 0; i < BUFFER_SIZE; ++i) {
            DWORD t = InterlockedIncrement(&globalT);
            currentHeader->lpData[i] = GenerateBytebeat(t);
        }

        waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));
        currentHeader = (currentHeader == &waveHdr1) ? &waveHdr2 : &waveHdr1;
    }

    waveOutUnprepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));
    delete[] audioBuf1;
    delete[] audioBuf2;

    return 0;
}

// -------------------------------------------------------------
// ГЛАВНЫЙ ПОТОК: Расширенное визуальное шоу
// -------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    // ИСПРАВЛЕНО: Использование MessageBoxW и широких строк L"" для поддержки Unicode
    int msgBoxResponse = MessageBoxW(
        NULL, 
        L"Внимание!\n\nДанная программа содержит интенсивные визуальные эффекты, быстрое мерцание экрана и громкий 8-битный звук.\n\nВы действительно хотите запустить демонстрацию?", 
        L"Предупреждение демо-сцены", 
        MB_YESNO | MB_ICONWARNING | MB_TOPMOST
    );

    // Если пользователь нажал "Нет" (или закрыл окно), программа безопасно завершает работу
    if (msgBoxResponse == IDNO) {
        return 0; 
    }

    srand(static_cast<unsigned int>(time(0)));

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

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

    HANDLE hAudioThread = CreateThread(NULL, 0, AudioThreadFunc, hWaveOut, 0, NULL);

    while (true) {
        DWORD t = globalT; 
        BYTE soundValue = GenerateBytebeat(t);

        HDC hdcScreen = GetDC(0);

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
            DrawIcon(hdcScreen, x, y, hIcon);
        }
        else if (payloadMode == 5) {
            if (soundValue > 230) {
                PatBlt(hdcScreen, 0, 0, screenWidth, screenHeight, DSTINVERT);
            }
        }

        ReleaseDC(0, hdcScreen);
        Sleep(2); 
    }

    isRunning = false;
    if (hAudioThread != NULL) {
        WaitForSingleObject(hAudioThread, INFINITE);
        CloseHandle(hAudioThread);
    }
    waveOutReset(hWaveOut);
    waveOutClose(hWaveOut);

    return 0;
}
