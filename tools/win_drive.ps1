# Drive the Windows build of pcsx-abnxt without touching the keyboard: start it on a game, post key
# presses to its window (WM_KEYDOWN/UP, like AutoBleem's tools/win_drive.ps1), screenshot after each,
# collect the log. For smoke tests from a script.
#
#   tools\win_drive.ps1 -Game "D:/AB/Games/X/game.cue" -Sequence "12;esc;2;down;down;1"
#     a number waits that many seconds, a name presses that key; screenshots land in build_win\run\shotN.png
#   keys: esc up down left right return backspace f1..f12 z x s d c v w r e t (the default binds);
#   'close' sends the window's close button
param(
  [string]$Game = "D:/AB/Games/Crash Bandicoot (U)/SCUS-94900.cue",
  [string]$Sequence = "12;esc;2",
  [string]$EmuArgs = ""
)
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Run = Join-Path $Root "build_win\run"
$Exe = Join-Path $Root "build_win\pcsx-ab.exe"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
  public static IntPtr FindByPid(uint pid, string titlePart) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) {
      var sb = new System.Text.StringBuilder(256); GetWindowText(h, sb, 256);
      if (sb.ToString().Contains(titlePart)) { found = h; return false; } } return true; }, IntPtr.Zero);
    return found;
  }
}
"@
# name -> virtual key, scancode
$keys = @{ esc=@(0x1B,0x01); return=@(0x0D,0x1C); backspace=@(0x08,0x0E); space=@(0x20,0x39)
           up=@(0x26,0x48); down=@(0x28,0x50); left=@(0x25,0x4B); right=@(0x27,0x4D)
           z=@(0x5A,0x2C); x=@(0x58,0x2D); s=@(0x53,0x1F); d=@(0x44,0x20); c=@(0x43,0x2E); v=@(0x56,0x2F)
           w=@(0x57,0x11); r=@(0x52,0x13); e=@(0x45,0x12); t=@(0x54,0x14)
           f1=@(0x70,0x3B); f2=@(0x71,0x3C); f3=@(0x72,0x3D); f4=@(0x73,0x3E); f5=@(0x74,0x3F); f6=@(0x75,0x40)
           f7=@(0x76,0x41); f8=@(0x77,0x42); f9=@(0x78,0x43); f10=@(0x79,0x44); f11=@(0x7A,0x57); f12=@(0x7B,0x58) }
# the arrows are extended keys: without bit 24 in lParam SDL takes scancode 0x50 for keypad 2, not Down
$extended = @('up','down','left','right')
function KeyDown($h, $name) {
  $k = $keys[$name]; if ($null -eq $k) { "unknown key $name"; return }
  $ext = 0; if ($extended -contains $name) { $ext = 1 -shl 24 }
  [W]::PostMessage($h, 0x100, [IntPtr]$k[0], [IntPtr](($k[1] -shl 16) -bor 1 -bor $ext)) | Out-Null
}
function KeyUp($h, $name) {
  $k = $keys[$name]; if ($null -eq $k) { return }
  $ext = 0; if ($extended -contains $name) { $ext = 1 -shl 24 }
  [W]::PostMessage($h, 0x101, [IntPtr]$k[0], [IntPtr](($k[1] -shl 16) -bor 1 -bor $ext -bor (1 -shl 30) -bor (1 -shl 31))) | Out-Null
}
$shot = 0
function Shot { $script:shot++; $b = New-Object System.Drawing.Bitmap 1920,1080; $g = [System.Drawing.Graphics]::FromImage($b); $g.CopyFromScreen(0,0,0,0,$b.Size); $b.Save("$Run\shot$script:shot.png"); $g.Dispose(); $b.Dispose() }

Set-Location $Run
Remove-Item "$Run\shot*.png" -ErrorAction SilentlyContinue
$argl = @('-cdfile', "`"$Game`"") + ($EmuArgs -split ' ' | Where-Object { $_ -ne "" })
$p = Start-Process -FilePath $Exe -ArgumentList $argl -RedirectStandardOutput "$Run\out.txt" -RedirectStandardError "$Run\err.txt" -PassThru
$h = [IntPtr]::Zero
foreach ($step in $Sequence.Split(';')) {
  $step = $step.Trim()
  if ($step -match '^\d+(\.\d+)?$') { Start-Sleep ([double]$step); continue }
  if ($h -eq [IntPtr]::Zero) { $h = [W]::FindByPid($p.Id, "PCSX"); "window handle: $h" }
  if ($step -eq 'close') { [W]::PostMessage($h, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep 3; continue }   # WM_CLOSE
  KeyDown $h $step; Start-Sleep -Milliseconds 120; KeyUp $h $step
  Start-Sleep -Milliseconds 700
  Shot
}
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; "killed (was running)" } else { "exited $($p.ExitCode)" }
