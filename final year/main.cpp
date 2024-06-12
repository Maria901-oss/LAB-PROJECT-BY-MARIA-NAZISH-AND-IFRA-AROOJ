#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include <psapi.h>
#include <vector>
#include<iostream>
#pragma comment(lib, "comctl32.lib")

LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);
void UpdateSystemResources(HWND hwndCPUBar, HWND hwndMemBar, HWND hwndDiskBar, HWND hwndCPUText, HWND hwndMemText, HWND hwndDiskText);
void DrawCPUHistory(HDC hdc, RECT rect, const std::vector<int>& cpuUsageHistory);

TCHAR szClassName[] = _T("SystemResourceMonitor");
std::vector<int> cpuUsageHistory(50, 0); // Store last 50 CPU usage values

int WINAPI WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nCmdShow) {
    HWND hwnd;
    MSG messages;
    WNDCLASSEX wincl;

    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowProcedure;
    wincl.style = CS_DBLCLKS;
    wincl.cbSize = sizeof(WNDCLASSEX);

    wincl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;
    wincl.cbClsExtra = 0;
    wincl.cbWndExtra = 0;
    wincl.hbrBackground = CreateSolidBrush(RGB(255, 255, 224)); // Light yellow color

    if (!RegisterClassEx(&wincl)) return 0;

    hwnd = CreateWindowEx(
        0, szClassName, _T("System Resource Monitoring"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 544, 375,
        HWND_DESKTOP, NULL, hThisInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    while (GetMessage(&messages, NULL, 0, 0)) {
        TranslateMessage(&messages);
        DispatchMessage(&messages);
    }

    return messages.wParam;
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hwndCPUBar, hwndMemBar, hwndDiskBar;
    static HWND hwndCPUText, hwndMemText, hwndDiskText;
    static UINT_PTR timerId;

    switch (message) {
    case WM_CREATE:
        InitCommonControls();


        CreateWindow(_T("STATIC"), _T("CPU Usage:"), WS_VISIBLE | WS_CHILD, 20, 20, 200, 20, hwnd, NULL, NULL, NULL);
        hwndCPUBar = CreateWindow(PROGRESS_CLASS, NULL, WS_VISIBLE | WS_CHILD | PBS_SMOOTH, 20, 40, 200, 20, hwnd, NULL, NULL, NULL);
        hwndCPUText = CreateWindow(_T("STATIC"), _T("0%"), WS_VISIBLE | WS_CHILD, 230, 40, 50, 20, hwnd, NULL, NULL, NULL);

        CreateWindow(_T("STATIC"), _T("Memory Usage:"), WS_VISIBLE | WS_CHILD, 20, 70, 200, 20, hwnd, NULL, NULL, NULL);
        hwndMemBar = CreateWindow(PROGRESS_CLASS, NULL, WS_VISIBLE | WS_CHILD | PBS_SMOOTH, 20, 90, 200, 20, hwnd, NULL, NULL, NULL);
        hwndMemText = CreateWindow(_T("STATIC"), _T("0%"), WS_VISIBLE | WS_CHILD, 230, 90, 50, 20, hwnd, NULL, NULL, NULL);

        CreateWindow(_T("STATIC"), _T("Disk Usage:"), WS_VISIBLE | WS_CHILD, 20, 120, 200, 20, hwnd, NULL, NULL, NULL);
        hwndDiskBar = CreateWindow(PROGRESS_CLASS, NULL, WS_VISIBLE | WS_CHILD | PBS_SMOOTH, 20, 140, 200, 20, hwnd, NULL, NULL, NULL);
        hwndDiskText = CreateWindow(_T("STATIC"), _T("0%"), WS_VISIBLE | WS_CHILD, 230, 140, 50, 20, hwnd, NULL, NULL, NULL);

        SendMessage(hwndCPUBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessage(hwndMemBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessage(hwndDiskBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

        timerId = SetTimer(hwnd, 1, 1000, NULL); // Update every second
        break;

    case WM_TIMER:
        UpdateSystemResources(hwndCPUBar, hwndMemBar, hwndDiskBar, hwndCPUText, hwndMemText, hwndDiskText);
                InvalidateRect(hwnd, NULL, TRUE); // Redraw the entire window
        break;

    case WM_DESTROY:
        KillTimer(hwnd, timerId);
        PostQuitMessage(0);
        break;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkColor(hdcStatic, RGB(255, 255, 224));
        return (INT_PTR)CreateSolidBrush(RGB(255, 255, 224));
    }

    case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT clientRect;
    GetClientRect(hwnd, &clientRect); // Get the client area of the main window

    // Calculate the position to draw the CPU history graph closer to the top
    int graphHeight = clientRect.bottom / 4; // Divide the client area into four parts and use one part for the graph
    RECT graphRect = { clientRect.left, clientRect.bottom - graphHeight - 100, clientRect.right, clientRect.bottom - 50 };

    // Adjust the length of the graph
    graphRect.left += 50;
    graphRect.right -= 50;

    // Draw "CPU History Usage Graph" text
    RECT textRect = { clientRect.left, graphRect.bottom + 10, clientRect.right, graphRect.bottom + 30 };
    //DrawText(hdc, _T("CPU History Usage Graph"), -1, &textRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // Draw the CPU history graph
    DrawCPUHistory(hdc, graphRect, cpuUsageHistory);

    EndPaint(hwnd, &ps);
    break;
}


    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

void UpdateSystemResources(HWND hwndCPUBar, HWND hwndMemBar, HWND hwndDiskBar, HWND hwndCPUText, HWND hwndMemText, HWND hwndDiskText) {

    // Get CPU usage
    static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
    static int numProcessors = 0;
    static HANDLE self = NULL;

    if (numProcessors == 0) {
        SYSTEM_INFO sysInfo;
        FILETIME ftime, fsys, fuser;

        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;

        GetSystemTimes(&ftime, &fsys, &fuser);
        memcpy(&lastCPU, &ftime, sizeof(FILETIME));
        memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
        memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));

        self = GetCurrentProcess();
    }

    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    double percent;

    GetSystemTimes(&ftime, &fsys, &fuser);
    memcpy(&now, &ftime, sizeof(FILETIME));
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));

    percent = (double)(((sys.QuadPart - lastSysCPU.QuadPart) + (user.QuadPart - lastUserCPU.QuadPart)) * 100.0 / (now.QuadPart - lastCPU.QuadPart)) / numProcessors;

    lastCPU = now;
    lastUserCPU = user;
    lastSysCPU = sys;

    SendMessage(hwndCPUBar, PBM_SETPOS, (int)percent, 0);

    TCHAR cpuUsageText[50];
    _stprintf_s(cpuUsageText, _T("%d%%"), (int)percent);
    SetWindowText(hwndCPUText, cpuUsageText);

// Store CPU usage in history
    cpuUsageHistory.erase(cpuUsageHistory.begin());
    cpuUsageHistory.push_back((int)percent);
    // Get RAM usage
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    double ramUsage = 100.0 - ((memInfo.ullAvailPhys * 100.0) / memInfo.ullTotalPhys);

    SendMessage(hwndMemBar, PBM_SETPOS, (int)ramUsage, 0);

    TCHAR memUsageText[50];
    _stprintf_s(memUsageText, _T("%d%%"), (int)ramUsage);
    SetWindowText(hwndMemText, memUsageText);

    // Get Disk usage
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceEx(NULL, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        double diskUsage = ((totalNumberOfBytes.QuadPart - totalNumberOfFreeBytes.QuadPart) * 100.0) / totalNumberOfBytes.QuadPart;

        SendMessage(hwndDiskBar, PBM_SETPOS, (int)diskUsage, 0);

        TCHAR diskUsageText[50];
        _stprintf_s(diskUsageText, _T("%d%%"), (int)diskUsage);
        SetWindowText(hwndDiskText, diskUsageText);
    }
}
void DrawCPUHistory(HDC hdc, RECT rect, const std::vector<int>& cpuHistory) {
    // Draw the CPU history graph with a unique color
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 224));
    FillRect(hdc, &rect, hBrush);
    DeleteObject(hBrush);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int step = width / (cpuHistory.size() - 1);

    // Calculate the position for the graph line to be slightly lower
    int graphLineY = rect.bottom - (height / 4); // 1/4th from the bottom

    // Draw the heading above the graph line
    RECT headingRect = { rect.left, rect.top, rect.right, graphLineY - 20 }; // Adjust -20 for spacing
    DrawText(hdc, _T("CPU History Usage Graph"), -1, &headingRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Use green color for the graph
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    MoveToEx(hdc, rect.left, graphLineY - (cpuHistory[0] * height / 100), NULL);
    for (size_t i = 1; i < cpuHistory.size(); ++i) {
        LineTo(hdc, rect.left + step * i, graphLineY - (cpuHistory[i] * height / 100));
    }

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}
