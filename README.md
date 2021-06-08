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

## Tools
Two things are necessary for the radio to obey. We need to control the volume and predict the length of a song. My radio listens to UPnP, it appears as renderer on apps like UPnPlay for Android. Device Spy from [Developer Tools for UPnP Technologies](https://www.meshcommander.com/upnptools) shows properties and methods of UPnP devices on the network. To test run device methods you'll use "Invoke Action"
![UPnP Device Spy - Invoke Action](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/media/UPnP%20Device%20Spy%20-%20Invoke%20Action.png?raw=true)

To see what's going on you need to show debug information
![UPnP Device Spy - Show Invoke Details](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/media/UPnP%20Device%20Spy%20-%20Show%20Invoke%20Details.png?raw=true)

After invoking the desired function
![UPnP Device Spy - Invoke](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/media/UPnP%20Device%20Spy%20-%20Invoke.png?raw=true)

you can look at the generated POST
![UPnP Device Spy - Show POST](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/media/UPnP%20Device%20Spy%20-%20Show%20POST.png?raw=true)

and the result returned (volume 45% in this case)
![UPnP Device Spy - Show Return data](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/media/UPnP%20Device%20Spy%20-%20Show%20Return%20data.png?raw=true)

So you see, the radio is in principle able to respond to UPnP commands. However, there are some strange things, e.g. my radio changes the volume after a SetVolume call only for about 2 seconds, then it returns to the previous volume. But if I switch the radio station via UPnP before, then changing the volume with UPnP also works. So you have to experiment a bit depending on the radio to get a working solution.

### Self-control
Now we have to teach the radio to control itself. My idea was to use UPnP calls that the radio sends to itself using a Busybox shell script. The first hurdle was to find a curl or netcat that runs on the radio and can be used to send the UPnP HTTP request messages. So what software is running on the radio in the first place?

Just ask the radio itself and then you know that there is an Arm chip at work.
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
After some searching I found a [netcat](https://github.com/Richard-Ni/arm-linux-netcat/tree/master/arm/bin) that was built for Arm (later I learned to build one myself). But you have to be careful not to use tools that overload the radio. If the memory or computational requirements become too large, the radio freezes and you have to start over. The radio has a  ``top`` that gives you an idea. Mine has about 7 MB of free memory - not really much
````
# top
Mem: 22760K used, 6892K free, 0K shrd, 0K buff, 6528K cached
CPU:  20% usr  12% sys   0% nic  66% idle   0% io   0% irq   0% sirq
Load average: 4.69 4.63 4.65 1/60 1723
  PID  PPID USER     STAT   VSZ %MEM %CPU COMMAND
 1432   379 root     S     1900   6%  21% mplayer 0 Param2
  314   313 root     R     6916  23%  10% W950OSD 3 4
 1711  1678 root     R      432   1%   1% top
 1437   379 root     S     1900   6%   1% mplayer 0 Param2
  321   319 root     S     6916  23%   0% W950OSD 3 4
  370     2 root     SW       0   0%   0% [RtmpMlmeTask]
````
I used ``$ python -m pyftpdlib -i 192.168.178.49 -w -p 2121 -d /home/me/FTP`` to set up a temporary FTP server on a PC. I have always used the /tmp folder on the radio for file exchange.
````
# ftpget -v 192.168.178.49 -P 2121 /tmp/netcat netcat
# chmod +x /tmp/netcat
# /tmp/netcat --help
GNU netcat 0.7.1, a rewrite of the famous networking tool.
Basic usages:
...
````
### Into the unknown
Now, before we cast the spell, we have to overcome one more hurdle. My radio didn't listen to itself at all and first had to be coaxed by an innocent-looking
``ifconfig lo 127.0.0.1``.
You can also check beforehand if localhost is configured for your radio by looking with ``ifconfig`` alone; that must show a ``UP LOOPBACK RUNNING``.

The spell itself can be derived from the UPnP HTTP request made by the Device Spy, see
[SetVolumeLow.xml](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/SetVolumeLow.xml). And now for the casting of the spell itself,
````
# cat /tmp/SetVolumeLow.xml  | /tmp/netcat -n -w 5  127.0.0.1  52525
````
which, if successful, returns something like this
````
# cat /tmp/SetVolumeLow.xml  | /tmp/netcat -n -w 5  127.0.0.1  52525
HTTP/1.1 200 OK
Content-Type: text/xml
Content-Length: 296

<?xml version="1.0" encoding="utf-8"?>
<s:Envelope s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:s="http://schemas.xmlsoap.org/soap/envelope/">
<s:Body>
<u:SetVolumeResponse xmlns:u="urn:schemas-upnp-org:service:RenderingControl:1"></u:SetVolumeResponse></s:Body></s:Envelope>
````
and you'll hear the volume decrease, but only briefly, then it gets louder again, at least on my radio. To actually change the volume permanently, we have to switch the radio station beforehand. This is done similarly as above with ``SetAVTransportURI``. But what is the URI we need for the desired station? If your radio comes with an internet service that lets you select favorites, then you might be able to get the URI that way. On my radio there is a ``/tmp/myradio.cfg`` that contains the radio presets. It's best to get the file via FTP because it's not a text file with easy to read content.
````
# ftpput 192.168.178.49 -v -P 2121 /tmp/myradio.cfg myradio.cfg
````
If you look at the file, at the beginning of each 366 bytes record is the name of the radio station and further at the end the URI you are looking for - provided the desired station is included in your presets. You can also use [favlist.exe](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/favlist.exe) from [kayrus](https://github.com/kayrus/iradio) to recreate the presets in [myradio.cfg](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/myradio.cfg) to your liking using a [playlist.csv](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/playlist.csv). You just have to make sure to encode an ampersand from myradio.cfg in the URI accordingly (e.g. ``id=123&sc=N9XX_AAC`` ➞ ``id=123&amp;amp;sc=N9XX_AAC``).
Thus equipped, we can now switch to the desired station and adjust the volume as desired.
````
# cat /tmp/SetChannel-1000XmasHits.xml  | /tmp/netcat -n -w 5  127.0.0.1  52525 > nul
# sleep 10
# cat /tmp/SetVolumeLow.xml  | /tmp/netcat -n -w 5  127.0.0.1  52525 > nul
````
Now the volume is actually permanently turned down, which is probably best for the station.

### Listen to the music
At first I thought I could detect (with artificial intelligence or Shazam or both) which song is playing and then turn down the volume according to its playing time. But very soon I realized that this could not be done with the resources of the radio. I might have needed another service on a server to determine the playing time. And worse - it would have taken a few (?) seconds to realize that music was now playing and I needed to reduce the volume.

So, what to do?

I found that the radio describes the currently played station in ``/tmp/playinfo.xml``. And some stations also give the artist and song title in this file, like this.
````
# cat /tmp/playinfo.xml
<stream_format>MP3 /224 Kbps</stream_format><logo_img>http://127.0.0.1:8080/playlogo.jpg</logo_img><station_info>Radio Efimera</station_info><song>Honey In Heat</song><artist>Laika</artist>#
````
For my solution to work, I need the artist and song from the file, which fortunately was the case with my station for Sunday. For your favorite station you have to find out by yourself.
But how do I get artist and song now if I don't have grep and sed? 

````
# /tmp/getq /tmp/playinfo.xml > /tmp/GET.xm
# cat /tmp/GET.xml /tmp/mbRequest.txt > /tmp/Request.txt
# cat /tmp/Request.txt | /tmp/netcat -vv -w 5  musicbrainz.org 80  
````



getpl.sh


## Limitations
* on my radio I cannot retrieve ListPresets.

## Acknowledgments
* use [MusicBrainz Search API](https://musicbrainz.org/doc/MusicBrainz_API) to determine the length of a song
* used Busybox tools from [BUSYBOX](https://busybox.net), especially make_single_applets.sh
* used musl-targeting cross-compilers made possible by [Rich Felker](https://github.com/richfelker/musl-cross-make)
* used netcat from [Richard Ni](https://github.com/Richard-Ni/arm-linux-netcat)
* used calc from [Thom Seddon](https://github.com/thomseddon/calc)
* used DeviceSpy from [Developer Tools for UPnP Technologies](https://www.meshcommander.com/upnptools)
* used favlist by [kayrus](https://github.com/kayrus/iradio) to decipher the secrets of myradio.cfg
