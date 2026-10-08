#include <windows.h>
#include <mmsystem.h>
#include <ctime>

// Подключаем библиотеку для работы со звуком
#pragma comment(lib, "winmm.lib")

// Настройки аудио (классический Bytebeat)
const int SAMPLE_RATE = 8000; 
const int BUFFER_SIZE = 2000; // ~0.25 секунды на буфер для отзывчивого синхрона

// Функция генерации Bytebeat (возвращает значение от 0 до 255)
inline BYTE GenerateBytebeat(DWORD t) {
    // Легендарная формула, задающая ритм и мелодию
    return static_cast<BYTE>((t * (t >> 8 | t >> 9) & 46 & t >> 8) ^ (t & t >> 13 | t >> 6));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    srand(static_cast<unsigned int>(time(0)));

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // 1. Настройка аудио-формата
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.nAvgBytesPerSec = SAMPLE_RATE;
    wfx.nBlockAlign = 1;
    wfx.wBitsPerSample = 8;
    wfx.cbSize = 0;

    HWAVEOUT hWaveOut = nullptr;
    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return 1;
    }

    // 2. Выделяем два звуковых буфера для непрерывного двойного буферизованного вывода
    char audioBuf1[BUFFER_SIZE];
    char audioBuf2[BUFFER_SIZE];

    WAVEHDR waveHdr1 = { audioBuf1, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };
    WAVEHDR waveHdr2 = { audioBuf2, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };

    waveOutPrepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutPrepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    DWORD t = 0; // Глобальный счетчик времени для Bytebeat
    WAVEHDR* currentHeader = &waveHdr1;

    // Главный цикл (работает, пока не нажмут ESC для безопасного выхода)
    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        
        // Заполняем текущий аудио-буфер звуком и одновременно рисуем графику
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            BYTE soundValue = GenerateBytebeat(t);
            currentHeader->lpData[i] = soundValue;

            // СИНХРОНИЗАЦИЯ: Визуальный эффект срабатывает в такт сильным долям или пикам звука
            // soundValue > 128 означает верхнюю половину амплитуды волны (акцент в звуке)
            // Дополнительное деление (t % 64 == 0) защищает процессор от перегрузки GDI
            if (soundValue > 128 && (t % 64 == 0)) {
                HDC hdcScreen = GetDC(0);

                // Динамика: размеры зависят от текущего значения звука
                int w = (soundValue % 200) + 50;
                int h = ((soundValue >> 2) % 200) + 50;
                int x = rand() % (screenWidth - w);
                int y = rand() % (screenHeight - h);

                // Цвет: формируется на основе математики Bytebeat
                COLORREF syncColor = RGB(
                    soundValue, 
                    (soundValue ^ (t >> 4)) & 0xFF, 
                    (t >> 8) & 0xFF
                );
                
                HBRUSH hBrush = CreateSolidBrush(syncColor);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcScreen, hBrush);

                // Оставляем исходный эффект инверсии (PATINVERT)
                PatBlt(hdcScreen, x, y, w, h, PATINVERT);

                SelectObject(hdcScreen, hOldBrush);
                DeleteObject(hBrush);
                ReleaseDC(0, hdcScreen);
            }

            t++;
        }

        // Отправляем заполненный буфер на воспроизведение
        waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));

        // Переключаемся на второй буфер
        currentHeader = (currentHeader == &waveHdr1) ? &waveHdr2 : &waveHdr1;

        // Ждем, пока устройство освободит следующий буфер, чтобы не забить память
        while (!(currentHeader->dwFlags & WHDR_DONE)) {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) break;
            Sleep(1); // Легкий отдых для CPU
        }
        
        // Сбрасываем флаг готовности буфера перед новым циклом
        currentHeader->dwFlags &= ~WHDR_DONE;
    }

    // Очистка памяти и остановка звука при выходе
    waveOutReset(hWaveOut);
    waveOutUnprepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);

    return 0;
}
