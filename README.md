# Play Integrity Fix

This module tries to fix Play Integrity and SafetyNet verdicts to get a valid attestation.

## NOTE

This module is not made to hide root, nor to avoid detections in other apps. It only serves to pass Device verdict in the Play Integrity tests and certify your device.
All issues created to report a non-Google app not working will be closed without notice.

## Tutorial

You will need root and Zygisk, so you must choose ONE of this three setups:

- [Magisk](https://github.com/topjohnwu/Magisk) with Zygisk enabled.
- [KernelSU](https://github.com/tiann/KernelSU) with [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) module installed.
- [APatch](https://github.com/bmax121/APatch) with [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) module installed.

After flashing and reboot your device, you can check PI and SN using these apps:

- Play Integrity -> https://play.google.com/store/apps/details?id=gr.nikolasspyr.integritycheck
- SafetyNet -> https://play.google.com/store/apps/details?id=rikka.safetynetchecker

NOTE: if you get an error message about a limit, you need to use another app, this is because a lot of users are requesting an attestation.

NOTE: SafetyNet is obsolete, more info here: https://developer.android.com/privacy-and-security/safetynet/deprecation-timeline

Also, if you are using custom rom or custom kernel, be sure that your kernel name isn't blacklisted, you can check it running ```uname -r``` command. This is a list of banned strings: https://xdaforums.com/t/module-play-integrity-fix-safetynet-fix.4607985/post-89308909

## Verdicts

After requesting an attestation, you should get this result:

- MEETS_BASIC_INTEGRITY   ✅
- MEETS_DEVICE_INTEGRITY  ✅
- MEETS_STRONG_INTEGRITY  ❌
- MEETS_VIRTUAL_INTEGRITY ❌ (this is for emulators only)

You can know more about verdicts in this post: https://xdaforums.com/t/info-play-integrity-api-replacement-for-safetynet.4479337/

And in SafetyNet you should get this:

- basicIntegrity:  true
- ctsProfileMatch: true
- evaluationType:  BASIC

NOTE: Strong verdict is impossible to pass on unlocked bootloader devices, there are few devices and "exploits" which will allow you to pass it, but, in normal conditions, this verdict will be green only if you are using stock ROM and locked bootloader. The old posts talking about Strong pass was an "exploit" in Google servers, obviously, now it's patched.

## Acknowledgments
- [kdrag0n](https://github.com/kdrag0n/safetynet-fix) & [Displax](https://github.com/Displax/safetynet-fix) for the original idea.
- [osm0sis](https://github.com/osm0sis) for his original [autopif2.sh](https://github.com/osm0sis/PlayIntegrityFork/blob/main/module/autopif2.sh) script, and [backslashxx](https://github.com/backslashxx) & [KOWX712](https://github.com/KOWX712) for improving it ([action.sh](https://github.com/chiteroman/PlayIntegrityFix/blob/main/module/action.sh)).

## FAQ
https://xdaforums.com/t/pif-faq.4653307/

## Download
https://github.com/chiteroman/PlayIntegrityFix/releases/latest

## Donations
[PayPal](https://www.paypal.com/paypalme/chiteroman0)
