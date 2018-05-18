#!/bin/bash


SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR=$SCRIPT_DIR/syntcomp-build/
TP_DIR=$SCRIPT_DIR

export SDF_TP=$SCRIPT_DIR/third_parties/

echo "building cudd.."
cd $SDF_TP/cudd-3.0.0/
./configure --enable-obj
make -j4

echo "building modified aiger.."
cd $SDF_TP/aiger-1.9.4/
./configure
make -j4

echo "building me.."
rm -rf $BUILD_DIR
mkdir $BUILD_DIR 
cd $BUILD_DIR 

cmake $SCRIPT_DIR
make -j4 sdf-opt

cd $SCRIPT_DIR

rm -rf binary
mkdir binary
ln -s $BUILD_DIR/src/sdf-opt ./binary/sdf

echo
echo "executable was created in binary/"
echo "run it: sdf-opt <input_aiger_file>"
