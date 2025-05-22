# Play Integrity Fix

## WARNING
Google has removed its legacy checks for Android 13 and above; now the Device verdict is the same as the old Strong verdict.
You will now need a valid keybox and to use the TrickyStore module if you want to pass at least the Device verdict.

If you are on Android 12 or lower, you should be safe for now.

A somewhat unstable solution exists: you can use PlayIntegrityFork and enable spoofVendingSdk. This should force the use of legacy checks on modern devices; however, the Play Store sometimes fails, including crashes.

I recommend that if you are not a very experienced user and absolutely need your device to be certified, install the stock ROM and lock the bootloader.

Issues and discussions will be closed for a few days.
