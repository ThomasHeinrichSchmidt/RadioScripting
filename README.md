# RadioScripting
Use Busybox shell scripts to tell your internet radio what to do

* limitations: on my radio I cannot retrieve ListPresets.

## Acknowledgments
* use [MusicBrainz Search API](https://musicbrainz.org/doc/MusicBrainz_API) to determine the length of a song 
* used Busybox tools from [BUSYBOX](https://busybox.net), especially make_single_applets.sh
* used musl-targeting cross-compilers made possible by [Rich Felker](https://github.com/richfelker/musl-cross-make)
* used netcat from [Richard Ni](https://github.com/Richard-Ni/arm-linux-netcat)
* used calc from [Thom Seddon](https://github.com/thomseddon/calc)
* used DeviceSpy from [Developer Tools for UPnP Technologies](https://www.meshcommander.com/upnptools)
* used favlist by [kayrus](https://github.com/kayrus/iradio) to decipher the secrets of myradio.cfg
