CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

Test package: WOL Ethernet enable fix
  Source: 809afad905e9db2f0c299ac4ea73cc7b88ae6a31
  Branch: cursor/wol-nic-enable-fix-ecca

This build includes the classroom product plus Wake-on-LAN temporary
Ethernet enable. Master is not elevated; Veyon Service (SYSTEM) opens
the Intel Ethernet adapter for a few seconds, then restores it.

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/wol-nic-enable-fix-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  50e94c397548fb56efd7c369dcee4a75de46f69c30fb590f59e6044a83613f86

After install (required)
------------------------
Restart the service so the new helper is loaded:

  sc stop VeyonService
  sc start VeyonService

Open Master from:

  C:\Program Files\Veyon\veyon-master.exe

NIC flash test (no school LAN required)
---------------------------------------
1. Open Windows adapter settings so you can see Intel Ethernet Connection.
2. Leave that adapter Disabled.
3. Select a computer in Master and click Power on.
4. The Intel Ethernet row should become Enabled for up to about 5 seconds,
   then return to Disabled. Fortinet adapters should not change.

Without a school Ethernet cable, the other PC will not wake. That is
expected. The adapter flash is the check that enable/restore works.

Install
-------
1. Uninstall any existing Veyon first (optional but recommended).
2. Right-click the setup EXE and Run as administrator.
3. Teacher PC: keep "CYC Veyon Master" selected.
   Student PC: uncheck "CYC Veyon Master", or run:
     veyon_4_11_2_win64_modified_setup.exe /S /NoMaster
4. Keep "Interception driver" selected on student PCs.
5. After install, restart VeyonService as above.

Install path stays `C:\Program Files\Veyon`. The Windows service name
stays VeyonService.

This is an unofficial MinGW cross-build of this fork (Qt 6.7.3),
not an official Veyon Solutions installer.
