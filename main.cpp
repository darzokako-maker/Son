#include <iostream>
#include <vector>
#include <windows.h>
#include "windivert.h"

#pragma comment(lib, "WinDivert.lib")

struct PacketData {
    WINDIVERT_ADDRESS addr;
    std::vector<unsigned char> data;
};

int main() {
    // FİLTRE: Sadece giden, 443 portu olan ve SonOyuncu'nun kullandığı IP aralığındaki paketler.
    // Bu sayede Discord veya tarayıcın etkilenmez.
    const char* filter = "outbound && tcp.DstPort == 443 && (ip.DstAddr >= 104.16.0.0 && ip.DstAddr <= 104.31.255.255)";
    
    HANDLE handle = WinDivertOpen(filter, WINDIVERT_LAYER_NETWORK, 0, 0);
    
    if (handle == INVALID_HANDLE_VALUE) {
        std::cerr << "[-] WinDivert baslatilamadi! Yonetici yetkisi kontrol edin." << std::endl;
        return 1;
    }

    std::cout << "--- SONOYUNCU OZEL BLINK ---" << std::endl;
    std::cout << "[X] Tusuna basili tutun (Sadece oyun etkilenir)." << std::endl;

    unsigned char packet[0xFFFF];
    UINT packetLen;
    WINDIVERT_ADDRESS addr;
    std::vector<PacketData> queue;
    bool isBlinking = false;

    while (TRUE) {
        // X tuşu (Virtual Key: 0x58)
        if (GetAsyncKeyState(0x58) & 0x8000) {
            if (!isBlinking) {
                isBlinking = true;
                std::cout << "[!] SONOYUNCU BLINK: AKTIF" << std::endl;
            }
        } else {
            if (isBlinking) {
                isBlinking = false;
                std::cout << "[*] PAKETLER GONDERILDI: " << queue.size() << std::endl;
                for (auto& p : queue) {
                    WinDivertSend(handle, p.data.data(), p.data.size(), NULL, &p.addr);
                }
                queue.clear();
            }
        }

        if (WinDivertRecv(handle, packet, sizeof(packet), &packetLen, &addr)) {
            if (isBlinking) {
                PacketData pd;
                pd.addr = addr;
                pd.data.assign(packet, packet + packetLen);
                queue.push_back(pd);
            } else {
                WinDivertSend(handle, packet, packetLen, NULL, &addr);
            }
        }
    }
    return 0;
}

