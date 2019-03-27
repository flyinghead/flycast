# Reicast Modem Configuration

You need to assign an IP address from your local LAN to the Dreamcast. Do not use the address of the computer on which reicast is running. 
Add it to your `emu.cfg` file in the `[network]` section.

For example:
```
[network]
IP = 192.168.1.99
```
Make sure this address is not being used on your network. Ping it just in case...

## Windows (7)
1. Install TAP-Windows

   Navigate to [OpenVPN Community Downloads](https://openvpn.net/index.php/download/community-downloads.html) and scroll all the way to the bottom. Download and install the NDIS 6 version.
   
2. In your Network Connections you should see a new network with a TAP-Windows Adapter V9 device. 
   *Make sure it is enabled.*

3. Run `reicast.exe` as an administrator (right-click on the program and select "Run as administrator")

4. That's it! Now you need to configure internet access in the Dreamcast itself.

## Linux
All these commands must be run as root.

1. Create the IP tunnel. If your username is not joe, put your username there.
   ```
   # ip tuntap add mode tun user joe
   ``` 
   
2. Bring the interface up
   ```
   # ip link set tun0 up
   ``` 
   
3. Add a route to this IP.
   ```
   # ip route add 192.168.1.99/32 dev tun0
   ```
   Replace 192.168.1.99 by the IP set in your emu.cfg file.
   
4. Enable proxy ARP for all interfaces.
   ```
   # echo '1' >/proc/sys/net/ipv4/conf/all/proxy_arp
   ```
5. Follow up to the next section


## Dreamcast
You need to configure the ISP settings in the Dreamcast. Some games allow to do it within the game itself and will save the configuration. Other games can then use it.

You can put any name, password and phone number in the ISP settings as they are ignored. Do not change or set any other option.

Some games require a DMZ or port forwarding to be configured on your Internet router. Refer to the [Dreamcast Live web site](https://www.dreamcastlive.net/connection-guide.html) for details about each game.
