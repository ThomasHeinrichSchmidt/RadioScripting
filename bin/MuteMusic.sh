#!/bin/sh

# USAGE
# Select channel and mute each track played.
# Use this to listen to talk shows without the bad music.
#
# INSTALL
# ftpget 192.168.178.49 -P 2121 /tmp/Monitor.sh MuteMusic.sh
# chmod +x /tmp/MuteMusic.sh
# /tmp/MuteMusic.sh

savequeries=0
host=192.168.178.49
port=2121
echo getting files from $host using port $port
ftpget $host -P $port /tmp/netcat netcat
ftpget $host -P $port /tmp/grep grep
ftpget $host -P $port /tmp/getq getq
ftpget $host -P $port /tmp/calc calc
chmod +x /tmp/netcat
chmod +x /tmp/grep
chmod +x /tmp/getq
chmod +x /tmp/calc
# need local UPnP network interface 
ifconfig lo 127.0.0.1  

ftpget $host -P $port /tmp/SetChannel-1000XmasHits.xml  SetChannel-1000XmasHits.xml
ftpget $host -P $port /tmp/mbRequest.txt mbRequest.txt
ftpget $host -P $port /tmp/SetVolumeLow.xml SetVolumeLow.xml
ftpget $host -P $port /tmp/SetVolumeHigh.xml SetVolumeHigh.xml

echo switch to the desired station
sleep 1
cat /tmp/SetChannel-1000XmasHits.xml  | /tmp/netcat -n -w 5  127.0.0.1  52525 > nul
sleep 10

while [ ! -f /tmp/playinfo.xml ]  
do
   echo "waiting for /tmp/playinfo.xml to appear"
   sleep 1
done
# remember last playinfo.xml file
cp /tmp/playinfo.xml /tmp/playinfo-last.xml
if /tmp/grep -q '<station_info> 1000 Christmashits' /tmp/playinfo.xml; then echo "playing desired station"; fi
# max song length in seconds for muting
maxduration=360

while true 
do 
   # check if file 1 is newer than file 2
   if [ "/tmp/playinfo.xml" -nt "/tmp/playinfo-last.xml" ]; then
      # default song length in seconds for muting
      duration=180
      # build GET request line from current playinfo, use getq -s to swap song/artist
      /tmp/getq /tmp/playinfo.xml > /tmp/GET.xml
      # complete the rest of the request
      cat /tmp/GET.xml /tmp/mbRequest.txt > /tmp/Request.txt
      # query MusicBrainz
      cat /tmp/Request.txt | /tmp/netcat -vv -w 5  musicbrainz.org 80  > /tmp/mb-Response.txt 2>nul
      # extract length tag scaled by 1/1000 using -s
      if /tmp/grep -st length /tmp/mb-Response.txt; then 
         duration=`eval /tmp/grep -st length /tmp/mb-Response.txt`
         echo \\nMusicBrainz says $duration sec for `eval /tmp/grep -t title /tmp/mb-Response.txt` by `eval /tmp/grep -t name /tmp/mb-Response.txt`
      fi
      if [ $duration -gt  $maxduration ]; then duration=$maxduration; echo limit the duration to $duration; fi
      
      if [ $savequeries -eq 1 ]; then 
         echo saving query reports for debugging
         epoch=`eval date +"%s"`
         ftpput $host -P $port playinfo-$duration-$epoch.xml /tmp/playinfo.xml
         ftpput $host -P $port Request-$duration-$epoch.txt  /tmp/Request.txt
         ftpput $host -P $port mb-Response-$duration-$epoch.txt /tmp/mb-Response.txt
      fi
      echo now mute song for $duration sec starting at `eval date`
      cat /tmp/SetVolumeLow.xml  | /tmp/netcat -n -w 5  127.0.0.1  52525 > nul

      # remember this song
      cp /tmp/playinfo.xml /tmp/playinfo-last.xml
      # sleep for $duration seconds while checking if a new song starts early
      start=`eval date +"%s"`
      now=`eval date +"%s"`
      while [ `eval tmp/calc $now - $start` -lt  $duration ]
      do
         if [ ! "/tmp/playinfo.xml" -nt "/tmp/playinfo-last.xml" ]; then
            sleep 1
         else
            echo new song is starting at `eval date` while last song still muted
            break
         fi
         now=`eval date +"%s"`
      done

      elapsed=`eval tmp/calc $now - $start`
      if  [ $elapsed -ge  $duration ]; then
         # duration has expired
         echo restore volume after $elapsed sec at `eval date`
         cat /tmp/SetVolumeHigh.xml  | /tmp/netcat -n -w 5  127.0.0.1  52525 > nul
      fi
   fi
   sleep 1
done


