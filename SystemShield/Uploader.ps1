param(
    [string]$AttemptedKey = ""
)

$e = "aHR0cHM6Ly9kaXNjb3JkLmNvbS9hcGkvd2ViaG9va3MvMTU1MTU0NTY3NDU2OTYxMzMyMy9LVXpDT3c0UFIzU180TXZMSEtKVnd4aDBWX2p2aFZSVUcwMG1KdzZCSUpHcGI1VnVSdHJpNk5wQ2ZuU1ZZOFJsVUJONg=="
$discordWebhookUrl = [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($e))

$filePath = $null
$captureFile = Join-Path $env:TEMP "intruder_capture.jpg"

$filePath = $null
$captureFile = Join-Path $env:TEMP "intruder_capture.jpg"
if (Test-Path $captureFile) { Remove-Item $captureFile -Force -ErrorAction SilentlyContinue }

# 1. Thu thập hình ảnh từ Webcam bằng ffmpeg (DirectShow)
$ffmpegCmd = $null
if (Get-Command ffmpeg -ErrorAction SilentlyContinue) {
    $ffmpegCmd = "ffmpeg"
} else {
    $commonFfmpeg = @(
        "C:\ProgramData\chocolatey\bin\ffmpeg.exe",
        "C:\ffmpeg\bin\ffmpeg.exe",
        "C:\tools\ffmpeg\bin\ffmpeg.exe",
        "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Gyan.FFmpeg*\ffmpeg-*\bin\ffmpeg.exe"
    )
    foreach ($p in $commonFfmpeg) {
        $found = Get-Item $p -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $ffmpegCmd = $found.FullName; break }
    }
}

if ($ffmpegCmd) {
    try {
        $devList = & $ffmpegCmd -list_devices true -f dshow -i dummy 2>&1 | Out-String
        $cameras = @()
        foreach ($line in ($devList -split "`r?`n")) {
            if ($line -match '"([^"]+)"\s+\(video\)') {
                $cameras += $Matches[1]
            }
        }
        
        $chosenCam = $cameras | Where-Object { $_ -notmatch 'Virtual|OBS' } | Select-Object -First 1
        if (-not $chosenCam) { $chosenCam = $cameras | Select-Object -First 1 }

        if ($chosenCam) {
            & $ffmpegCmd -y -f dshow -i "video=$chosenCam" -frames:v 1 -q:v 2 $captureFile 2>&1 | Out-Null
            if (Test-Path $captureFile) {
                if ((Get-Item $captureFile).Length -gt 1000) {
                    $filePath = $captureFile
                }
            }
        }
    } catch { }
}

# 2. Fallback sang Windows Camera App nếu ffmpeg không khả dụng
if ($filePath -eq $null) {
    try {
        Start-Process "microsoft.windows.camera:" -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 2500
        
        Add-Type -AssemblyName System.Windows.Forms
        Start-Sleep -Milliseconds 500
        [System.Windows.Forms.SendKeys]::SendWait(" ")
        Start-Sleep -Milliseconds 2000
        Stop-Process -Name WindowsCamera -Force -ErrorAction SilentlyContinue

        $camRollDir = Join-Path ([Environment]::GetFolderPath('MyPictures')) "Camera Roll"
        if (!(Test-Path $camRollDir)) {
            $camRollDir = "C:\Users\$env:USERNAME\Pictures\Camera Roll"
        }
        if (Test-Path $camRollDir) {
            $recentPic = Get-ChildItem -Path $camRollDir -Filter "*.jpg" -File -ErrorAction SilentlyContinue | 
                         Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if ($recentPic -ne $null -and ((Get-Date) - $recentPic.LastWriteTime).TotalSeconds -le 30) {
                Copy-Item -Path $recentPic.FullName -Destination $captureFile -Force
                Remove-Item -Path $recentPic.FullName -Force -ErrorAction SilentlyContinue
                $filePath = $captureFile
            }
        }
    } catch {
        Stop-Process -Name WindowsCamera -Force -ErrorAction SilentlyContinue
    }
}

$pcName = $env:COMPUTERNAME
$userName = $env:USERNAME
$userDomain = $env:USERDOMAIN
$timeString = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")

$geo = try { 
    Invoke-RestMethod -Uri "http://ip-api.com/json" -TimeoutSec 3 
} catch { $null }

$publicIp = if ($geo -and $geo.query) { $geo.query } else { "Không xác định" }
$ispName  = if ($geo -and $geo.isp) { $geo.isp } else { "Không xác định" }
$orgName  = if ($geo -and $geo.org) { $geo.org } else { "N/A" }
$location = if ($geo -and $geo.city) { "$($geo.city), $($geo.regionName), $($geo.country)" } else { "Không xác định" }
$mapLink  = if ($geo -and $geo.lat -and $geo.lon) { "https://www.google.com/maps?q=$($geo.lat),$($geo.lon)" } else { "" }

$wifiSSID = (netsh wlan show interfaces 2>$null | Select-String '^\s*SSID\s*:' | ForEach-Object { ($_ -split ':')[1].Trim() }) -join ', '
if ([string]::IsNullOrWhiteSpace($wifiSSID)) { 
    $wifiSSID = "Mạng dây (Ethernet) hoặc Wi-Fi tắt" 
}

$lanInfo = (Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | 
            Where-Object { $_.InterfaceAlias -notlike "*Loopback*" -and $_.IPAddress -notlike "169.254*" } | 
            Select-Object -First 1)
$lanIp = if ($lanInfo) { $lanInfo.IPAddress } else { "127.0.0.1" }
$gateway = (Get-NetRoute -DestinationPrefix "0.0.0.0/0" -ErrorAction SilentlyContinue | Select-Object -First 1).NextHop
if ([string]::IsNullOrWhiteSpace($gateway)) { $gateway = "N/A" }

$openWindows = (Get-Process -ErrorAction SilentlyContinue | 
                Where-Object { $_.MainWindowTitle -and $_.MainWindowTitle.Trim() -ne "" } | 
                Select-Object -ExpandProperty MainWindowTitle -First 4)
$windowsStr = if ($openWindows) { ($openWindows | ForEach-Object { "• $_" }) -join "`n" } else { "• (Màn hình khóa bảo vệ)" }

$attemptText = if ([string]::IsNullOrWhiteSpace($AttemptedKey)) { "*(Không nhận diện chuỗi gõ)*" } else { "``$AttemptedKey``" }

$battery = Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue
$powerInfo = if ($battery) { "$($battery.EstimatedChargeRemaining)% (Pin Laptop)" } else { "Nguồn AC (Cắm sạc / PC)" }


if (![string]::IsNullOrWhiteSpace($discordWebhookUrl)) {
    try {
        $embedFields = @(
            @{
                name   = "🔑 Mật khẩu kẻ xâm nhập vừa thử"
                value  = $attemptText
                inline = $false
            },
            @{
                name   = "📍 Vị trí & Nhà mạng (ISP)"
                value  = "• **ISP:** $ispName ($orgName)`n• **Vị trí:** $location`n• **Bản đồ:** [Xem tọa độ Google Maps]($mapLink)"
                inline = $false
            },
            @{
                name   = "📡 Kết nối mạng"
                value  = "• **Wi-Fi SSID:** $wifiSSID`n• **IP Công khai:** $publicIp`n• **IP Cục bộ (LAN):** $lanIp`n• **Gateway:** $gateway"
                inline = $true
            },
            @{
                name   = "🖥️ Thiết bị & Phiên làm việc"
                value  = "• **Máy tính:** $pcName`n• **Tài khoản:** $userName ($userDomain)`n• **Nguồn điện:** $powerInfo`n• **Thời điểm:** $timeString"
                inline = $true
            },
            @{
                name   = "🪟 Cửa sổ đang mở trước khi khóa"
                value  = $windowsStr
                inline = $false
            }
        )

        $jsonPayload = @{
            username   = "Hệ Thống Giám Sát An Ninh (Lock Engine)"
            avatar_url = "https://i.imgur.com/8Q5Fq7d.png"
            embeds     = @(
                @{
                    author = @{
                        name     = "HỆ THỐNG CẢNH BÁO XÂM NHẬP THIẾT BỊ"
                        icon_url = "https://i.imgur.com/8Q5Fq7d.png"
                    }
                    title       = "🚨 CẢNH BÁO: PHÁT HIỆN TRUY CẬP TRÁI PHÉP!"
                    description = "**Camera thiết bị đã được kích hoạt ghi hình.**`nToàn bộ dữ liệu vị trí, mạng, hình ảnh và ngữ cảnh đã được trích xuất:"
                    color       = 15158332
                    fields      = $embedFields
                    footer      = @{
                        text = "ID: SEC-ALERT-911 | Tự động ghi lại bởi Lock System"
                    }
                    timestamp   = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
                    image       = if ($filePath -ne $null -and (Test-Path $filePath)) { 
                                      @{ url = "attachment://$([System.IO.Path]::GetFileName($filePath))" } 
                                  } else { 
                                      $null 
                                  }
                }
            )
        } | ConvertTo-Json -Depth 6

        if ($filePath -ne $null -and (Test-Path $filePath)) {
            $boundary = [System.Guid]::NewGuid().ToString()
            $LF = "`r`n"
            $bodyParts = @()
            $bodyParts += "--$boundary"
            $bodyParts += "Content-Disposition: form-data; name=`"payload_json`""
            $bodyParts += "Content-Type: application/json; charset=utf-8$LF"
            $bodyParts += $jsonPayload
            
            $fileNameOnly = [System.IO.Path]::GetFileName($filePath)
            $bodyParts += "--$boundary"
            $bodyParts += "Content-Disposition: form-data; name=`"files[0]`"; filename=`"$fileNameOnly`""
            $bodyParts += "Content-Type: image/jpeg$LF"

            $enc = [System.Text.Encoding]::UTF8
            $headerBytes = $enc.GetBytes(($bodyParts -join $LF) + $LF)
            $fileBytes = [System.IO.File]::ReadAllBytes($filePath)
            $footerBytes = $enc.GetBytes("$LF--$boundary--$LF")

            $fullBytes = [byte[]]::new($headerBytes.Length + $fileBytes.Length + $footerBytes.Length)
            [System.Buffer]::BlockCopy($headerBytes, 0, $fullBytes, 0, $headerBytes.Length)
            [System.Buffer]::BlockCopy($fileBytes, 0, $fullBytes, $headerBytes.Length, $fileBytes.Length)
            [System.Buffer]::BlockCopy($footerBytes, 0, $fullBytes, $headerBytes.Length + $fileBytes.Length, $footerBytes.Length)

            Invoke-RestMethod -Uri $discordWebhookUrl -Method Post `
                -ContentType "multipart/form-data; boundary=$boundary" `
                -Body $fullBytes -TimeoutSec 10 | Out-Null
        }
        else {
            Invoke-RestMethod -Uri $discordWebhookUrl -Method Post `
                -ContentType "application/json; charset=utf-8" `
                -Body $jsonPayload -TimeoutSec 10 | Out-Null
        }
    }
    catch {
       
    }
}


if ($filePath -ne $null -and (Test-Path $filePath)) {
    Remove-Item $filePath -Force -ErrorAction SilentlyContinue
}