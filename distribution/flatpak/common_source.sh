if [ -n "$XDG_CACHE_HOME" ]; then
    cp -pr /app/share/xtrkcad-gtk/* ${XDG_CACHE_HOME}
fi
export XTRKCADLIB=${XDG_CACHE_HOME}
export XTRKCAD_GTKLIB=${XDG_CACHE_HOME}
