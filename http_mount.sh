base="/var/tmp/nofs"
mount="$base[mount]"
cache="$base[cache]"

echo "Using $mount as mount directory."
echo "Using $cache as cache directory."

fusermount -uz $mount
rm -rf $cache
mkdir -p $cache
./src/nofs 1 128.83.139.250:8000 $base $mount $cache