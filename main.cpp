#include <windows.h>
#include <mmsystem.h>
#include <ctime>

// Подключаем библиотеку для работы со звуком
#pragma comment(lib, "winmm.lib")

// Настройки аудио (классический Bytebeat)
const int SAMPLE_RATE = 8000; 
const int BUFFER_SIZE = 4000; 

// Глобальные переменные старого типа (WinAPI Thread-safe не требуется для простых типов в таком режиме)
volatile DWORD globalT = 0;
volatile bool isRunning = true;

// Самая первая оригинальная формула Bytebeat
inline BYTE GenerateBytebeat(DWORD t) {
    return static_cast<BYTE>((((t * (t >> 8 | t >> 9) & 46 & t >> 8)) ^ (t & t >> 13 | t >> 6)) & 0xFF);
}

// -------------------------------------------------------------
// ПОТОК ЗВУКА: Классическая WinAPI-потоковая функция (DWORD WINAPI)
// -------------------------------------------------------------
DWORD WINAPI AudioThreadFunc(LPVOID lpParam) {
    HWAVEOUT hWaveOut = (HWAVEOUT)lpParam;
    
    // Структуры аудио-буферов вынесены в кучу для предотвращения проблем с памятью потока
    char* audioBuf1 = new char[BUFFER_SIZE];
    char* audioBuf2 = new char[BUFFER_SIZE];

    WAVEHDR waveHdr1 = { audioBuf1, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };
    WAVEHDR waveHdr2 = { audioBuf2, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };

    waveOutPrepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutPrepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    // Первичное наполнение
    for (int i = 0; i < BUFFER_SIZE; ++i) audioBuf1[i] = GenerateBytebeat(InterlockedIncrement(&globalT));
    for (int i = 0; i < BUFFER_SIZE; ++i) audioBuf2[i] = GenerateBytebeat(InterlockedIncrement(&globalT));

    waveOutWrite(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    WAVEHDR* currentHeader = &waveHdr1;

    while (isRunning) {
        // Ожидание освобождения буфера звуковой картой
        while (!(currentHeader->dwFlags & WHDR_DONE) && isRunning) {
            Sleep(1); 
        }

        if (!isRunning) break;

        // Генерация аудио
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            DWORD t = InterlockedIncrement(&globalT); // Безопасное инкрементирование в WinAPI
            currentHeader->lpData[i] = GenerateBytebeat(t);
        }

        waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));
        currentHeader = (currentHeader == &waveHdr1) ? &waveHdr2 : &waveHdr1;
    }

    // Чистим буферы внутри потока
    waveOutUnprepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));
    delete[] audioBuf1;
    delete[] audioBuf2;

    return 0;
}

// -------------------------------------------------------------
// ГЛАВНЫЙ ПОТОК: Графика и инициализация
// -------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
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

    // Создаем поток старым методом через CreateThread (без использования библиотек C++11)
    HANDLE hAudioThread = CreateThread(NULL, 0, AudioThreadFunc, hWaveOut, 0, NULL);

    // Главный цикл графических эффектов
    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        DWORD t = globalT; 
        BYTE soundValue = GenerateBytebeat(t);

        if (soundValue > 120) {
            HDC hdcScreen = GetDC(0);

            int w = rand() % 200 + 50;  
            int h = rand() % 200 + 50;  
            int x = rand() % (screenWidth - w);
            int y = rand() % (screenHeight - h);

            COLORREF syncColor = RGB(soundValue, (soundValue ^ (t >> 4)) & 0xFF, (t >> 8) & 0xFF);
            HBRUSH hBrush = CreateSolidBrush(syncColor);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcScreen, hBrush);

            PatBlt(hdcScreen, x, y, w, h, PATINVERT);

            SelectObject(hdcScreen, hOldBrush);
            DeleteObject(hBrush);
            ReleaseDC(0, hdcScreen);
        }

        // Чистый WinAPI Sleep вместо std::this_thread::sleep_for
        Sleep(5); 
    }

    // Корректно тушим звуковой поток
    isRunning = false;
    if (hAudioThread != NULL) {
        WaitForSingleObject(hAudioThread, INFINITE);
        CloseHandle(hAudioThread);
    }

    waveOutReset(hWaveOut);
    waveOutClose(hWaveOut);

    return 0;
}
