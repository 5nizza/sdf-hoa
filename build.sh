#!/bin/bash

set -e  # stop on error

# some colors for printing
GREEN='\033[1;32m'
NC='\033[0m' # No Color

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR=$SCRIPT_DIR/syntcomp-build/
TP_DIR=$SCRIPT_DIR

export SDF_TP=$SCRIPT_DIR/third_parties/

echo -e "${GREEN}building cudd..${NC}"
cd $SDF_TP/cudd-3.0.0/
./configure --enable-obj
make -j4

echo -e "${GREEN}building spot..${NC}"
cd $SDF_TP/spot-2.5.3/
./configure --prefix=$SDF_TP/spot-install-prefix/
make -j4
make install

echo -e "${GREEN}building modified aiger (it is not really needed)..${NC}"
cd $SDF_TP/aiger-1.9.4/
make

echo -e "${GREEN}building myself..${NC}"
rm -rf $BUILD_DIR
mkdir $BUILD_DIR
cd $BUILD_DIR

cmake $SCRIPT_DIR
make -j4 sdf-hoa-opt

cd $SCRIPT_DIR

rm -rf binary
mkdir binary
ln -s $BUILD_DIR/src/sdf-hoa-opt $SCRIPT_DIR/binary/sdf-hoa

echo
echo -e "${GREEN}executable was created in binary/${NC}"

# remove intermediate compilation files of spot to save space
# (this _keeps_ the installed binaries and libs)
cd $SDF_TP/spot-2.5.3/
make clean
