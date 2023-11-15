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

## Make FCM Push back to work after cleared GSF data
Once your cleared GSF (Google Service Framework, com.google.android.gsf), a new DeviceID of Google Service Framework will be generated. So all the FCM tokens that have registered in the server of Apps will no longer work (it will point to your old DeviceID). You can follow these steps to make the Apps to generate a new FCM token. 

The idea is to delete a file called `xxx.gms.appid-no-backup` (xxx usually is the package name) in the app's files folder. Once the file does not exist, the app will generate a new FCM token when it starts up next time.

Run the following commands to do that, you can use `adb shell`, Termux, some terminal apps, whatever.

1. Get the root user
```
su
```

2. cd to `/data/data`
```
cd /data/data
```

3. Search for the files end with `gms.appid-no-backup` firstly (without really delete it), so you can review the list of the files will be deleted, make sure it will not delete something wrong (usually it should not. I don't think any other useful files named like this). If you don't really care, you can skip this step.
```
find . -type f -name '*gms.appid-no-backup'
```

4. Delete all the files end with `gms.appid-no-backup`
```
find . -type f -name '*gms.appid-no-backup' -delete
```

5. Reboot your device.

6. It is better to launch the apps that receive FCM push one time, to make sure it generate a new FCM token and register with the server.

## Thanks to
- [Dobby](https://github.com/jmpews/Dobby)
