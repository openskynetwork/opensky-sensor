# OpenSky Daemon
This is the Opensky Daemon (OpenSkyD), a program running on a radarcape,
relaying ADS-B frames to the OpenSky collector server.
It is meant as a replacement for rcd, although there is a possiblity for
interwork with rcd.

OpenSkyD has the following features:
* Read ADS-B frames from the radarcape cape and relay them to the server
* Buffer frames in case of a network failure
* Configure the FPGA at startup
* Trigger the Watchdog
* Gather statistics
* Optionally: read frames from rcd instead of reading them directly from the
  cape
* Execute important commands, such as updating and a reverse SSH shell
