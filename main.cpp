#include <windows.h>
#include <vector>
#include <chrono>
#include <thread>
#include "resource.h"
#include "windivert.h"

#pragma comment(lib, "WinDivert.lib")
#pragma comment(lib, "user32.lib")

// Global Kontroller
HANDLE hDivert = INVALID_HANDLE_VALUE;
bool g_BypassActive = true;
bool isBlinking = false;

struct QueuedPacket {
    WINDIVERT_ADDRESS addr;
    std::vector<unsigned char> data;
};

// Dosyaları EXE'den dışarı çıkaran fonksiyon
bool ExtractResource(int resId, const char* outPath) {
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(NULL, hRes);
    DWORD size = SizeofResource(NULL, hRes);
    LPVOID ptr = LockResource(hData);
    HANDLE hFile = CreateFileA(outPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written;
    WriteFile(hFile, ptr, size, &written, NULL);
    CloseHandle(hFile);
    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDM_EXIT) PostQuitMessage(0);
            if (LOWORD(wParam) == IDM_RESET) {
                isBlinking = false;
                MessageBox(hwnd, "Tum paketler temizlendi ve sistem sifirlandi.", "Bypass", MB_OK);
            }
            break;
        case WM_DESTROY: PostQuitMessage(0); break;
        default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    // 1. Dosyaları hazırla
    ExtractResource(IDR_DLL_FILE, "WinDivert.dll");
    ExtractResource(IDR_SYS_FILE, "WinDivert64.sys");

    // 2. Pencere oluştur
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst, 
                      NULL, LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW+1), 
                      MAKEINTRESOURCE(IDM_MAIN_MENU), "UltraBlink", NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindowEx(0, "UltraBlink", "Yahya Stealth Blink v18.2", WS_OVERLAPPEDWINDOW, 
                               CW_USEDEFAULT, CW_USEDEFAULT, 400, 150, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nShow);

    // 3. WinDivert Aç (Port 443 Filtresi)
    hDivert = WinDivertOpen("outbound && tcp.DstPort == 443", WINDIVERT_LAYER_NETWORK, 0, 0);

    MSG msg;
    unsigned char packet[0xFFFF];
    UINT packetLen;
    WINDIVERT_ADDRESS addr;
    std::vector<QueuedPacket> queue;

    while (true) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // X Tuşu Kontrolü (Virtual Key: 0x58)
        if (GetAsyncKeyState(0x58) & 0x8000) {
            isBlinking = true;
        } else if (isBlinking) {
            isBlinking = false;
            // BYPASS SALMA MANTIĞI
            for (auto& p : queue) {
                // Gizli hareket için paketleri milisaniyelik gecikmelerle salıyoruz
                // Bu, sunucunun paketleri "normal lag" gibi görmesini sağlar
                if (g_BypassActive) std::this_thread::sleep_for(std::chrono::milliseconds(3));
                WinDivertSend(hDivert, p.data.data(), p.data.size(), NULL, &p.addr);
            }
            queue.clear();
        }

        // Paket Dinleme
        if (WinDivertRecv(hDivert, packet, sizeof(packet), &packetLen, &addr)) {
            if (isBlinking) {
                // Blink aktifken paketleri biriktir (Gizle)
                queue.push_back({addr, std::vector<unsigned char>(packet, packet + packetLen)});
                // Sunucuya bağlantının ölmediğini bildirmek için çok boş paket (keep-alive) 
                // gönderme mantığı buraya eklenebilir ancak karmaşıktır.
            } else {
                WinDivertSend(hDivert, packet, packetLen, NULL, &addr);
            }
        }
    }

    WinDivertClose(hDivert);
    DeleteFileA("WinDivert.dll");
    DeleteFileA("WinDivert64.sys");
    return 0;
}
