#include <windows.h>
#include <mmsystem.h>
#include <ctime>
#include <thread>
#include <atomic>

// Подключаем библиотеку для работы со звуком
#pragma comment(lib, "winmm.lib")

// Настройки аудио (классический Bytebeat)
const int SAMPLE_RATE = 8000; 
const int BUFFER_SIZE = 4000; 

// Атомарная переменная времени, общая для звука и графики
std::atomic<DWORD> globalT(0);
std::atomic<bool> isRunning(true);

// ТА САМАЯ ПЕРВАЯ ФОРМУЛА: Ритмичный chiptune-трек
inline BYTE GenerateBytebeat(DWORD t) {
    return static_cast<BYTE>((((t * (t >> 8 | t >> 9) & 46 & t >> 8)) ^ (t & t >> 13 | t >> 6)) & 0xFF);
}

// -------------------------------------------------------------
// ПОТОК ЗВУКА: Работает в фоне, плавно гонит аудио в карту
// -------------------------------------------------------------
void AudioThreadFunc(HWAVEOUT hWaveOut, WAVEHDR* waveHdr1, WAVEHDR* waveHdr2) {
    WAVEHDR* currentHeader = waveHdr1;

    while (isRunning) {
        // Ожидаем, пока текущий буфер отыграет
        while (!(currentHeader->dwFlags & WHDR_DONE) && isRunning) {
            Sleep(1); 
        }

        if (!isRunning) break;

        // Заполняем буфер чистым звуком
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            DWORD t = globalT.fetch_add(1); 
            currentHeader->lpData[i] = GenerateBytebeat(t);
        }

        // Отправляем буфер обратно в очередь
        waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));

        // Переключаем буфер
        currentHeader = (currentHeader == waveHdr1) ? waveHdr2 : waveHdr1;
    }
}

// -------------------------------------------------------------
// ГЛАВНЫЙ ПОТОК (ГРАФИКА): Рисует на максимальной скорости
// -------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    srand(static_cast<unsigned int>(time(0)));

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Настройка аудио-формата
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

    // Выделяем звуковые буферы
    char audioBuf1[BUFFER_SIZE] = {0};
    char audioBuf2[BUFFER_SIZE] = {0};

    WAVEHDR waveHdr1 = { audioBuf1, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };
    WAVEHDR waveHdr2 = { audioBuf2, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };

    waveOutPrepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutPrepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    // Инициализируем буферы первым звуком
    for (int i = 0; i < BUFFER_SIZE; ++i) audioBuf1[i] = GenerateBytebeat(globalT.fetch_add(1));
    for (int i = 0; i < BUFFER_SIZE; ++i) audioBuf2[i] = GenerateBytebeat(globalT.fetch_add(1));

    waveOutWrite(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    // Запускаем фоновый поток для звука
    std::thread audioThread(AudioThreadFunc, hWaveOut, &waveHdr1, &waveHdr2);

    // Цикл графических эффектов
    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        DWORD t = globalT.load(); // Синхронизируемся с треком
        BYTE soundValue = GenerateBytebeat(t);

        // Синхронизация: вспышка на пиках звуковой волны
        if (soundValue > 120) {
            HDC hdcScreen = GetDC(0);

            // Исходные случайные размеры прямоугольников (от 50 до 250 пикселей)
            int w = rand() % 200 + 50;  
            int h = rand() % 200 + 50;  
            int x = rand() % (screenWidth - w);
            int y = rand() % (screenHeight - h);

            // Цвет плавно мутирует вместе с ходом времени трека
            COLORREF syncColor = RGB(soundValue, (soundValue ^ (t >> 4)) & 0xFF, (t >> 8) & 0xFF);
            HBRUSH hBrush = CreateSolidBrush(syncColor);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcScreen, hBrush);

            // Фирменный визуальный эффект PATINVERT
            PatBlt(hdcScreen, x, y, w, h, PATINVERT);

            SelectObject(hdcScreen, hOldBrush);
            DeleteObject(hBrush);
            ReleaseDC(0, hdcScreen);
        }

        // Задержка в 5 мс, чтобы Windows не зависал (для максимальной скорости можно поставить 1)
        Sleep(5); 
    }

    // Завершаем потоки и чистим память при выходе (по нажатию ESC)
    isRunning = false;
    if (audioThread.joinable()) {
        audioThread.join();
    }

    waveOutReset(hWaveOut);
    waveOutUnprepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);

    return 0;
}
