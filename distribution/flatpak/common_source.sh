if [ -n "$XDG_CACHE_HOME" ]; then
    cp -pr /app/share/xtrkcad/* ${XDG_CACHE_HOME}
fi
export XTRKCADLIB=${XDG_CACHE_HOME}
