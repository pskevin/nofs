base="/var/tmp/nofs"
mount="$base[mount]"
cache="$base[cache]"

echo "Using $mount as mount directory."
echo "Using $cache as cache directory."

fusermount -uz $mount
rm -rf $cache
mkdir -p $cache
./src/nofs 0 pskevin@indus.cs.utexas.edu $base $mount $cache