# OpenSky Daemon
This is the OpenSky Daemon (OpenSkyD), which serves as an intermediate
component between an ADSB receiver and the OpenSky Network, relaying ADS-B
frames to the OpenSky collector server.
It is supposed to run on the user side.

OpenSkyD has the following features:
* Read ADS-B frames from the radarcape cape and relay them to the server
* Buffer frames in case of a network failure
* Configure the FPGA at startup
* Trigger the Watchdog
* Gather statistics
* Optionally: read frames from rcd instead of reading them directly from the
  cape
* Execute important commands, such as updating and a reverse SSH shell
