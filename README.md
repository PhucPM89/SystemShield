# SystemShield

Ứng dụng bảo vệ màn hình khóa hệ thống trên Windows, được viết bằng C++ Win32 API.

## Yêu cầu

- **Visual Studio 2022** (có cài workload **Desktop development with C++**)
- **Windows 10/11**

## Cách build

### Cách 1: Mở bằng Visual Studio (đơn giản nhất)

1. Mở file `SystemShield.sln` bằng Visual Studio 2022
2. Chọn cấu hình **Release | x64**
3. Nhấn **Ctrl+B** hoặc vào menu **Build → Build Solution**
4. File `.exe` sẽ nằm trong thư mục `x64\Release\`

### Cách 2: Build bằng dòng lệnh

Mở **Developer PowerShell for VS 2022** (tìm trong Start Menu), sau đó chạy:

```powershell
msbuild SystemShield.sln /p:Configuration=Release /p:Platform=x64
```

> **Lưu ý:** Không dùng `dotnet run` vì đây là dự án C++ (không phải .NET).

## Cấu trúc dự án

```
SystemShield/
├── SystemShield.sln              # Solution file
├── SystemShield/
│   ├── SystemShield.cpp          # Mã nguồn chính
│   ├── SystemShield.vcxproj      # Project file
│   ├── Uploader.ps1              # Script cảnh báo Discord
│   └── pdf.ico                   # Icon ứng dụng
├── .gitignore
└── README.md
```
