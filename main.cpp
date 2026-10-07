#include <windows.h>
#include <ctime>

// Правильная графическая точка входа для Windows-приложений
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Инициализируем генератор случайных чисел
    srand(static_cast<unsigned int>(time(0)));

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Бесконечный цикл визуального эффекта
    while (true) {
        HDC hdcScreen = GetDC(0);

        // 1. Генерируем случайные координаты и размеры окна фильтра
        int w = rand() % 200 + 50;  // Ширина от 50 до 250 пикселей
        int h = rand() % 200 + 50;  // Высота от 50 до 250 пикселей
        int x = rand() % (screenWidth - w);
        int y = rand() % (screenHeight - h);

        // 2. Генерируем случайный цвет для фильтра
        COLORREF randomColor = RGB(rand() % 255, rand() % 255, rand() % 255);
        HBRUSH hBrush = CreateSolidBrush(randomColor);
        
        // Выбираем кисть в контекст экрана
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcScreen, hBrush);

        // 3. Применяем инверсию цвета (XOR) в выбранной области
        PatBlt(hdcScreen, x, y, w, h, PATINVERT);

        // Освобождаем ресурсы, чтобы избежать утечек памяти
        SelectObject(hdcScreen, hOldBrush);
        DeleteObject(hBrush);
        ReleaseDC(0, hdcScreen);

        // Задержка между эффектами (в миллисекундах)
        Sleep(30); 
    }

    return 0;
}
