if [ -n "$XDG_CACHE_HOME" ]; then
    rm -rf ${XDG_CACHE_HOME}/*
    cp -pr /app/share/xtrkcad/* ${XDG_CACHE_HOME}
fi
export XTRKCADLIB=${XDG_CACHE_HOME}
# there is no access to host gtk3 modules
unset GTK3_MODULES
unset GTK_MODULES
BOOKMARK="file://$XTRKCADLIB xtrkcad-lib"
# gtk2 bookmarks
FILE="$HOME/.gtk-bookmarks"
touch $FILE
sed "/xtrkcad-lib$/d" $FILE > ${FILE}_
cp ${FILE}_ $FILE
rm ${FILE}_
echo "$BOOKMARK" >> $FILE
if [ "$1" = "-d" ]; then
    echo "ARCH=$(uname -m)"
    env | sort | grep -E "DISPLAY|XTRK|XDG"
    echo
fi
