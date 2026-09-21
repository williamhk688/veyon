CYC Veyon 4.11.2 (based on Veyon) — Windows 64-bit installer
============================================================

Test package: NCPA-visible Ethernet enable (netsh)
  Source: e4fa7637
  Branch: cursor/wol-nic-enable-fix-ecca

Direct download:

  https://github.com/williamhk688/veyon/raw/cursor/wol-nic-enable-fix-ecca/installer/veyon_4_11_2_win64_modified_setup.exe

SHA-256:
  0c7c12782d4e188f16f7028e54a95470555c2351ea4a002dae179baf556bcac6

Previous log showed SetIfEntry changing MIB status for Intel I219-V, but
ncpa.cpl stayed 已停用. This build uses netsh first (same as the adapter
window), then INetConnection/SetupDi.

Watch 乙太網路 (Intel), not 乙太網路 2/3. With no cable it should look
like 乙太網路 2: 已拔除網路線, for about 8 seconds, then 已停用 again.

Close Master, install as administrator, open:

  C:\Program Files\Veyon\veyon-master.exe
