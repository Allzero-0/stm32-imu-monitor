# ---------------------------------------------------------------------------
#  把本项目推送到 GitHub（纯 PowerShell，不需要 Git Bash）
#
#  用法：双击同目录的 push_to_github.bat，或右键本文件 -> 使用 PowerShell 运行
#
#  前置条件：
#    1. GitHub 网页已建好空仓库： https://github.com/new
#       - Public，且 README / .gitignore / License 三个勾都不要勾
#    2. 已生成 Personal Access Token（classic，只勾 repo）
#
#  安全性：token 输入时不显示，不写入任何文件，不写入 .git/config
# ---------------------------------------------------------------------------

$ErrorActionPreference = "Continue"
Set-Location $PSScriptRoot

# git 可执行文件：优先用 WorkBuddy 内置的 PortableGit，找不到再用系统 git
$GIT = "C:\Users\lenovo\.workbuddy\binaries\PortableGit\versions\1.2.0\mingw64\bin\git.exe"
if (-not (Test-Path $GIT)) {
    $GIT = "git"
}

Write-Host ""
Write-Host "===========================================" -ForegroundColor Cyan
Write-Host "  推送 stm32-imu-monitor 到 GitHub" -ForegroundColor Cyan
Write-Host "===========================================" -ForegroundColor Cyan
Write-Host ""

try {
    & $GIT --version | Out-Null
} catch {
    Write-Host "[错误] 找不到 git" -ForegroundColor Red
    Read-Host "按回车退出"
    exit 1
}

# ---------- 1. 仓库信息 ----------
$user = Read-Host "GitHub 用户名 [Allzero-0]"
if ([string]::IsNullOrWhiteSpace($user)) { $user = "Allzero-0" }

$repo = Read-Host "仓库名 [stm32-imu-monitor]"
if ([string]::IsNullOrWhiteSpace($repo)) { $repo = "stm32-imu-monitor" }

Write-Host ""
Write-Host "请先确认 https://github.com/$user/$repo 已经建好：" -ForegroundColor Yellow
Write-Host "  Public，且创建时 README / .gitignore / License 三个勾都没勾" -ForegroundColor Yellow
$ready = Read-Host "建好了吗？输入 y 继续，其他键退出"
if ($ready -ne "y" -and $ready -ne "Y") {
    Write-Host ""
    Write-Host "先去这里建仓库： https://github.com/new" -ForegroundColor Yellow
    Write-Host "  仓库名 : $repo"
    Write-Host "  可见性 : Public"
    Write-Host "  三个勾 : 都不勾"
    Read-Host "按回车退出"
    exit 1
}

# ---------- 2. git 身份 ----------
$name = (& $GIT config user.name) 2>$null
if ([string]::IsNullOrWhiteSpace($name)) {
    $name = Read-Host "提交者名字（会显示在 commit 记录里，建议用 GitHub 用户名）"
    & $GIT config user.name "$name"
}
$email = (& $GIT config user.email) 2>$null
if ([string]::IsNullOrWhiteSpace($email)) {
    $email = Read-Host "提交者邮箱"
    & $GIT config user.email "$email"
}

# ---------- 3. token（输入时不显示） ----------
Write-Host ""
$sec = Read-Host "粘贴 GitHub token（ghp_ 开头，输入时不显示，输完回车）" -AsSecureString
$TOKEN = [Runtime.InteropServices.Marshal]::PtrToStringAuto(
    [Runtime.InteropServices.Marshal]::SecureStringToBSTR($sec))
if ([string]::IsNullOrWhiteSpace($TOKEN)) {
    Write-Host "[错误] token 为空" -ForegroundColor Red
    Read-Host "按回车退出"
    exit 1
}

# ---------- 4. 提交 ----------
Write-Host ""
Write-Host ">> 整理文件..." -ForegroundColor Green
& $GIT add -A

$hasChanges = (& $GIT diff --cached --name-only) 2>$null
if ([string]::IsNullOrWhiteSpace($hasChanges)) {
    Write-Host "  没有新的改动需要提交"
} else {
    Write-Host ">> 提交中..." -ForegroundColor Green
    $msg = "feat: STM32F103 + MPU6050 姿态采集与上位机监控系统`n`n" +
           "固件（寄存器级裸机，不依赖 HAL/CMSIS）：`n" +
           "- 系统时钟 72MHz，HSE 失败自动回退 HSI`n" +
           "- 软件 I2C 驱动 MPU6050，含总线死锁恢复与零偏校准`n" +
           "- ADC 三通道扫描 + DMA 循环搬运`n" +
           "- TIM2 200Hz 采样节拍，中断只置标志`n" +
           "- USART1 DMA 非阻塞发送 + 中断环形接收`n" +
           "- 加速度/陀螺仪互补滤波姿态解算`n" +
           "- 自定义二进制帧协议（帧头+序号+时间戳+CRC8）`n`n" +
           "上位机：`n" +
           "- Python 串口解析 + 内置 HTTP 服务`n" +
           "- 浏览器端原生 Canvas：实时曲线、3D 姿态、CSV 记录`n" +
           "- 支持无硬件模拟模式"
    & $GIT commit -q -m $msg
    Write-Host "  已提交"
}

& $GIT branch -M main

# ---------- 5. 推送（一次性 URL，token 不落盘） ----------
Write-Host ">> 推送中..." -ForegroundColor Green
& $GIT push "https://$TOKEN@github.com/$user/$repo.git" main

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "===========================================" -ForegroundColor Green
    Write-Host "  推送成功！" -ForegroundColor Green
    Write-Host "  https://github.com/$user/$repo" -ForegroundColor Green
    Write-Host "===========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "建议：去 GitHub -> Settings -> Developer settings" -ForegroundColor Yellow
    Write-Host "      -> Personal access tokens 把这个 token 删掉" -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "[推送失败] 常见原因：" -ForegroundColor Red
    Write-Host "  1. 建仓库时勾了 README/.gitignore/License -> 删掉仓库重建，什么都别勾"
    Write-Host "  2. token 没勾 repo 权限，或已过期"
    Write-Host "  3. 用户名/仓库名填错了"
}

Write-Host ""
Read-Host "按回车关闭窗口"
