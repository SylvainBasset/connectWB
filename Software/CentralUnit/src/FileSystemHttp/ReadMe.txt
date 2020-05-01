To update http files in Wallybox :

cd FileSystemHttp
./BuildAndPost.sh (update outfile.img and copy it to apach server location)

Open WallyTerm
Send the command :
$01:AT+S.HTTPDFSUPDATE=172.22.69.2,/outfile.img

($01:AT+S.HTTPDFSUPDATE=192.168.1.33,/outfile.img)