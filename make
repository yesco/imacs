echo "#define OTA 1" > esp-imacs.c
xxd -i README.md >> esp-imacs.c
cat imacs.c >> esp-imacs.c
mv imacs.c imacs.ccc
source ../add-xtensa-to-path && make
mv imacs.ccc imacs.c

