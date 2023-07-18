directoryx="$(dirname -- $(readlink -fn -- "$0"; echo x))"
directory="${directoryx%x}"

/home/pi/Downloads/6502/bintomon/bintomon -l 0x1000 -r 0x1000 $directory/a.out > $directory/a.mon
