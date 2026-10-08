#include <windows.h>
#include <mmsystem.h>
#include <ctime>

// Подключаем библиотеку для работы со звуком
#pragma comment(lib, "winmm.lib")

// Настройки аудио (классический Bytebeat)
const int SAMPLE_RATE = 8000; 
const int BUFFER_SIZE = 4000; // Оптимальный размер буфера для стабильного потока (~0.5 сек)

// Функция генерации Bytebeat (возвращает значение от 0 до 255)
inline BYTE GenerateBytebeat(DWORD t) {
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

    // 2. Выделяем два звуковых буфера
    char audioBuf1[BUFFER_SIZE] = {0};
    char audioBuf2[BUFFER_SIZE] = {0};

    WAVEHDR waveHdr1 = { audioBuf1, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };
    WAVEHDR waveHdr2 = { audioBuf2, BUFFER_SIZE, 0, 0, 0, 0, nullptr, 0 };

    waveOutPrepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutPrepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    DWORD t = 0; // Глобальный счетчик времени для Bytebeat

    // Функция заполнения буфера звуком и синхронной графикой
    auto FillBufferAndRender = [&](WAVEHDR* header) {
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            BYTE soundValue = GenerateBytebeat(t);
            header->lpData[i] = soundValue;

            // СИНХРОНИЗАЦИЯ: Визуальный эффект в такт звуку
            if (soundValue > 128 && (t % 64 == 0)) {
                HDC hdcScreen = GetDC(0);

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
                ReleaseDC(0, hdcScreen);
            }
            t++;
        }
    };

    // 3. Первичное заполнение и запуск обоих буферов в очередь звуковой карты
    FillBufferAndRender(&waveHdr1);
    waveOutWrite(hWaveOut, &waveHdr1, sizeof(WAVEHDR));

    FillBufferAndRender(&waveHdr2);
    waveOutWrite(hWaveOut, &waveHdr2, sizeof(WAVEHDR));

    WAVEHDR* currentHeader = &waveHdr1;

    // Главный бесконечный цикл (выход по ESC)
    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        
        // Ожидаем, пока текущий буфер полностью отыграет (Windows выставит флаг WHDR_DONE)
        while (!(currentHeader->dwFlags & WHDR_DONE)) {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) break;
            Sleep(10); // Разгружаем процессор во время ожидания
        }

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) break;

        // Как только буфер освободился — генерируем в него следующую порцию звука/видео
        FillBufferAndRender(currentHeader);

        // Отправляем буфер обратно в очередь воспроизведения
        waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));

        // Переключаем указатель на противоположный буфер
        currentHeader = (currentHeader == &waveHdr1) ? &waveHdr2 : &waveHdr1;
    }

    // 4. Очистка ресурсов при выходе
    waveOutReset(hWaveOut);
    waveOutUnprepareHeader(hWaveOut, &waveHdr1, sizeof(WAVEHDR));
    waveOutUnprepareHeader(hWaveOut, &waveHdr2, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);

    return 0;
}
