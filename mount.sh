base="/var/tmp/nofs"
mount="$base[mount]"
cache="$base[cache]"

echo "Using $mount as mount directory."
echo "Using $cache as cache directory."

if [[ $1 == "scp" ]]
then
  transport=0
  host="pskevin@indus.cs.utexas.edu"
elif [[ $1 == "http" ]]
then
  transport=1 
  host="128.83.139.250:8000"
else
  echo "No switch of transports defined!"
  exit 1
fi

echo "Using $1 on $host"

fusermount -uz $mount
rm -rf $cache
mkdir -p $cache
./src/nofs $transport $host $base $mount $cache

cd ./benchmarks
./benchmark.sh $1 nofs

cd ..
pwd

fusermount -uz $mount
rm -rf $cache
mkdir -p $cache
./src/nofs $transport $host $base $mount $cache

cd ./benchmarks
./benchmark.sh $1 nfs

cd ..
pwd