# Post-Grape - start the Standalone app, set the window size, save a screenshot (PNG).
# Usage: pwsh tools/capture.ps1 -Out shot.png [-Width 1216] [-Height 900] [-Wait 4] [-Keep]
param(
    [string]$Out = "shot.png",
    [int]$Width = 0,
    [int]$Height = 0,
    [int]$Wait = 4,
    [int]$HoverX = -1,
    [int]$HoverY = -1,
    [int]$ClickX = -1,
    [int]$ClickY = -1,
    [switch]$Keep
)

Add-Type @"
using System;
using System.Drawing;
using System.Runtime.InteropServices;
public static class Win {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int hh, uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, int x, int y, uint d, IntPtr e);
    [DllImport("user32.dll")] public static extern IntPtr PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    // Click at window-relative pixel (x, y) with window messages (the real mouse does not move).
    public static void Click(IntPtr h, int x, int y) {
        IntPtr lp = (IntPtr) ((y << 16) | (x & 0xffff));
        PostMessage(h, 0x0200, IntPtr.Zero, lp);
        System.Threading.Thread.Sleep(100);
        PostMessage(h, 0x0201, (IntPtr) 1, lp);
        System.Threading.Thread.Sleep(80);
        PostMessage(h, 0x0202, IntPtr.Zero, lp);
    }
    // Move the real mouse cursor over a window point (window-relative pixels).
    public static void Hover(IntPtr h, int x, int y) {
        RECT r; GetWindowRect(h, out r);
        SetCursorPos(r.L + x, r.T + y);
        System.Threading.Thread.Sleep(200);
        SetCursorPos(r.L + x + 1, r.T + y + 1);
    }
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    public static Bitmap Capture(IntPtr h) {
        RECT r; GetWindowRect(h, out r);
        var bmp = new Bitmap(r.R - r.L, r.B - r.T);
        using (var g = Graphics.FromImage(bmp)) {
            IntPtr dc = g.GetHdc();
            PrintWindow(h, dc, 2);
            g.ReleaseHdc(dc);
        }
        return bmp;
    }
}
"@ -ReferencedAssemblies System.Drawing

[Win]::SetProcessDPIAware() | Out-Null
$exe = Join-Path $PSScriptRoot "..\build\PostGrape_artefacts\Release\Standalone\Post-Grape.exe"
$p = Start-Process -FilePath $exe -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 50 -and $h -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 200
    $p.Refresh()
    $h = $p.MainWindowHandle
}
if ($Width -gt 0) {
    [Win]::SetWindowPos($h, [IntPtr]::Zero, 40, 40, $Width, $Height, 0x0004) | Out-Null
}
if ($HoverX -ge 0) {
    Start-Sleep -Seconds 2
    [Win]::Hover($h, $HoverX, $HoverY)
}
if ($ClickX -ge 0) {
    Start-Sleep -Seconds 2
    [Win]::Click($h, $ClickX, $ClickY)
}
Start-Sleep -Seconds $Wait
$bmp = [Win]::Capture($h)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "Saved $Out ($($bmp.Width) x $($bmp.Height))"
if (-not $Keep) { Stop-Process -Id $p.Id -Force }
