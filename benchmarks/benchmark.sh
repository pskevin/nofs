#!/bin/bash

transport=$1
dir=$2

if [[ $2 == "nofs" ]]
then
  dir="/var/tmp/nofs[mount]"
elif [[ $2 == "nfs" ]]
then
  dir="/u/pskevin"
else
  echo "No root directory defined!"
  exit 1
fi

echo "[bench] Using $dir directory"


echo "Running small_read.o"
out=$2"_"$transport"_small_read.out"
echo "Output in $out"
./small_read.o $dir > $out


echo "Running unsynced_writes.o"
out=$2"_"$transport"_unsynced_writes.out"
echo "Output in $out"
./unsynced_writes.o $dir > $out

echo "Running synced_writes.o"
out=$2"_"$transport"_synced_writes.out"
echo "Output in $out"
./synced_writes.o $dir > $out
