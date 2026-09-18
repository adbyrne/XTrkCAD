FP_BASENAME=$(echo $XTRKCAD_APP_ID | awk -F "." '{ print $NF;}')
if [ -n "$XDG_CACHE_HOME" ]; then
    cp -pr /app/share/$FP_BASENAME/* ${XDG_CACHE_HOME}
    # versioning makes the app lib dir variable dynamic
    FP_BASEENV=$(echo ${FP_BASENAME} | tr '[a-z]-' '[A-Z]_')
    eval "export ${FP_BASEENV}LIB=${XDG_CACHE_HOME}"
    eval "export ${FP_BASEENV}BETALIB=${XDG_CACHE_HOME}"
else
    echo "WARNING: no XDG_CACHE_HOME set for flatpak affecting help and params"
fi
