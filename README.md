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
- MEETS_STRONG_INTEGRITY  ❌ ([Can be achieved with some additional work](https://github.com/chiteroman/PlayIntegrityFix#a-word-on-passing-strong-integrity))
- MEETS_VIRTUAL_INTEGRITY ❌ (this is for emulators only)

You can know more about verdicts in this post: https://xdaforums.com/t/info-play-integrity-api-replacement-for-safetynet.4479337/

And in SafetyNet you should get this:

- basicIntegrity:  true
- ctsProfileMatch: true
- evaluationType:  BASIC

## A word on passing Strong Integrity
While this module only returns passing verdicts for as far up as `MEETS_DEVICE_INTEGRITY`, it is possible to achieve a passing verdict for `MEETS_STRONG_INTEGRITY` by using [5ec1cff's TrickyStore](https://github.com/5ec1cff/TrickyStore) ([XDA thread](https://xdaforums.com/t/tricky-store-bootloader-keybox-spoofing.4683446/)). To put simply, this allows for spoofing a valid certificate chain (Often distributed as a file named `keybox.xml` and just called a _keybox_) to your device's [Trusted Execution Environment (TEE) module](https://en.wikipedia.org/wiki/Trusted_execution_environment), in addition to spoofing the bootloader as locked. 

**However, it must be stressed that a keybox is hard to come by**, given that they're leaked (Usually inadvertently) from OEMs and vendors. Even still, they are also often quite quickly revoked, due to a combination of people sending a deluge of server requests (Mostly for flexing their strong verdicts, which they probably didn't need anyway... You know who you are) and Google [deploying specialised crawlers](https://developers.google.com/search/docs/crawling-indexing/google-special-case-crawlers#google-safety) for automated detection. And, as quickly mentioned before, **you'll likely won't even need one, since basic functions (NFC payments and RCS messaging... etc.) and the vast majority of apps only mandate device integrity/a spoofed locked bootloader**.

As for when a keybox is _eventually_ revoked, you'll know it's happened when you're only passing `MEETS_BASIC_INTEGRITY` or by checking the key's validity status via [vvb2060's Key Attestation Demo](https://github.com/vvb2060/KeyAttestation). At this point, you'll need to find another unrevoked keybox (Strong integrity), use the publicly available AOSP keybox (Device integrity), or just remove TrickyStore entirely (Device integrity).

_**TL;DR: Unless it is ABSOLUTELY VITAL for your use case(s), chances are you'll be completely fine only passing up as far as `MEETS_DEVICE_INTEGRITY`, and not diving into this rabbit hole.**_

_NOTE: [Per the upcoming changes for Play Integrity's verdicts on May 2025](https://developer.android.com/google/play/integrity/improvements), by default you'll only pass `MEETS_BASIC_INTEGRITY` as device integrity now requires a locked bootloader on Android 13 and later. While this can be circumvented by using the `spoofVendingSdk` attribute (Spoofs SDK 32/Android 12) in your `pif.json` configuration, **this will, to a varying degree, break some functionality in the Play Store** ([Given that this is an experimental feature of PIF](https://github.com/osm0sis/PlayIntegrityFork/pull/30)) including the likes of degraded ease of navigation, and the store outright crashing whenever an app is installed/updated. YMMV, to which it probably will._

## Acknowledgments
- [kdrag0n](https://github.com/kdrag0n/safetynet-fix) & [Displax](https://github.com/Displax/safetynet-fix) for the original idea.
- [osm0sis](https://github.com/osm0sis) for his original [autopif2.sh](https://github.com/osm0sis/PlayIntegrityFork/blob/main/module/autopif2.sh) script, and [backslashxx](https://github.com/backslashxx) & [KOWX712](https://github.com/KOWX712) for improving it ([action.sh](https://github.com/chiteroman/PlayIntegrityFix/blob/main/module/action.sh)).

## FAQ
https://xdaforums.com/t/pif-faq.4653307/

## Download
https://github.com/chiteroman/PlayIntegrityFix/releases/latest

## Donations
[PayPal](https://www.paypal.com/paypalme/chiteroman0)
