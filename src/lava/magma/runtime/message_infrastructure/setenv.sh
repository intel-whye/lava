SCRIPTPATH=$(cd `dirname -- $BASH_SOURCE` && pwd)
export PYTHONPATH="${SCRIPTPATH}/build:${SCRIPTPATH}:$PYTHONPATH"
export LD_LIBRARY_PATH="${SCRIPTPATH}/build:${SCRIPTPATH}:$LD_LIBRARY_PATH"