#!/bin/bash

# stop script on error and print it
set -e
# inform me of undefined variables
set -u
# handle cascading failures well
set -o pipefail

SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PROJECT_DIR=$(dirname $( dirname $SCRIPT_DIR ))

STACK_SIZE=1
PATIENCE=5
CPU_CORES=`sysctl hw.ncpu`
CPU_CORES="${CPU_CORES: -1}"
CPU_CORES=$((CPU_CORES+1))
echo "Commencing Grid Search"
echo "* running on ${CPU_CORES} cores"
DATA_DIR="${PROJECT_DIR}/data/sentiment/"
PROGRAM="${PROJECT_DIR}/build/examples/lstm_sentiment"

if [ "$#" -ne 1 ]; then
    echo "usage: $0 [results_dir] "
    exit
fi

if [ ! -d "$1" ]; then
    echo "Could not find directory \"$1\""
    exit
fi

if [ ! -f "${DATA_DIR}train.txt" ]; then
    echo "Training data not present. Downloading it now."
    python3 ${DATA_DIR}generate.py
fi

function ensure_dir {
    if [ "${1: -1}" != "/" ]; then
        echo "${1}/"
    else
        echo $1
    fi
}

SAVE_FOLDER="$(ensure_dir $1)saved_models"
RESULTS_FILE="$(ensure_dir $1)results.txt"
BASE_FLAGS="--results_file=${RESULTS_FILE}"
BASE_FLAGS="${BASE_FLAGS} --save_location=${SAVE_FOLDER}/model --stack_size=${STACK_SIZE} --patience=${PATIENCE} -epochs=2000 -j=${CPU_CORES} --fast_dropout --noshortcut "
BASE_FLAGS="${BASE_FLAGS} --train=${DATA_DIR}train.txt "
BASE_FLAGS="${BASE_FLAGS} --validation=${DATA_DIR}dev.txt "
BASE_FLAGS="${BASE_FLAGS} --test=${DATA_DIR}test.txt "
echo "* saving results to ${RESULTS_FILE}"

# start out clean
if [ -f $RESULTS_FILE ]; then
    rm $RESULTS_FILE
fi

# create the results file
touch $RESULTS_FILE

# build the save folder
if [ ! -d "$SAVE_FOLDER" ]; then
    mkdir $SAVE_FOLDER
fi

# change the variance of the gradients using minibatch sizes:
for minibatch in 2 50 100
do
    # models keeps improving at these hidden sizes:
    for hidden in 300 350 400
    do
        # higher dropout values are subpar
        for dropout in 0.05 0.1 0.2 0.3 0.4
        do
            # previously saved models are no longer useful for this grid tile
            rm -rf $SAVE_FOLDER/*
            $PROGRAM $BASE_FLAGS --hidden $hidden --dropout $dropout --minibatch $minibatch
        done
    done
done