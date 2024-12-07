resetprop -d persist.log.tag.LSPosed
resetprop -d persist.log.tag.LSPosed-Bridge
su -c killall com.google.android.gms
su -c killall com.google.android.gms.unstable

echo "Gms and Play Integrity API restarted."
