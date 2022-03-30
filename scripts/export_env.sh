export ABC=$(realpath build/logic_synthesis-rs/bin/abc)
export DE=$(realpath build/logic_synthesis-rs/bin//de)
LSORACLE=build/logic_synthesis-rs/bin/lsoracle
if [ -f "$LSORACLE" ]; then
    export LSORACLE=$(realpath $LSORACLE)
else
    export LSORACLE="Don't care"
fi
