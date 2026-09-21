CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

Test package: WOL Ethernet enable fix (retry)
  Source: 4f6e529f (includes PR #4 + enable/helper fixes)
  Branch: cursor/wol-nic-enable-fix-ecca

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/wol-nic-enable-fix-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  c61c5734131f4ff24977d9fcd89405d9ef052b3d5917cd116f0f6f6f2df59e30

This installer stops running Veyon processes before replacing
windows-platform.dll, then restarts VeyonService.

NIC flash test
--------------
1. Close Master if it is open. Run this setup as administrator.
2. After install, open Master from:
     C:\Program Files\Veyon\veyon-master.exe
3. Leave Intel Ethernet Disabled. Keep the adapter window visible.
4. Select a computer and click Power on.
5. Intel Ethernet should become Enabled for up to about 5 seconds,
   then return to Disabled.

If it still does not flash, send this file:

  C:\ProgramData\Veyon\wol-adapter.log

Without a school Ethernet cable, the other PC will not wake.
