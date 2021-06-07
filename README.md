# RadioScripting
*Use Busybox shell scripts to tell your internet radio what to do.*

One Sunday morning I was listening to a talk show on the radio and was getting more and more annoyed with the bad music that was being played between the conversations. I had to turn the volume down every few minutes and make sure I caught the end of the song to turn it up again. So I thought, this has to work better. Why can't the radio turn itself down when a song starts and turn itself up again at the end?  
Let's see.

## Access your radio
I used [Nmap](https://nmap.org) to check if the radio accepts telnet connections
````
$ nmap -p 23 --script telnet-brute.nse   192.168.178.51
   Nmap scan report for 192.168.178.51
   PORT   STATE SERVICE
   23/tcp open  telnet
   | telnet-brute:
   |   Accounts:
   |     root:password - Valid credentials
   |_  Statistics: Performed 3522 guesses in 603 seconds, average tps: 5.7
   Nmap done: 1 IP address (1 host up) scanned in 608.02 seconds
````
and it did.
Logging in showed the radio was running under BUSYBOX
````
(none) login: root
Password:

BusyBox v1.15.2 (2016-03-18 10:34:36 CST) built-in shell (ash)
Enter 'help' for a list of built-in commands.
````
Pressing ``<Tab>`` twice is better than help to show a complete list of available commands.
````
#
[               free            kill            ping            top
[[              ftpget          killall         ps              touch
busybox         ftpput          login           pwd             true
cat             gunzip          ls              rm              udhcpc
chmod           httpd           lsmod           rmdir           umount
cp              hwclock         mac             rmmod           unlzma
date            ifconfig        mdev            route           usb-storage.ko
df              ifdown          mkdir           sh              usleep
du              ifup            mount           sleep
echo            init            msh             sync
env             insmod          mt7601Usta.ko   telnetd
false           iwtools         mv              test
#
````
This seems impressive, but if you look closer, you will notice that many useful or necessary tools are missing. There is no editor, no grep, no curl, no netcat that you would expect to script the radio. Later I found out that the shell itself is also quite limited. It did not allow the evaluation of arithmetic expressions, so a counting loop is not really possible. But there is ``ftpget`` and ``ftpput`` to allow file transfer. That should actually be enough.
### Building tools


````
# cat /proc/cpuinfo
Processor       : ARM926EJ-S rev 5 (v5l)
BogoMIPS        : 119.60
Features        : swp half thumb fastmult edsp java
CPU implementer : 0x41
CPU architecture: 5TEJ
CPU variant     : 0x0
CPU part        : 0x926
CPU revision    : 5

Hardware        : W55FA93
Revision        : 0000
Serial          : 0000000000000000

````



* limitations: on my radio I cannot retrieve ListPresets.

## Acknowledgments
* use [MusicBrainz Search API](https://musicbrainz.org/doc/MusicBrainz_API) to determine the length of a song
* used Busybox tools from [BUSYBOX](https://busybox.net), especially make_single_applets.sh
* used musl-targeting cross-compilers made possible by [Rich Felker](https://github.com/richfelker/musl-cross-make)
* used netcat from [Richard Ni](https://github.com/Richard-Ni/arm-linux-netcat)
* used calc from [Thom Seddon](https://github.com/thomseddon/calc)
* used DeviceSpy from [Developer Tools for UPnP Technologies](https://www.meshcommander.com/upnptools)
* used favlist by [kayrus](https://github.com/kayrus/iradio) to decipher the secrets of myradio.cfg
