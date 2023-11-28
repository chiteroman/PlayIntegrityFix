# Play Integrity Fix

A Zygisk module which fixes "ctsProfileMatch" (SafetyNet) and "MEETS_DEVICE_INTEGRITY" (Play Integrity).

To use this module you must have one of the following:

- Magisk with Zygisk enabled.
- KernelSU with [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) module installed.

[**Download the latest here**](https://github.com/chiteroman/PlayIntegrityFix/releases/latest).

## Telegram group

https://t.me/playintegrityfix

## Donations

- [PayPal](https://paypal.me/chiteroman)

## Official posts

- [XDA](https://xdaforums.com/t/module-play-integrity-fix-safetynet-fix.4607985/)

## About module

It injects a classes.dex file to modify a few fields in the android.os.Build class. Also, it creates a hook in the native code to modify system properties. These are spoofed only to Google Play Services' DroidGuard (SafetyNet/Play Integrity) service.

The purpose of the module is to avoid a hardware attestation.

## Troubleshooting

### Failing BASIC verdict

If you are failing basicIntegrity (SafetyNet) or MEETS_BASIC_INTEGRITY (Play Integrity) something is wrong in your setup. Recommended steps in order to find the problem:

- Disable all modules except this one

Some modules which modify system can trigger DroidGuard detection, never hook GMS processes.

### Failing DEVICE verdict (on KernelSU)

- Disable ZygiskNext
- Reboot
- Enable ZygiskNext

### Play Protect/Store Certification and Google Wallet Tap To Pay Setup Security Requirements

Follow these steps:

- Flash the module in Magisk/KernelSU
- Clear Google Wallet cache (if you have it)
- Clear Google Play Store cache and data
- Clear Google Play Services (com.google.android.gms) cache and data (Optionally skip clearing data and wait some time, ~24h, for it to resolve on its own)
- Reboot

<details>
<summary>Guide</summary>

![Google services cache](./wallet-troubleshoot-1.jpg)
![Removing all data](./wallet-troubleshoot-2.jpg)

</details>

### Read module logs

You can read module logs using this command directly after boot:

```
adb shell "logcat | grep 'PIF'"
```

## Can this module pass MEETS_STRONG_INTEGRITY?

No.

## About Play Integrity, SafetyNet is deprecated

You can read more info
here: [click me](https://xdaforums.com/t/info-play-integrity-api-replacement-for-safetynet.4479337/)
