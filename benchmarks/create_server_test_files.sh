#!/bin/bash
rm -r /tmp/kavan-fuse/
mkdir -p /tmp/kavan-fuse/
for val in {1..50}
do
   echo "Test File Number $val" > server_test_files/testFile$val.txt
done
cp server_test_files/* /tmp/kavan-fuse/
