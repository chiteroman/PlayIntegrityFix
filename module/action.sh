MODPATH="${0%/*}"

# ensure not running in busybox ash standalone shell
set +o standalone
unset ASH_STANDALONE

echo -e "Staring script..."

sh $MODPATH/autopif.sh || exit 1

echo -e "All set!"