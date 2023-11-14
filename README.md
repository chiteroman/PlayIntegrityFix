# Play Integrity Fix
A Zygisk module which fix "ctsProfileMatch" (SafetyNet) and "MEETS_DEVICE_INTEGRITY" (Play Integrity).

To use this module you must have one of this:
- Magisk with Zygisk enabled.
- KernelSU with [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) module installed.

[**Download the latest here**](https://github.com/chiteroman/PlayIntegrityFix/releases/latest).

## Donations
- [PayPal](https://paypal.me/chiteroman)

## Official posts
- [XDA](https://xdaforums.com/t/module-play-integrity-fix-safetynet-fix.4607985/)

## About module
It injects a classes.dex file to modify few fields in android.os.Build class. Also, in native code it creates a hook to modify system properties.
The purpose of the module is to avoid a hardware attestation.

## Failing BASIC verdict
If you are failing basicIntegrity (SafetyNet) or MEETS_BASIC_INTEGRITY (Play Integrity) something is wrong in your setup. My recommended steps in order to find the problem:
- Disable all modules except mine.
- Check your SELinux (must be enforced).

Some modules which modify system can trigger DroidGuard detection, never hook GMS processes.

## Certify Play Store and fix Google Wallet
Follow this steps:
- Clear Google Wallet cache.
- Clear Google Play Store cache.
- Clear GSF (com.google.android.gsf) data and cache.
- Flash my module in Magisk/KernelSU (if you already have my module, just ignore this step)

Then reboot device and should work. Also some users recommend to clear GMS data and cache but for me it wasn't necessary.

## Read module logs
You can read module logs using this command:
```
adb shell "logcat | grep 'PIF'"
```

## Can this module pass MEETS_STRONG_INTEGRITY?
No

## SafetyNet is obsolete
You can read more info here: [click me](https://xdaforums.com/t/info-play-integrity-api-replacement-for-safetynet.4479337/)

## Current Issues
It doesn't work in Xiaomi.eu custom ROMs due their fix implementation.
Their devs are already working on it: [click me](https://xiaomi.eu/community/threads/google-wallet-stopped-working-device-doesnt-meet-security-requirements.70444/post-704331).
If Xiaomi.eu devs drop support for your device and this module doesn't work you must change the ROM if you want to pass DEVICE verdict.

## Thanks to
- [Dobby](https://github.com/jmpews/Dobby)
