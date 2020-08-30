#include <Windows.h>
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <map>

enum Color
{
    BLACK = RGB(0, 0, 0),
    WHITE = RGB(255, 255, 255),

    RED = RGB(255, 0, 0),
    GREEN = RGB(0, 128, 0),
    BLUE = RGB(0, 0, 255),

    TEAL = RGB(0, 128, 128),
    MAROON = RGB(128, 0, 0),
    NAVY = RGB(0, 0, 128),

    GREY = RGB(128, 128, 128),
    LIGHTGREY = RGB(192, 192, 192),
};

const char* ColorToString(Color color)
{
    switch (color)
    {
    case BLACK:
        return "BLACK";
    case WHITE:
        return "WHITE";
    case RED:
        return "RED";
    case GREEN:
        return "GREEN";
    case BLUE:
        return "BLUE";
    case GREY:
        return "GREY";
    case LIGHTGREY:
        return "LIGHTGREY";
    case TEAL:
        return "TEAL";
    case MAROON:
        return "MAROON";
    }

    return "UNKNOWN";
}

void PrintColorref(COLORREF color)
{
    std::cout << (int)GetRValue(color) << ", " << (int)GetGValue(color) << ", " << (int)GetBValue(color) << std::endl;
}

struct Cell
{
    enum Type
    {
        UNKNOWN,
        EMPTY,
        NUMBER,
        MINE,
        FLAG
    };

    Type type = UNKNOWN;
    int number = 0;
};

struct Coord
{
    int y = 0;
    int x = 0;

    bool operator<(const Coord& coord) const
    {
        if (y == coord.y)
            return x < coord.x;
        return y < coord.y;
    }

    bool operator==(const Coord& coord) const
    {
        return x == coord.x && y == coord.y;
    }
};

typedef std::vector<std::vector<Cell>> Field;
typedef std::vector<Coord> Coords;

class Window
{
private:
    const int OFFSET_LEFT = 12;
    const int OFFSET_RIGHT = 8;
    const int OFFSET_TOP = 55;
    const int OFFSET_BOTTOM = 8;
    const int CELL_SIZE = 16;
    const int CELL_BORDER = 2;

public:
    Window()
    {
        m_hWnd = FindWindow(nullptr, "Minesweeper");

        assert(m_hWnd);
        if (m_hWnd == NULL)
        {
            throw std::exception("'Minesweeper.exe' not found");
        }

        RECT windowRect;
        GetClientRect(m_hWnd, &windowRect);

        assert(windowRect.right != 0 && windowRect.bottom != 0);

        if (windowRect.right == 0 || windowRect.bottom == 0)
        {
            throw std::exception("'Minesweeper.exe' is minimized");
        }

        assert((windowRect.right - OFFSET_LEFT - OFFSET_RIGHT) % CELL_SIZE == 0);
        assert((windowRect.bottom - OFFSET_TOP - OFFSET_BOTTOM) % CELL_SIZE == 0);

        m_nWidth = (windowRect.right - OFFSET_LEFT - OFFSET_RIGHT) / CELL_SIZE;
        m_nHeight = (windowRect.bottom - OFFSET_TOP - OFFSET_BOTTOM) / CELL_SIZE;

        GetField();
    }

    Field GetField() const
    {
        Field field(m_nHeight, std::vector<Cell>(m_nWidth, { Cell::UNKNOWN }));

        RECT rc;
        GetClientRect(m_hWnd, &rc);

        HDC hScreenDC = GetDC(NULL);
        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

        const int cx = GetDeviceCaps(hScreenDC, HORZRES);
        const int cy = GetDeviceCaps(hScreenDC, VERTRES);

        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, rc.right - rc.left, rc.bottom - rc.top);
        SelectObject(hMemoryDC, hBitmap);
        PrintWindow(m_hWnd, hMemoryDC, PW_CLIENTONLY);

        std::vector<std::vector<COLORREF>> pixels = ToPixels(hBitmap);
        assert(pixels.size() >= 0);

        const int height = pixels.size();
        const int width = pixels[0].size();

        assert(height % CELL_SIZE == 0 && height / CELL_SIZE == m_nHeight);
        assert(width % CELL_SIZE == 0 && width / CELL_SIZE == m_nWidth);

        for (int x = 0; x < m_nWidth; ++x)
        {
            for (int y = 0; y < m_nHeight; ++y)
            {
                field[y][x] = ScanCell(pixels, y, x);
            }
        }

        return field;
    }

    void Click(Coord coord, bool rightClick = false) const
    {
        const int x = coord.x;
        const int y = coord.y;

        assert(x >= 0 && x < m_nWidth);
        assert(y >= 0 && y < m_nHeight);
        if (x >= 0 && x < m_nWidth && y >= 0 && y < m_nHeight)
        {
            DWORD mousePosX = OFFSET_LEFT + x * CELL_SIZE + CELL_SIZE / 2;
            DWORD mousePosY = OFFSET_TOP + y * CELL_SIZE + CELL_SIZE / 2;

            if (rightClick)
            {
                SendMessage(m_hWnd, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(mousePosX, mousePosY));
                SendMessage(m_hWnd, WM_RBUTTONUP, MK_RBUTTON, MAKELPARAM(mousePosX, mousePosY));
            }
            else
            {
                SendMessage(m_hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(mousePosX, mousePosY));
                SendMessage(m_hWnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(mousePosX, mousePosY));
            }
        }
    }

private:
    Cell ScanCell(const std::vector<std::vector<COLORREF>>& pixels, const int y, const int x) const
    {
        std::map<COLORREF, int> colorsCount;

        const int offsetX = x * CELL_SIZE;
        const int offsetY = y * CELL_SIZE;

        bool isMineOrNumberOrEmpty = pixels[offsetY][offsetX] != WHITE;

        for (int cellX = CELL_BORDER; cellX < CELL_SIZE - CELL_BORDER; ++cellX)
        {
            for (int cellY = CELL_BORDER; cellY < CELL_SIZE - CELL_BORDER; ++cellY)
            {
                ++colorsCount[pixels[offsetY + cellY][offsetX + cellX]];
            }
        }

        if (colorsCount[BLACK] && colorsCount[RED])
            return { Cell::FLAG };

        if (isMineOrNumberOrEmpty)
        {
            if (colorsCount[BLACK] && colorsCount[WHITE])
                return { Cell::MINE };

            if (colorsCount[BLUE])
                return { Cell::NUMBER, 1 };
            if (colorsCount[GREEN])
                return { Cell::NUMBER, 2 };
            if (colorsCount[RED])
                return { Cell::NUMBER, 3 };
            if (colorsCount[NAVY])
                return { Cell::NUMBER, 4 };
            if (colorsCount[MAROON])
                return { Cell::NUMBER, 5 };
            if (colorsCount[TEAL])
                return { Cell::NUMBER, 6 };
            if (colorsCount[BLACK])
                return { Cell::NUMBER, 7 };
            if (colorsCount[GREY])
                return { Cell::NUMBER, 8 };

            return { Cell::EMPTY };
        }

        return { Cell::UNKNOWN };
    }

    std::vector<std::vector<COLORREF>> ToPixels(HBITMAP hBitmap) const
    {
        BITMAP Bmp = { 0 };
        BITMAPINFO info = { 0 };
        std::vector<unsigned char> data = std::vector<unsigned char>();

        HDC dc = CreateCompatibleDC(NULL);
        std::memset(&info, 0, sizeof(BITMAPINFO));
        HBITMAP oldBitmap = (HBITMAP)SelectObject(dc, hBitmap);
        GetObject(hBitmap, sizeof(Bmp), &Bmp);

        int width = 0;
        int height = 0;

        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width = Bmp.bmWidth;
        info.bmiHeader.biHeight = height = Bmp.bmHeight;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = Bmp.bmBitsPixel;
        info.bmiHeader.biCompression = BI_RGB;
        info.bmiHeader.biSizeImage = width * height * 4;

        data.resize(info.bmiHeader.biSizeImage);
        GetDIBits(dc, hBitmap, 0, height, &data[0], &info, DIB_RGB_COLORS);
        SelectObject(dc, oldBitmap);

        height = std::abs(height);
        DeleteDC(dc);

        std::vector<std::vector<COLORREF>> pixels(height - OFFSET_TOP - OFFSET_BOTTOM,
                                                  std::vector<COLORREF>(width - OFFSET_RIGHT - OFFSET_LEFT));

        for (int y = OFFSET_BOTTOM; y < height - OFFSET_TOP; ++y)
        {
            for (int x = OFFSET_LEFT; x < width - OFFSET_RIGHT; ++x)
            {
                BYTE* pColor = (BYTE*)&data[(y * width + x) * sizeof(COLORREF)];
                pixels[y - OFFSET_BOTTOM][x - OFFSET_LEFT] = RGB(pColor[2], pColor[1], pColor[0]);
            }
        }

        std::reverse(pixels.begin(), pixels.end());

        return pixels;
    }

private:
    HWND m_hWnd = 0;
    int m_nWidth = 0;
    int m_nHeight = 0;
};

class Game
{
public:
    void SetField(const Field& field)
    {
        m_field = field;
    }

    void UpdateMines(const Coords& mines)
    {
        for (const Coord& coord : mines)
        {
            m_field[coord.y][coord.x] = { Cell::FLAG };
        }
    }

    void GetCornerMines(Coords& mines) const
    {
        for (int y = 0; y < m_field.size(); ++y)
        {
            for (int x = 0; x < m_field[y].size(); ++x)
            {
                if (m_field[y][x].type == Cell::NUMBER)
                {
                    Coords unknowns = GetCountOfType({ y, x }, Cell::UNKNOWN);
                    Coords flags = GetCountOfType({ y, x }, Cell::FLAG);
                    if (unknowns.size() + flags.size() == m_field[y][x].number)
                    {
                        mines.insert(mines.end(), unknowns.begin(), unknowns.end());
                    }
                }
            }
        }

        RemoveDuplicates(mines);
    }

    void GetFreeMoves(Coords& moves)
    {
        for (int y = 0; y < m_field.size(); ++y)
        {
            for (int x = 0; x < m_field[y].size(); ++x)
            {
                if (m_field[y][x].type == Cell::NUMBER)
                {
                    Coords unknowns = GetCountOfType({ y, x }, Cell::UNKNOWN);
                    Coords flags = GetCountOfType({ y, x }, Cell::FLAG);
                    if (flags.size() == m_field[y][x].number)
                    {
                        moves.insert(moves.end(), unknowns.begin(), unknowns.end());
                    }
                }
            }
        }

        RemoveDuplicates(moves);
    }

    bool IsOver() const
    {
        if (m_field.empty())
            return false;

        int unknownCount = 0;

        for (const auto& row : m_field)
        {
            for (const auto& cell : row)
            {
                if (cell.type == Cell::UNKNOWN)
                    ++unknownCount;
                if (cell.type == Cell::MINE)
                    return true;
            }
        }

        return unknownCount == 0;
    }

    Coord RandomMove() const
    {
        const int height = m_field.size();
        const int width = m_field[0].size();

        Coord move;
        for (int attempt = 0; attempt < height * width; ++attempt)
        {
            move.y = rand() % height;
            move.x = rand() % width;

            if (m_field[move.y][move.x].type == Cell::UNKNOWN)
                break;
        }

        return move;
    }

private:
    Coords GetCountOfType(const Coord& coord, Cell::Type type) const
    {
        Coords unknowns;

        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dy == 0 && dx == 0)
                    continue;

                Coord shifted = { coord.y + dy, coord.x + dx };

                if (IsCorrect(shifted) && m_field[shifted.y][shifted.x].type == type)
                {
                    unknowns.push_back(shifted);
                }
            }
        }

        return unknowns;
    }

    bool IsCorrect(const Coord& coord) const
    {
        return 0 <= coord.y && coord.y < m_field.size() && 
               0 <= coord.x && coord.x < m_field[0].size();
    }

    void RemoveDuplicates(Coords& coords) const
    {
        std::sort(coords.begin(), coords.end());
        coords.erase(std::unique(coords.begin(), coords.end()), coords.end());
    }

private:
    Field m_field;
};

void PrintField(const Field& field)
{
    for (const auto& row : field)
    {
        for (const auto& cell : row)
        {
            switch (cell.type)
            {
            case Cell::UNKNOWN:
                std::cout << " ";
                break;
            case Cell::EMPTY:
                std::cout << "0";
                break;
            case Cell::NUMBER:
                std::cout << cell.number;
                break;
            case Cell::MINE:
                std::cout << "*";
                break;
            case Cell::FLAG:
                std::cout << "F";
                break;
            }
        }
        std::cout << std::endl;
    }
}

BOOL ShowConsoleCursor(BOOL bShow)
{
    CONSOLE_CURSOR_INFO cci;
    HANDLE hStdOut;
    hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE)
        return FALSE;
    if (!GetConsoleCursorInfo(hStdOut, &cci))
        return FALSE;
    cci.bVisible = bShow;
    if (!SetConsoleCursorInfo(hStdOut, &cci))
        return FALSE;
    return TRUE;
}

void MoveConsoleCursor(COORD position = { 0, 0 })
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleCursorPosition(hConsole, position);
}


int main()
{
    ShowConsoleCursor(FALSE);

    while (true)
    {
        const auto delay = std::chrono::milliseconds(0);

        Window window;
        Game game;

        while (!game.IsOver())
        {
            Field field = window.GetField();
            game.SetField(field);

            MoveConsoleCursor();
            PrintField(field);

            Coords mines;
            game.GetCornerMines(mines);
            game.UpdateMines(mines);

            Coords moves;
            game.GetFreeMoves(moves);

            if (moves.empty() && mines.empty())
                moves.push_back(game.RandomMove());

            for (const auto& move : moves)
                window.Click(move, false);

            for (const auto& mine : mines)
                window.Click(mine, true);

            std::this_thread::sleep_for(delay);
        }

        std::cout << "Game is over. Restart your game" << std::endl;
    }

    return 0;
}