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
This seems impressive, but if you look closer, you will notice that many useful or necessary tools are missing. There is no editor, no grep, no curl, no netcat that you would expect to script the radio. And - worst of all - no apt-get to install anything afterwards. Later I found out that the shell itself is also quite limited. It did not allow the evaluation of arithmetic expressions, so a counting loop is not really possible. But there is ``ftpget`` and ``ftpput`` to allow file transfer. In the end, that should actually be enough.

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
After some searching I found a [netcat](https://github.com/Richard-Ni/arm-linux-netcat/tree/master/arm/bin) that was built for Arm (later I learned to build one myself). But you have to be careful not to use tools that overload the radio. If the memory or computational requirements become too large, the radio freezes and you have to start over.
The radio has a  ``top`` that gives you an idea. Mine has about 7 MB of free memory - not really much
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
By the way, every time after the radio was turned off, it restores its original configuration, so you have to reinstall everything again.

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
and you'll hear the volume decrease, but only briefly, then it gets louder again, at least on my radio. To actually change the volume permanently, we have to switch the radio station beforehand. This is done similarly as above with ``SetAVTransportURI``. But what is the URI we need for the desired station? If your radio comes with an internet service that lets you select favorites, then you might be able to get the URI that way. On my radio there is a ``/tmp/myradio.cfg`` that contains the radio presets. It's best to retrieve the file via FTP because it's not a text file with easy to read content.
````
# ftpput 192.168.178.49 -v -P 2121 /tmp/myradio.cfg myradio.cfg
````
If you look at the file, at the beginning of each 366 bytes record is the name of the radio station and further at the end the URI you are looking for - provided the desired station is included in your presets. You can also use [favlist.exe](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/favlist.exe) from [kayrus](https://github.com/kayrus/iradio) to recreate the presets in [myradio.cfg](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/myradio.cfg) to your liking using a [playlist.csv](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/playlist.csv). You just have to make sure to encode an ampersand from myradio.cfg in the URI accordingly (e.g. ``id=123&sc=N9XX_AAC`` ➞ ``id=123&amp;amp;sc=N9XX_AAC``).
Thus equipped, we can now switch to the desired station and adjust the volume as desired (see [SetChannel-1000XmasHits](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/SetChannel-1000XmasHits.xml)).
````
# cat /tmp/SetChannel-1000XmasHits.xml  | /tmp/netcat -n -w 5  127.0.0.1  52525 > nul
# sleep 10
# cat /tmp/SetVolumeLow.xml  | /tmp/netcat -n -w 5  127.0.0.1  52525 > nul
````
Now the volume is actually permanently turned down, which is probably best for the station anyway.

### Listen to the music
At first I thought I could detect (with artificial intelligence or Shazam or both) which song is playing and then turn down the volume according to its playing time. But very soon I realized that this could not be done with the resources of the radio. I might have had to use a service on a separate server to determine playing time. And worse - it would have taken a few (?) seconds to realize that music was now playing and I needed to reduce the volume.

So, what to do?

I found that the radio describes the currently played station in ``/tmp/playinfo.xml``. And some stations also give the artist and song title in this file, like this.
````
# cat /tmp/playinfo.xml
<stream_format>MP3 /224 Kbps</stream_format><logo_img>http://127.0.0.1:8080/playlogo.jpg</logo_img><station_info>Radio Efimera</station_info><song>Honey In Heat</song><artist>Laika</artist>#
````
For my solution to work, the artist and song must be in the file, which fortunately was the case with my station for Sunday. For your favorite station you have to find out by yourself.
But how can I process artist and song if I don't have grep or sed?
And - moreover, who now tells me the corresponding playing time to artist and song?
Fortunately for me, there are [services](https://en.wikipedia.org/wiki/List_of_online_music_databases) on the Internet that catalog music recordings and also make that publicly available via an API. I chose [MusicBrainz](https://musicbrainz.org/search?query=Winter+Wonderland+Aretha+Franklin&type=recording&method=indexed) in the end, not because it's possibly the best service, but because I understood how to use it the most. The API call for use with netcat fits into a [small file](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/MusicBrainz.xml), with artist and song specified on the first line.

````
GET  /ws/2/recording?query=Winter+Wonderland%20AND%20artist:Aretha+Franklin&type:song&limit=1  HTTP/1.1
````
All I needed now was a tool (I called it [getq](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/src/getq.c)) that would turn artist and song from playinfo.xml into a GET for MusicBrainz. In principle not very difficult, but how do I get this to work on the radio? Luckily [Rich Felker](https://github.com/richfelker/musl-cross-make) came to my rescue, who has already prepared everything:

````
$  git clone git://github.com/richfelker/musl-cross-make.git
        -- in config.mak set ==> TARGET = arm-linux-musleabi
$  make           -- runs for two hours
$  make install
````
With this you are able to build [getq](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/getq).
````
$  ~/Dev/musl/musl-cross/musl-cross-make/output/bin/arm-linux-musleabi-gcc -Wall -g -static getq.c -o getq
````


Together with a fixed template [mbRequest.txt](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/mbRequest.txt) file, we can now send the request to MusicBrainz.

````
# /tmp/getq /tmp/playinfo.xml > /tmp/GET.xml
# cat /tmp/GET.xml /tmp/mbRequest.txt > /tmp/Request.txt
# cat /tmp/Request.txt | /tmp/netcat -vv -w 5  musicbrainz.org 80  
````
And that finally gives us the longed for playing time.
````
Warning: Inverse name lookup failed for `138.201.227.205'
musicbrainz.org [138.201.227.205] 80 open
HTTP/1.1 200 OK
Date: Wed, 09 Jun 2021 09:48:56 GMT
Content-Type: application/xml; charset=UTF-8
Content-Length: 2564
Connection: close
Vary: Accept-Encoding
X-RateLimit-Limit: 1200
X-RateLimit-Remaining: 1187
X-RateLimit-Reset: 1623232138
Last-Modified: Wed, 09 Jun 2021 08:39:08 GMT
ETag: "MmJkMmRlMDAwMDAwMDAwMFNvbHI="
X-Cache-Status: MISS
Access-Control-Allow-Origin: *

<?xml version="1.0" encoding="UTF-8" standalone="yes"?><metadata created="2021-06-09T09:48:56.313Z" xmlns="http://musicbrainz.org/ns/mmd-2.0#" xmlns:ns2="http://musicbrainz.org/ns/ext#-2.0"><recording-list count="10" offset="0"><recording id="c66fe1bf-939c-494a-be9f-9cef55e25696" ns2:score="100"><title>Winter Wonderland</title><length>134066</length><artist-credit><name-credit><name>Aretha Franklin</name><artist id="2f9ecbed-27be-40e6-abca-6de49d50299e"><name>Aretha Franklin</name><sort-name>Franklin, Aretha</sort-name><alias-list><alias sort-name="Franklin, Aretha">Franklin, Aretha</alias><alias sort-name="Aretha Fanklin">Aretha Fanklin</alias><alias sort-name="Franklin, Aretha Louise" type="Legal name" type-id="d4dcd0c0-b341-3612-a332-c0ce797b25cf">Aretha Louise Franklin</alias><alias sort-name="Arthaa Franklin">Arthaa Franklin</alias><alias sort-name="Aretha Franklyn">Aretha Franklyn</alias><alias sort-name="Aretja Franklin">Aretja Franklin</alias></alias-list></artist></name-credit></artist-credit><first-release-date>2002</first-release-date><release-list><release id="5bc1c73e-7155-4b45-9bd6-f5fbfc11901f"><title>Making Spirits Bright</title><status id="518ffc83-5cde-34df-8627-81bff5093d92">Promotion</status><artist-credit><name-credit><name>Various Artists</name><artist id="89ad4ac3-39f7-470e-963a-56509c546377"><name>Various Artists</name><sort-name>Various Artists</sort-name><disambiguation>add compilations to this artist</disambiguation></artist></name-credit></artist-credit><release-group id="cb0c5e60-5c3c-4756-ac8c-80cb76ed2b0d" type="Compilation" type-id="dd2a21e1-0c00-3729-a7a0-de60b84eb5d1"><title>Making Spirits Bright</title><primary-type id="f529b476-6e62-324f-b0aa-1f3e33d313fc">Album</primary-type><secondary-type-list><secondary-type id="dd2a21e1-0c00-3729-a7a0-de60b84eb5d1">Compilation</secondary-type></secondary-type-list></release-group><date>2002</date><country>CA</country><release-event-list><release-event><date>2002</date><area id="71bbafaa-e825-3e15-8ca9-017dcad1748b"><name>Canada</name><sort-name>Canada</sort-name><iso-3166-1-code-list><iso-3166-1-code>CA</iso-3166-1-code></iso-3166-1-code-list></area></release-event></release-event-list><medium-list count="1"><track-count>10</track-count><medium><position>1</position><format>CD</format><track-list count="10" offset="7"><track id="57748cb4-48fa-4716-9789-f1da97391eb1"><number>8</number><title>Winter Wonderland</title><length>134066</length></track></track-list></medium></medium-list></release></release-list></recording></recording-list></metadata>Total received bytes: 2953
Total sent bytes: 398
````
OK, this is a bit confusing, but with a homemade little [grep](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/grep) you can easily get the playing time of 134 seconds from it, hidden in ``<length>134066</length>``.

````
# cat /tmp/Request.txt | /tmp/netcat -vv -w 5  musicbrainz.org 80  > /tmp/Response.txt
# /tmp/grep -st length /tmp/Response.txt
134
````
### Put everything together
Now we have almost everything together to really tell the radio what to do. All that's missing is a tool with which to calculate the playing times, like this
````
# /tmp/calc 134 - 7
127
````
I used [calc](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/calc) from [Thom Seddon](https://github.com/thomseddon/calc) for this, which can do more than enough for my needs.

Check out my script [MuteMusic.sh](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/MuteMusic.sh) at work:

````
# ftpget 192.168.178.49 -P 2121 /tmp/MuteMusic.sh MuteMusic.sh
# chmod +x /tmp/MuteMusic.sh
# /tmp/MuteMusic.sh
getting files from 192.168.178.49 using port 2121
switch to the desired station
now mute song for 180 sec starting at Wed Jun 9 15:34:11 UTC 2021
new song is starting at Wed Jun 9 15:34:23 UTC 2021 while last song still muted
198
MusicBrainz says 198 sec for Blue Christmas by The Brian Setzer Orchestra
now mute song for 198 sec starting at Wed Jun 9 15:34:25 UTC 2021
new song is starting at Wed Jun 9 15:37:07 UTC 2021 while last song still muted
260
MusicBrainz says 260 sec for Little Drummer Boy by Josh Groban
now mute song for 260 sec starting at Wed Jun 9 15:37:09 UTC 2021
...
...
````
Of course, this requires that the station sends the corresponding ``/tmp/playinfo.xml`` in time for the start of a new song. Furthermore, my script simply turns down the volume for 3 minutes if it can't determine the length of the song, for example when the station announces news.

### More Tools
While I was working hard to get all the tools together, I had been looking at Busybox on and off, but only learned how to make individual components work for my radio pretty much at the end. If you know how to do it, it's not difficult either. You need to download the [BusyBox sources](https://busybox.net/downloads/) in the desired version, I used version 1.15.2, matching my radio. The [cross-compiler](https://github.com/richfelker/musl-cross-make) must be installed, in my case in ``~/Dev/musl/musl-cross/musl-cross-make/output/bin``. First we have to build BusyBox itself.
````
export PATH=$PATH:/home/thomas/Dev/musl/musl-cross/musl-cross-make/output/bin
make CROSS_COMPILE=arm-linux-musleabi- menuconfig
make CROSS_COMPILE=arm-linux-musleabi-
````

Thus, we are able to create any BusyBox tool using the ``make_single_applets.sh`` script provided by BusyBox. We only need to change the make options by setting ``makeopts="-j9 CROSS_COMPILE=arm-linux-musleabi- LDFLAGS=--static"`` inside the script first. Then it is easy to build the world famous editor ED.

````
$ ./make_single_applets.sh ED
Making ED...
Failures: 0
$ file busybox_ED
busybox_ED: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), statically linked, stripped
````

### Where to go from here?
If you're a music lover, you might want to do the exact opposite: listen only to the music and tune out the goofy talking. This is quite simple, all you need to do is swap the desired volume values in [SetVolumeHigh.xml](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/SetVolumeHigh.xml) and [SetVolumeLow.xml](https://github.com/ThomasHeinrichSchmidt/RadioScripting/blob/main/bin/SetVolumeLow.xml). However, there may be songs that you don't want to listen to at all, or some that you absolutely have to listen to as loud as possible.

Then it's time to do something about it yourself and modify the whole thing accordingly.


## Limitations
* On my radio I can't get the list of presets (``ListPresets``) via UPnP, that would be nice to change the station automatically for example if the music on the current station gets too bad.

## Some devices with (possibly) matching software
- [ ] ABEO AB-H2003 Internet Radio
- [ ] Albrecht DR 460 C Internet Radio
- [ ] Albrecht DR 461 Internet Radio
- [ ] Albrecht DR 463 Internet Radio
- [ ] Asus AIR u3445
- [x] auna Areal Bar Connect Soundbar
- [x] auna KC3-Connect 100 WL-v2
- [x] auna Radio Quarz 150
- [x] auna Silver Star
- [ ] DUAL IR 11 WLAN Internet Radio
- [x] Hama Wireless LAN Internet-Tuner IR210
- [ ] HDigit Orbiter
- [ ] Hipshing IR608n
- [ ] Imperial 22-235-00 Dabman i200 Internet / DAB+ Radio
- [ ] Imperial 22-241-00 Dabman i400 Internet / DAB+ Radio
- [ ] Imperial i110 black
- [ ] Majority Homerton
- [ ] Majority Pembroke Internet Radio
- [ ] Majority Robinson Internet Radio
- [ ] Majority Wolfson Internet Radio
- [x] Netmus NS120
- [x] NOXON iRadio Internet Radio
- [ ] Oakcastle Internet Radio
- [ ] oneConcept Streamo stereo system with internet radio
- [ ] oneConcept TuneUp Internet Radio
- [ ] Peterhouse Colours oak
- [ ] Reflexion Design Internet Radio HRA19INT
- [ ] Sencor SIR 6000WDB WiFi Internet Radio
- [ ] TechniSat Star Radio IR 1
- [ ] Xoro HMT 500 - Micro System Internet / DAB+ / FM Radio
- [ ] Xoro »DAB 150 IR« Digitalradio (DAB)

## Acknowledgments
* use [MusicBrainz Search API](https://musicbrainz.org/doc/MusicBrainz_API) to determine the length of a song
* used Busybox tools from [BUSYBOX](https://busybox.net), especially ``make_single_applets.sh``
* used musl-targeting cross-compilers made possible by [Rich Felker](https://github.com/richfelker/musl-cross-make)
* used netcat from [Richard Ni](https://github.com/Richard-Ni/arm-linux-netcat)
* used calc from [Thom Seddon](https://github.com/thomseddon/calc)
* used DeviceSpy from [Developer Tools for UPnP Technologies](https://www.meshcommander.com/upnptools)
* used favlist by [kayrus](https://github.com/kayrus/iradio) to decipher the secrets of myradio.cfg
