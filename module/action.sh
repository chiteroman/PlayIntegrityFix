MODDIR=${0%/*}
curl -o $MODDIR/pif.json https://raw.githubusercontent.com/chiteroman/PlayIntegrityFix/refs/heads/main/module/pif.json
killall -9 com.google.android.gms.unstable
