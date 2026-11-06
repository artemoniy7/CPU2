#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <filesystem>
#include <string>
#include <vector>
#include <sstream>
#include "manager/MachineCatalog.h"

namespace {
using cpu16::manager::MachineCatalog;
using cpu16::manager::MachineRecord;
constexpr COLORREF Bg = RGB(17, 21, 29);
constexpr COLORREF Sidebar = RGB(22, 28, 38);
constexpr COLORREF Panel = RGB(29, 37, 49);
constexpr COLORREF Accent = RGB(76, 151, 232);
constexpr COLORREF Text = RGB(229, 235, 243);
constexpr COLORREF Muted = RGB(143, 157, 176);
enum class Page { Overview, Machines, Storage, Network, Logs };
Page currentPage = Page::Overview;
HWND mainWindow = nullptr;
std::vector<MachineRecord> machines;
std::filesystem::path dataRoot;

void text(HDC dc, int x, int y, const wchar_t* value, COLORREF color = Text, int size = 16, bool bold = false) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HFONT font = CreateFontW(size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    auto old = SelectObject(dc, font);
    TextOutW(dc, x, y, value, static_cast<int>(wcslen(value)));
    SelectObject(dc, old);
    DeleteObject(font);
}
void panel(HDC dc, RECT r, COLORREF color = Panel) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    auto oldBrush = SelectObject(dc, brush);
    auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, 10, 10);
    SelectObject(dc, oldBrush); SelectObject(dc, oldPen);
    DeleteObject(brush); DeleteObject(pen);
}
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
    out.resize(static_cast<size_t>(n - 1));
    return out;
}
void refresh() {
    MachineCatalog catalog(dataRoot);
    std::string error;
    if (catalog.initialize(&error)) machines = catalog.list(&error);
    if (!error.empty()) machines.clear();
    InvalidateRect(mainWindow, nullptr, TRUE);
}
void drawPage(HDC dc, RECT client) {
    FillRect(dc, &client, CreateSolidBrush(Bg));
    RECT side{0, 0, 224, client.bottom};
    FillRect(dc, &side, CreateSolidBrush(Sidebar));
    text(dc, 26, 25, L"CPU16", Accent, 27, true);
    text(dc, 26, 58, L"SERVER MANAGER", Muted, 11, true);
    const wchar_t* nav[] = {L"Overview", L"Virtual machines", L"Storage", L"Networks", L"Event log"};
    const Page pages[] = {Page::Overview, Page::Machines, Page::Storage, Page::Network, Page::Logs};
    for (int i = 0; i < 5; ++i) {
        RECT r{14, 104 + i * 48, 210, 144 + i * 48};
        if (currentPage == pages[i]) panel(dc, r, RGB(39, 57, 78));
        text(dc, 31, 115 + i * 48, nav[i], currentPage == pages[i] ? Text : Muted, 15, currentPage == pages[i]);
    }
    text(dc, 26, client.bottom - 56, L"HOST CONTROL PLANE", Muted, 10, true);
    text(dc, 26, client.bottom - 34, L"CPU16 Server 0.2", Text, 12);

    int left = 254;
    text(dc, left, 30, currentPage == Page::Overview ? L"Server overview" :
        currentPage == Page::Machines ? L"Virtual machines" :
        currentPage == Page::Storage ? L"Storage pool" :
        currentPage == Page::Network ? L"Virtual networks" : L"Event log", Text, 25, true);
    text(dc, left, 68, L"Manage virtual hardware, machine capacity and host resources", Muted, 13);
    RECT refreshButton{client.right - 154, 29, client.right - 28, 68};
    panel(dc, refreshButton, RGB(43, 62, 83));
    text(dc, refreshButton.left + 25, refreshButton.top + 11, L"Refresh", Text, 14, true);

    if (currentPage == Page::Overview || currentPage == Page::Machines) {
        const int cardW = (client.right - left - 60) / 3;
        const wchar_t* labels[] = {L"VIRTUAL MACHINES", L"ALLOCATED MEMORY", L"VIRTUAL DISKS"};
        std::wstring values[] = {std::to_wstring(machines.size()), L"—", std::to_wstring(machines.size())};
        for (int i = 0; i < 3; ++i) {
            int x = left + i * (cardW + 14);
            panel(dc, RECT{x, 112, x + cardW, 211});
            text(dc, x + 18, 130, labels[i], Muted, 11, true);
            text(dc, x + 18, 159, values[i].c_str(), Text, 28, true);
        }
        text(dc, left, 244, L"PROVISIONED MACHINES", Muted, 11, true);
        panel(dc, RECT{left, 270, client.right - 28, client.bottom - 72});
        text(dc, left + 20, 287, L"NAME", Muted, 10, true);
        text(dc, left + 300, 287, L"CPU MODEL", Muted, 10, true);
        text(dc, left + 510, 287, L"MEMORY", Muted, 10, true);
        text(dc, left + 660, 287, L"VIRTUAL DISK", Muted, 10, true);
        text(dc, left + 850, 287, L"POWER", Muted, 10, true);
        if (machines.empty()) {
            text(dc, left + 20, 335, L"No virtual machines have been provisioned yet.", Muted, 14);
            text(dc, left + 20, 365, L"Create one with: cpu16-server create <id> <name> [ram-MiB] [disk-MiB]", Muted, 12);
        } else {
            int y = 324;
            for (const auto& m : machines) {
                auto name = widen(m.config.name + "  [" + m.id + "]");
                auto cpu = widen(m.config.cpuModel);
                auto ram = std::to_wstring(m.config.ramSizeBytes / (1024 * 1024)) + L" MiB";
                auto disk = std::to_wstring(m.diskSizeBytes / (1024 * 1024)) + L" MiB";
                text(dc, left + 20, y, name.c_str(), Text, 13, true);
                text(dc, left + 300, y, cpu.c_str(), Muted, 13);
                text(dc, left + 510, y, ram.c_str(), Muted, 13);
                text(dc, left + 660, y, disk.c_str(), Muted, 13);
                text(dc, left + 850, y, L"STOPPED", RGB(230, 180, 92), 12, true);
                y += 42;
                if (y > client.bottom - 100) break;
            }
        }
    } else {
        panel(dc, RECT{left, 112, client.right - 28, client.bottom - 72});
        const wchar_t* title = currentPage == Page::Storage ? L"Persistent virtual disk images" :
            currentPage == Page::Network ? L"Virtual network fabric" : L"Host and machine events";
        text(dc, left + 24, 138, title, Text, 18, true);
        const wchar_t* detail = currentPage == Page::Storage ?
            L"Disk images are stored as opaque .cpu16disk files in the host data directory." :
            currentPage == Page::Network ? L"In-process virtual switching is available in the server core. Physical LAN transport is not connected yet." :
            L"Runtime event streaming will appear here when VM lifecycle and logging are integrated.";
        text(dc, left + 24, 180, detail, Muted, 13);
        if (currentPage == Page::Storage) {
            int y = 230;
            for (const auto& m : machines) {
                auto line = widen(m.id + ".cpu16disk  |  " + std::to_string(m.diskSizeBytes / (1024 * 1024)) + " MiB");
                text(dc, left + 24, y, line.c_str(), Text, 13);
                y += 30;
            }
        }
    }
}
LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps);
        RECT r{}; GetClientRect(hwnd, &r); drawPage(dc, r);
        EndPaint(hwnd, &ps); return 0;
    }
    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (x < 224 && y >= 104 && y < 344) {
            int index = (y - 104) / 48;
            currentPage = static_cast<Page>(index);
            InvalidateRect(hwnd, nullptr, TRUE);
        } else if (x > 1000 && y < 82) refresh();
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    dataRoot = std::filesystem::current_path() / L"cpu16-server-data";
    MachineCatalog catalog(dataRoot);
    std::string error;
    catalog.initialize(&error);
    machines = catalog.list(&error);

    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc; wc.hInstance = instance;
    wc.lpszClassName = L"CPU16ServerManagerWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(Bg);
    RegisterClassW(&wc);
    mainWindow = CreateWindowExW(0, wc.lpszClassName, L"CPU16 Server Manager",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1240, 790,
        nullptr, nullptr, instance, nullptr);
    if (!mainWindow) return 1;
    ShowWindow(mainWindow, show);
    UpdateWindow(mainWindow);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
#else
int main() { return 0; }
#endif
