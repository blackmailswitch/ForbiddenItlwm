# Installing ForbiddenItlwm

This beta has been tested on one hackintosh: an HP Envy x360 13-bf with an Intel AX211, on macOS 26.7.1 (25G241), booted with OpenCore 1.0.8, with an iPhone 11. Users report it working on 26.7.2 (25G332) and 26.7.3 (25G342). The Sequoia build (15.8 and 15.8.3) was verified offline. Users report Wi-Fi working on it. AirDrop needs 0.5.1-test1 or later; users confirmed it working on 15.8.1, 15.8.2, 15.8.3 and 15.6, on 15.8.1 and 15.8.2 before 0.6.0-beta.1. The Sonoma build (14.8.9, test build) was verified offline, and one user reports AirDrop working on it; it hasn't been tested on the project's hackintosh. The details are in [TESTED-HARDWARE.md](TESTED-HARDWARE.md). Read the whole page before you start.

## Before you start

- **Check your macOS build:** `sw_vers -buildVersion`.
  - **Tahoe:** `25G241`, `25G332` or `25G342`, and from 0.7.0-alpha.2 `25G229` (26.7) and `25G330` (26.7.2). Use the Tahoe kexts.
  - **Sequoia:** `24H23` (15.8) and `24H241` (15.8.3) were checked offline. Up to 0.7.0-alpha.1, `24H32` (15.8.1) and `24H232` (15.8.2) start AirDrop, but the announcement copy stays off, so other devices may not list the Mac. From 0.7.0-alpha.2, 15.8.1 (`24H32`), 15.8.2 (`24H230`, `24H232`), 15.7.3 (`24G419`), 15.7.4 (`24G517`), 15.7.8 (`24G824`) and 15.7.9 (`24G830`) are checked too, with BTLEAdvFixup patterns for 15.7.3, 15.7.8 and 15.7.9. Use the Sequoia kexts. AirDrop transfers on Sequoia need 0.5.1-test1 or later.
  - **Sonoma (test build):** `23J631` (14.8.9) was checked offline, and from 0.7.0-alpha.2 `23J220` (14.8.3), `23J319` (14.8.4), `23J520` (14.8.7) and `23J620` (14.8.8). Use the Sonoma kexts. On other Sonoma 14.4+ builds the Sonoma kext runs as plain Wi-Fi.
  - On another build of these macOS versions the driver keeps AirDrop off and works as plain Wi-Fi, apart from "expected" Sequoia 15.8.x kernels (AirDrop starts without the announcement copy) and with `-itlawdlanybuild`. Each kext starts only on its own macOS version.
  - If your build isn't listed, run `perl tools/check-my-mac.pl` and paste its output in [discussion #98](https://github.com/retropineaple/ForbiddenItlwm/discussions/98), so support can be added ([CHECK-MY-MAC](CHECK-MY-MAC.md)).
- **Use the kexts for your macOS version.** ForbiddenItlwm and BTLEAdvFixup are built separately for Tahoe, Sequoia and Sonoma. A ForbiddenItlwm built for the other version refuses to start, and BTLEAdvFixup's patterns won't match.
- **Revert OCLP Wi-Fi root patches.** ForbiddenItlwm uses the Wi-Fi stack that ships with macOS. If you applied OpenCore Legacy Patcher's "Modern Wireless" root patches for an older AirportItlwm, revert them first. Also remove what that setup added to config.plist: a `Kernel → Block` entry for `com.apple.iokit.IOSkywalkFamily`, and injected `IOSkywalkFamily.kext` or `IO80211FamilyLegacy.kext`.
- **Run the checker:** `bash tools/compat/check_compat.sh`. It tells you whether your card and Bluetooth are likely to work.
- **Use a USB stick.** Copy your working EFI folder to a USB stick and make every change there. Your internal EFI stays as it is, so if anything goes wrong you just boot without the stick.
- **Know the rollback:** boot from your internal EFI, or put the original `AirportItlwm.kext` and `config.plist` back on the stick.

## What you need

| Kext | Where from | Notes |
| --- | --- | --- |
| `ForbiddenItlwm.kext` | This repository, Tahoe, Sequoia or Sonoma build | Replaces `AirportItlwm.kext` |
| `BTLEAdvFixup.kext` | This repository, Tahoe, Sequoia or Sonoma build | Lets macOS start Bluetooth LE advertising on an Intel controller, and replaces IntelBTPatcher. BlueToolFixup is still needed next to it |
| `Lilu.kext` | [acidanthera/Lilu](https://github.com/acidanthera/Lilu) | Tested with 1.7.2 |
| `IntelBluetoothFirmware.kext` | [OpenIntelWireless/IntelBluetoothFirmware](https://github.com/OpenIntelWireless/IntelBluetoothFirmware) | Tested with 2.5.1 |
| `BlueToolFixup.kext` | [acidanthera/BrcmPatchRAM](https://github.com/acidanthera/BrcmPatchRAM) | Tested with 2.7.3 |

**Disable `IntelBTPatcher.kext`.** It and BTLEAdvFixup hook the same function, and Lilu panics if two plugins do. BTLEAdvFixup takes over IntelBTPatcher's job. For the same reason, don't use IntelBTUnified, which includes IntelBTPatcher's hook ([#32](https://github.com/retropineaple/ForbiddenItlwm/issues/32), [#51](https://github.com/retropineaple/ForbiddenItlwm/issues/51)); use IntelBluetoothFirmware and BlueToolFixup.

## Building

You need the Xcode Command Line Tools (`xcode-select --install`). Full Xcode isn't needed. The build was tested with Python 3 on the Intel hackintosh it was developed on.

1. Put MacKernelSDK in the repository folder. Commit `3f75008` is the one tested:

   ```bash
   git clone https://github.com/acidanthera/MacKernelSDK
   ```

2. Build the Wi-Fi driver for your macOS version. For Tahoe:

   ```bash
   python3 tools/build.py --out build
   ```

   For Sequoia:

   ```bash
   python3 tools/build.py --target sequoia --out build-sequoia
   ```

   For Sonoma:

   ```bash
   python3 tools/build.py --target sonoma --out build-sonoma
   ```

   The result is `ForbiddenItlwm.kext` in the output folder. The first build also generates `include/FwBinary.cpp` from the firmware in `itlwm/firmware`.

3. Build BTLEAdvFixup. It needs the Lilu SDK, which comes with Lilu's **DEBUG** release. Make a folder holding `MacKernelSDK` and the DEBUG `Lilu.kext`, then:

   ```bash
   python3 tools/build_btleadv.py --out build/btleadv --tree /path/to/that/folder
   ```

   For Sequoia, add `--target sequoia`, and for Sonoma `--target sonoma`, each with its own output folder. The result is `BTLEAdvFixup.kext` in the output folder.

4. Optional, for developers: run the offline tests and the ABI audit. The audit checks every virtual slot of the AWDL interface against its macOS version's own table. It must report 568 of 568 for Tahoe, 563 of 563 for Sequoia with `--target sequoia --build build-sequoia`, and 547 of 547 for Sonoma with `--target sonoma --build build-sonoma`:

   ```bash
   python3 tools/test_awdl.py
   ```

   ```bash
   python3 tools/abi/audit_vtable.py --lifecycle-overrides
   ```

## OpenCore setup

All of this goes on the **USB stick's** EFI.

1. **Kexts:** in `EFI/OC/Kexts`, remove `AirportItlwm.kext` and copy in `ForbiddenItlwm.kext` and `BTLEAdvFixup.kext`.

2. **`Kernel` → `Add`:** keep this order, so each kext comes after what it depends on.

   | Order | BundlePath | ExecutablePath | Notes |
   | --- | --- | --- | --- |
   | 1 | `Lilu.kext` | `Contents/MacOS/Lilu` | |
   | 2 | `IntelBluetoothFirmware.kext` | `Contents/MacOS/IntelBluetoothFirmware` | |
   | 3 | `BlueToolFixup.kext` | `Contents/MacOS/BlueToolFixup` | `MinKernel` 21.0.0 |
   | 4 | `BTLEAdvFixup.kext` | `Contents/MacOS/BTLEAdvFixup` | `MinKernel` 21.0.0. After BlueToolFixup, so it can undo BlueToolFixup's patches for APFS's check too (#62) |
   | 5 | `ForbiddenItlwm.kext` | `Contents/MacOS/ForbiddenItlwm` | Replaces the old `AirportItlwm.kext` entry |

   **Recommended:** limit `ForbiddenItlwm.kext` and `BTLEAdvFixup.kext` to their macOS version with `MinKernel` and `MaxKernel`, so a stick moved to another macOS doesn't load the wrong build:

   | Build | `MinKernel` | `MaxKernel` |
   | --- | --- | --- |
   | Tahoe | 25.0.0 | 25.99.99 |
   | Sequoia | 24.0.0 | 24.99.99 |
   | Sonoma | 23.4.0 | 23.99.99 |

   Set `IntelBTPatcher.kext` to `Enabled` = false if you have it. This is required. Make sure no `AirportItlwm.kext` or `itlwm.kext` entry is still enabled.

3. **Boot arguments:** from 0.7.0-alpha.2 add `-btleadv`; the driver turns AirDrop's other flags on by itself, and flags left over from an older release do no harm. Up to 0.7.0-alpha.1, add the whole tested set from [BOOT-FLAGS.md](BOOT-FLAGS.md#up-to-070-alpha1-the-tested-set). Add them to `boot-args` under `NVRAM` → `Add` → `7C436110-AB2A-4BBB-A880-FE41995C9F82`. Keep your existing arguments. Because OpenCore only writes NVRAM keys listed under `Delete`, check that `boot-args` is listed there for the same GUID. The experimental flags in BOOT-FLAGS are optional, for testers, and work only from 0.7.0-alpha.1. macOS ignores `boot-args` longer than 1023 characters, so keep the line shorter than that.

4. **Validate:** `plutil -lint config.plist`, and `ocvalidate` if you have it.

## First boot

1. Boot from the USB stick.
2. Join your normal Wi-Fi network as usual.
3. Wait a minute, then check that the driver and its AirDrop listener are running. If Wi-Fi is off or greyed out, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md#wi-fi-is-off-or-greyed-out-and-theres-no-en0).

   ```bash
   ioreg -l -w0 | grep -E '"ItlwmAWDL(InterfaceBuild|BuildCheck)"|"ITLAWDLCoex(Added|Channel|CountryLearned)"'
   ```

   You should see `forbidden-0.7.0-alpha.2` (`forbidden-0.7.0-alpha.2-sequoia` on Sequoia, `forbidden-0.7.0-alpha.2-sonoma` on Sonoma), `BuildCheck` `verified`, `Added` Yes, `Channel` 149 (or 44 where your iPhone uses 44, for example in Japan) and `CountryLearned` Yes. `Channel` 6 means the firmware hasn't opened 149 or 44: see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

4. Or run `bash tools/compat/check_compat.sh` again. It now also reads the listener's state.
5. Turn on AirDrop on both devices. Contacts Only works when the iPhone and the Mac share an Apple Account or know each other as contacts. Otherwise use Everyone for 10 Minutes.
6. Send a file each way.

## Before a macOS update

Boot once without `-btleadv` and `-btleadvnodupfilter` (or with BTLEAdvFixup disabled), then update. `-btleadvnodupfilter` is new in 0.7.0-alpha.1 and hasn't been tried with an update yet, so remove it too. While BTLEAdvFixup is active, `bluetoothd`'s patched pages are what the system reads, and an incremental update that checks the file can fail. If the update still fails, disable BlueToolFixup and BTLEAdvFixup too for the update (#78): BlueToolFixup patches `bluetoothd` the same way, whatever the flags. Then check that the new build is supported before turning the flags back on: see [the macOS builds](TESTED-HARDWARE.md#macos-builds). If it isn't listed, run `perl tools/check-my-mac.pl` on the new build and post the output in [discussion #98](https://github.com/retropineaple/ForbiddenItlwm/discussions/98).

## Uninstalling

On the stick (or your EFI, if you moved it there):
1. Remove `ForbiddenItlwm.kext` and `BTLEAdvFixup.kext`, and their `Kernel` → `Add` entries.
2. Put `AirportItlwm.kext` back.
3. Put your Bluetooth setup back as it was, for example re-enable `IntelBTPatcher.kext` if you used it: BTLEAdvFixup took over its fixes.
4. Remove the `-itlawdl*`, `itlawdl*`, `itlsched*`, `-itlsched*`, `itlsim*`, `-itlsim*` and `-btleadv*` boot flags.
5. Reboot.

The driver writes nothing to disk. BTLEAdvFixup patches `bluetoothd` in memory only.
