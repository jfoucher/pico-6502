rm -f a.out
/home/pi/Downloads/6502/vasm/vasm6502_oldstyle -ce02 -L Listing.txt -Fbin -dotdir helloworld.s
hexdump -C a.out
