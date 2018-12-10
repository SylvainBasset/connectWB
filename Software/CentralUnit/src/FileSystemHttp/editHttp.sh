
mntPath='/media/sylv/FsHttpEdit'
echo $mntPath

sudo mkdir $mntPath
sudo mount -o loop,umask=000,offset=65536 FsHttp.img $mntPath

xdg-open $mntPath

echo ""
echo "Http filesystem mounted"
echo "-- Press any key to unmount --"
read -n1 -s

sudo umount $mntPath

echo ""
echo "Copying file to apache server"
echo "-- Press any key to finish --"
read -n1 -s

sudo cp FsHttp.img /var/www/html