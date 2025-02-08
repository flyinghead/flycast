#!/usr/bin/env python
#dreampi.py_version=202402202004
# from __future__ import absolute_import
# from __future__ import print_function
import atexit
# from typing import List, Optional, Tuple
import serial
import socket
import os
import logging
import logging.handlers
import sys
import time
import subprocess
import sh
import signal
import re
import config_server
import iptc
import select
import requests

from dcnow import DreamcastNowService
from port_forwarding import PortForwarding

from datetime import datetime, timedelta
def updater():

    if os.path.isfile("/boot/noautoupdates.txt") == True:
        logger.info("Dreampi script auto updates are disabled")
        return
    netlink_script_url = "https://raw.githubusercontent.com/eaudunord/Netlink/latest/tunnel/netlink.py"
    xband_script_url = "https://raw.githubusercontent.com/eaudunord/Netlink/latest/tunnel/xband.py"
    checkScripts = [netlink_script_url,xband_script_url]
    restartFlag = False
    for script in checkScripts:
        url = script
        try:
            r=requests.get(url, stream = True)
            r.raise_for_status()
            for line in r.iter_lines():
                if b'_version' in line: 
                    upstream_version = str(line.decode().split('version=')[1]).strip()
                    break
            local_script = "/home/pi/dreampi/"+script.split("/")[-1]
            if os.path.isfile(local_script) == False:
                local_version = None
            else:
                with open(local_script,'rb') as f:
                    for line in f:
                        if b'_version' in line:
                            local_version = str(line.decode().split('version=')[1]).strip()
                            break
            if upstream_version == local_version:
                logger.info('%s Up To Date' % local_script)
            else:
                r = requests.get(url)
                r.raise_for_status()
                with open(local_script,'wb') as f:
                    f.write(r.content)
                logger.info('%s Updated' % local_script)
                if local_script == "dreampi.py":
                    os.system("sudo chmod +x dreampi.py")
                restartFlag = True
            
        except requests.exceptions.HTTPError:
            logger.info("Couldn't check updates for: %s" % local_script)
            continue

        except requests.exceptions.SSLError:
            logger.info("SSL error while checking for updates. System time may need to be synced")
            return

    if restartFlag:
        logger.info('Updated. Rebooting')
        os.system("sudo reboot")

DNS_FILE = "https://dreamcast.online/dreampi/dreampi_dns.conf"


logger = logging.getLogger("dreampi")
first_run = 1

def check_internet_connection():
    """ Returns True if there's a connection """

    IP_ADDRESS_LIST = [
        "1.1.1.1",  # Cloudflare
        "1.0.0.1",
        "8.8.8.8",  # Google DNS
        "8.8.4.4",
        "208.67.222.222",  # Open DNS
        "208.67.220.220",
    ]

    port = 53
    timeout = 3

    for host in IP_ADDRESS_LIST:
        try:
            socket.setdefaulttimeout(timeout)
            socket.socket(socket.AF_INET, socket.SOCK_STREAM).connect((host, port))
            return True
        except socket.error:
            pass
    else:
        logger.exception("No internet connection")
        return False


def restart_dnsmasq():
    subprocess.call("sudo service dnsmasq restart".split())


def update_dns_file():
    """
        Download a DNS settings file for the DreamPi configuration (avoids forwarding requests to the main DNS server
        and provides a backup if that ever goes down)
    """
    # check for a remote configuration
    try:
        response = requests.get(DNS_FILE)
        response.raise_for_status()
    except requests.exceptions.HTTPError:
        logging.info(
            "Did not find remote DNS config; will use upstream"
        )
        return
    except requests.exceptions.Timeout:
        logging.info(
            "Request timed out; will use upstream"
        )
        return
    except requests.exceptions.SSLError:
        logging.info(
            "SSL error; will use upstream"
        )
        return

    # Stop the server
    subprocess.check_call("sudo service dnsmasq stop".split())

    # Update the configuration
    try:
        with open("/etc/dnsmasq.d/dreampi.conf", "w") as f:
            f.write(response.read())
    except IOError:
        logging.exception("Found remote DNS config but failed to apply it locally")

    # Start the server again
    subprocess.check_call("sudo service dnsmasq start".split())


# Update dreampi.py if file exists in /boot
def dreampi_py_local_update():
    if os.path.isfile("/boot/dpiupdate.py") == False:
        logger.info("No update file is found in /boot")
        return

    os.system("sudo mv /boot/dpiupdate.py /home/pi/dreampi/dreampi.py")
    os.system("sudo chown pi:pi /home/pi/dreampi/dreampi.py")
    os.system("sudo chmod +x /home/pi/dreampi/dreampi.py")
    logger.info('Updated the dreampi.py from /boot/dpiupdate.py ... Rebooting')
    os.system("sudo reboot")

# Increase the TTL in the IP HDR from 30 to 64
def add_increased_ttl():
    table = iptc.Table(iptc.Table.MANGLE)
    chain = iptc.Chain(table, "PREROUTING")

    rule = iptc.Rule()
    rule.in_interface = "ppp0"
    rule.create_target("TTL").ttl_set = str(64)

    chain.insert_rule(rule)

    logger.info("DC TTL increased from 30 to 64")
    return rule

def remove_increased_ttl(ttl_rule):
    if ttl_rule:
        table = iptc.Table(iptc.Table.MANGLE)
        chain = iptc.Chain(table, "PREROUTING")
        chain.delete_rule(ttl_rule)
        logger.info("DC TTL removed")

# Add additional DNAT rules
def start_dnat_rules():
    rules = []

    def fetch_replacement_ips():
        url = "https://shumania.ddns.net/dnat.txt"
        try:
            r = requests.get(url, verify=False)
            r.raise_for_status()
            return r.text.strip()
        except requests.exceptions.HTTPError:
            logging.info(
            "HTTP error; will skip adding DNAT rules"
            )
            return None
        except requests.exceptions.Timeout:
            logging.info(
            "Request timed out; will skip adding DNAT rules"
            )
            return None
        except requests.exceptions.SSLError:
            logging.info(
            "SSL error; will skip adding DNAT rules"
            )
            return None

    data = fetch_replacement_ips()

    if data is None:
        logger.info("No DNAT rules added")
        return None

    for ips in data.splitlines():
        ip = ips.split()
        
        if ip[0] is None:
            logger.info("Missing SRC in DNAT rule - SKIP")
            return None

        if ip[1] is None:
            logger.info("Missing DST in DNAT rule - SKIP")
            return None
 
        table = iptc.Table(iptc.Table.NAT)
        chain = iptc.Chain(table, "PREROUTING")

        rule = iptc.Rule()
        rule.protocol = "tcp"
        rule.dst = ip[0]
        rule.create_target("DNAT")
        rule.target.to_destination = ip[1]

        chain.append_rule(rule)
        logger.info("DNAT rule appended %s -> %s",ip[0],ip[1])
        rules.append(rule)
    return rules

def remove_dnat_rule(drule):
    if drule:
        table = iptc.Table(iptc.Table.NAT)
        chain = iptc.Chain(table, "PREROUTING")
        chain.delete_rule(drule)
        logger.info("DNAT rule removed")

def start_afo_patching():

    def fetch_replacement_ip():
        url = "http://dreamcast.online/afo.txt"
        try:
            r = requests.get(url)
            r.raise_for_status()
            afo_IP = r.text.strip()
            return afo_IP
        except requests.exceptions.HTTPError:
            return None

    replacement = fetch_replacement_ip()

    if replacement is None:
        logger.warning("Not starting AFO patch as couldn't get IP from server")
        return

    table = iptc.Table(iptc.Table.NAT)
    chain = iptc.Chain(table, "PREROUTING")

    rule = iptc.Rule()
    rule.protocol = "tcp"
    rule.dst = "63.251.242.131"
    rule.create_target("DNAT")
    rule.target.to_destination = replacement

    chain.append_rule(rule)

    logger.info("AFO routing enabled")
    return rule


def stop_afo_patching(afo_patcher_rule):
    if afo_patcher_rule:
        table = iptc.Table(iptc.Table.NAT)
        chain = iptc.Chain(table, "PREROUTING")
        chain.delete_rule(afo_patcher_rule)
        logger.info("AFO routing disabled")

def start_service(name):
    try:
        logger.info("Starting {} process - Thanks ShuoumaDC!".format(name))
        with open(os.devnull, "wb") as devnull:
            subprocess.check_call(["sudo", "service", name, "start"], stdout=devnull)
    except (subprocess.CalledProcessError, IOError):
        logging.warning("Unable to start the {} process".format(name))


def stop_service(name):
    try:
        logger.info("Stopping {} process".format(name))
        with open(os.devnull, "wb") as devnull:
            subprocess.check_call(["sudo", "service", name, "stop"], stdout=devnull)
    except (subprocess.CalledProcessError, IOError):
        logging.warning("Unable to stop the {} process".format(name))


def get_default_iface_name_linux():
    route = "/proc/net/route"
    with open(route) as f:
        for line in f.readlines():
            try:
                iface, dest, _, flags, _, _, _, _, _, _, _, = line.strip().split()
                if dest != "00000000" or not int(flags, 16) & 2:
                    continue
                return iface
            except:
                continue


def ip_exists(ip, iface):
    command = ["arp", "-a", "-i", iface]
    output = subprocess.check_output(command).decode()
    if ("(%s)" % ip) in output:
        logger.info("IP existed at %s", ip)
        return True
    else:
        logger.info("Free IP at %s", ip)
        return False


def find_next_unused_ip(start):
    interface = get_default_iface_name_linux()

    parts = [int(x) for x in start.split(".")]
    current_check = parts[-1] - 1

    while current_check:
        test_ip = ".".join([str(x) for x in parts[:3] + [current_check]])
        if not ip_exists(test_ip, interface):
            return test_ip
        current_check -= 1

    raise Exception("Unable to find a free IP on the network")


def autoconfigure_ppp(device, speed):
    """
       Every network is different, this function runs on boot and tries
       to autoconfigure PPP as best it can by detecting the subnet and gateway
       we're running on.

       Returns the IP allocated to the Dreamcast
    """

    gateway_ip = subprocess.check_output(
        "route -n | grep 'UG[ \t]' | awk '{print $2}'", shell=True
    ).decode()
    subnet = gateway_ip.split(".")[:3]

    PEERS_TEMPLATE = "{device}\n" "{device_speed}\n" "{this_ip}:{dc_ip}\n" "auth\n"

    OPTIONS_TEMPLATE = "debug\n" "ms-dns {this_ip}\n" "proxyarp\n" "ktune\n" "noccp\n"

    PAP_SECRETS_TEMPLATE = "# Modded from dreampi.py\n" "# INBOUND connections\n" '*       *       ""      *' "\n"

    this_ip = find_next_unused_ip(".".join(subnet) + ".100")
    dreamcast_ip = find_next_unused_ip(this_ip)

    logger.info("Dreamcast IP: {}".format(dreamcast_ip))

    peers_content = PEERS_TEMPLATE.format(
        device=device, device_speed=speed, this_ip=this_ip, dc_ip=dreamcast_ip
    )

    with open("/etc/ppp/peers/dreamcast", "w") as f:
        f.write(peers_content)

    options_content = OPTIONS_TEMPLATE.format(this_ip=this_ip)

    with open("/etc/ppp/options", "w") as f:
        f.write(options_content)

    pap_secrets_content = PAP_SECRETS_TEMPLATE

    with open("/etc/ppp/pap-secrets", "w") as f:
        f.write(pap_secrets_content)

    return dreamcast_ip


ENABLE_SPEED_DETECTION = (
    False
)  # Set this to true if you want to use wvdialconf for device detection


def detect_device_and_speed():
    MAX_SPEED = 57600

    if not ENABLE_SPEED_DETECTION:
        # By default we don't detect the speed or device as it's flakey in later
        # Pi kernels. But it might be necessary for some people so that functionality
        # can be enabled by setting the flag above to True
        return ("/dev/ttyACM0", MAX_SPEED)

    command = ["wvdialconf", "/dev/null"]

    try:
        output = subprocess.check_output(command, stderr=subprocess.STDOUT).decode()

        lines = output.split("\n")

        for line in lines:
            match = re.match(r"(.+)<Info>:\sSpeed\s(\d+);", line.strip())
            if match:
                device = match.group(1)
                speed = int(match.group(2))
                logger.info("Detected device {} with speed {}".format(device, speed))

                # Many modems report speeds higher than they can handle so we cap
                # to 56k
                return device, min(speed, MAX_SPEED)
        else:
            logger.info("No device detected")

    except:
        logger.exception("Unable to detect modem. Falling back to ttyACM0")
    return ("/dev/ttyACM0", MAX_SPEED)


class Daemon(object):
    def __init__(self, pidfile, process):
        self.pidfile = pidfile
        self.process = process

    def daemonize(self):
        try:
            pid = os.fork()
            if pid > 0:
                sys.exit(0)

        except OSError:
            sys.exit(1)

        os.chdir("/")
        os.setsid()
        os.umask(0)

        try:
            pid = os.fork()
            if pid > 0:
                sys.exit(0)
        except OSError:
            sys.exit(1)

        atexit.register(self.delete_pid)
        pid = str(os.getpid())
        with open(self.pidfile, "w+") as f:
            f.write("%s\n" % pid)

    def delete_pid(self):
        os.remove(self.pidfile)

    def _read_pid_from_pidfile(self):
        try:
            with open(self.pidfile, "r") as pf:
                pid = int(pf.read().strip())
        except IOError:
            pid = None
        return pid

    def start(self):
        pid = self._read_pid_from_pidfile()

        if pid:
            logger.info("Daemon already running, exiting")
            sys.exit(1)

        logger.info("Starting daemon")
        self.daemonize()
        self.run()

    def stop(self):
        pid = self._read_pid_from_pidfile()

        if not pid:
            logger.info("pidfile doesn't exist, deamon must not be running")
            return

        try:
            while True:
                os.kill(pid, signal.SIGTERM)
                time.sleep(0.1)

        except OSError:
            if os.path.exists(self.pidfile):
                os.remove(self.pidfile)
            else:
                sys.exit(1)

    def restart(self):
        self.stop()
        self.start()

    def run(self):
        self.process()


class Modem(object):
    def __init__(self, device, speed, send_dial_tone=True):
        self._device, self._speed = device, speed
        self._serial = None
        self._sending_tone = False

        if send_dial_tone:
            self._dial_tone_wav = self._read_dial_tone()
        else:
            self._dial_tone_wav = None

        self._time_since_last_dial_tone = None
        self._dial_tone_counter = 0

    @property
    def device_speed(self):
        return self._speed

    @property
    def device_name(self):
        return self._device

    def _read_dial_tone(self):
        this_dir = os.path.dirname(os.path.abspath(os.path.realpath(__file__)))
        dial_tone_wav = os.path.join(this_dir, "dial-tone.wav")

        with open(dial_tone_wav, "rb") as f:
            dial_tone = f.read()  # Read the entire wav file
            dial_tone = dial_tone[44:]  # Strip the header (44 bytes)

        return dial_tone

    def connect(self):
        if self._serial:
            self.disconnect()

        logger.info("Opening serial interface to {}".format(self._device))
        self._serial = serial.Serial(
            self._device, self._speed, timeout=0
        )
        return self._serial
    
    def connect_netlink(self,speed = 115200, timeout = 0.01, rtscts = False): #non-blocking
        if self._serial:
            self.disconnect()
        logger.info("Opening serial interface to {}".format(self._device))
        self._serial = serial.Serial(
            self._device, speed, timeout=timeout, rtscts = rtscts
        )

    def disconnect(self):
        if self._serial and self._serial.isOpen():
            self._serial.flush()
            self._serial.close()
            self._serial = None
            logger.info("Serial interface terminated")

    def reset(self):
        while True:
            try:
                self.send_command("ATZ0",timeout=3)  # Send reset command
                time.sleep(1)
                self.send_command("AT&F0")
                self.send_command("ATE0W2")  # Don't echo our responses
                return
            except IOError:
                self.shake_it_off() # modem isn't responding. Try a harder reset

    def start_dial_tone(self):
        if not self._dial_tone_wav:
            return
        global first_run
        i = 0
        while i < 3:
            try:
                if first_run:
                    first_run = 0
                    subprocess.Popen("/home/pi/dreampi/dcnet.rpi")
                    time.sleep(2)
                    subprocess.call("sudo killall dcnet.rpi".split())
                    self.shake_it_off()
                self.reset()
                self.send_command(b"AT+FCLASS=8")  # Enter voice mode
                self.send_command(b"AT+VLS=1")  # Go off-hook
                self.send_command(b"AT+VSM=1,8000")  # 8 bit unsigned PCM
                self.send_command(b"AT+VTX")  # Voice transmission mode
                logger.info("<LISTENING>")
                break
            except IOError:
                time.sleep(0.5)
                i+=1
                pass

        self._sending_tone = True

        self._time_since_last_dial_tone = datetime.now() - timedelta(seconds=100)

        self._dial_tone_counter = 0

    def stop_dial_tone(self):
        if not self._sending_tone:
            return
        if self._serial is None:
            raise Exception("Not connected")

        self._serial.write(b"\x00\x10\x03\r\n")
        self.send_escape()
        self.send_command(b"ATH0")  # Go on-hook
        self.reset()  # Reset the modem
        self._sending_tone = False

    def answer(self):
        self.reset()
        # When we send ATA we only want to look for CONNECT. Some modems respond OK then CONNECT
        # and that messes everything up
        self.send_command(b"ATA", ignore_responses=[b"OK"])
        time.sleep(5)
        logger.info("Call answered!")
        #logger.info(subprocess.check_output(["pon", "dreamcast"]).decode())
        self.disconnect()
        subprocess.check_call(["/home/pi/dreampi/dcnet.rpi"])
        logger.info("Connection terminated")
        self.connect()

    def netlink_answer(self):
        self.reset()
        # When we send ATA we only want to look for CONNECT. Some modems respond OK then CONNECT
        # and that messes everything up
        self.send_command(b"ATA", ignore_responses=[b"OK"])
        # time.sleep(5)
        logger.info("Call answered!")
        logger.info("Connected")

    def query_modem(self, command, timeout=3, response = "OK"): #this function assumes we're being passed a non-blocking modem
        if isinstance(command, bytes):
            final_command = command + b'\r\n'
        else:
            final_command = ("%s\r\n" % command).encode()      
        self._serial.write(final_command)
        logger.info(final_command.decode())

        start = time.time()

        line = b""
        while True:
            new_data = self._serial.readline().strip()

            if not new_data: #non-blocking modem will end up here when timeout reached, try until this function's timeout is reached.
                if time.time() - start < timeout:
                    continue
                raise IOError()

            line = line + new_data
            
            if response.encode() in line:
                if response != "OK":
                    logger.info(line.decode())
                return  # Valid response

    def send_command(
        self, command, timeout=60, ignore_responses = None
    ):
        if self._serial is None:
            raise Exception("Not connected")
        if ignore_responses is None:
            ignore_responses = []

        VALID_RESPONSES = [b"OK", b"ERROR", b"CONNECT", b"VCON"]

        for ignore in ignore_responses:
            VALID_RESPONSES.remove(ignore)

        if isinstance(command, bytes):
            final_command = command + b'\r\n'
        else:
            final_command = ("%s\r\n" % command).encode() 

        self._serial.write(final_command)
        logger.info('Command: %s' % final_command.decode())

        start = time.time()
        line = b""
        while True:
            new_data = self._serial.readline().strip()

            if not new_data:
                if time.time() - start < timeout:
                    continue
                raise IOError("There was a timeout while waiting for a response from the modem")

            line = line + new_data
            for resp in VALID_RESPONSES:
                if resp in line:
                    if resp != b"OK":
                        logger.info('Response: %s' % line.decode())
                        if resp == b"ERROR":
                            raise IOError("Command returned an error")
                    # logger.info(line[line.find(resp) :].decode())
                    return  # We are done


    def send_escape(self):
        if self._serial is None:
            raise Exception("Not connected")
        time.sleep(1.0)
        self._serial.write(b"+++")
        time.sleep(1.0)

    def shake_it_off(self): #sometimes the modem gets stuck in data mode
        for i in range(3):
            self._serial.write(b'+')
            time.sleep(0.2)
        time.sleep(4)
        self.send_command('ATH0') #make sure we're on hook
        logger.info("Shook it off")


    def update(self):
        now = datetime.now()
        if self._sending_tone:
            # Keep sending dial tone
            BUFFER_LENGTH = 1000
            TIME_BETWEEN_UPLOADS_MS = (1000.0 / 8000.0) * BUFFER_LENGTH

            if self._dial_tone_wav is None:
                raise Exception("Dial tone wav not loaded")
            if self._serial is None:
                raise Exception("Not connected")

            if (
                not self._time_since_last_dial_tone
                or ((now - (self._time_since_last_dial_tone)).microseconds * 1000)
                >= TIME_BETWEEN_UPLOADS_MS
            ):
                byte = self._dial_tone_wav[
                    self._dial_tone_counter : self._dial_tone_counter + BUFFER_LENGTH
                ]
                self._dial_tone_counter += BUFFER_LENGTH
                if self._dial_tone_counter >= len(self._dial_tone_wav):
                    self._dial_tone_counter = 0
                self._serial.write(byte)
                self._time_since_last_dial_tone = now


class GracefulKiller(object):
    def __init__(self):
        self.kill_now = False
        signal.signal(signal.SIGINT, self.exit_gracefully)
        signal.signal(signal.SIGTERM, self.exit_gracefully)

    def exit_gracefully(self, signum, frame):
        logging.warning("Received signal: %s", signum)
        self.kill_now = True

def do_netlink(side,dial_string,modem,saturn=True):
    # ser = serial.Serial(device_and_speed[0], device_and_speed[1], timeout=0.005)
    state, opponent  = netlink.netlink_setup(side,dial_string,modem)
    if state == "failed":
        for i in range(3):
            modem._serial.write(b'+')
            time.sleep(0.2)
        time.sleep(4)
        modem.send_command(b'ATH0')
        return
    if saturn == False:
        netlink.kddi_exchange(side,state,opponent,ser=modem._serial)
    else:
        netlink.netlink_exchange(side,state,opponent,ser=modem._serial)


def process():
    
    xbandnums = ["18002071194","19209492263","0120717360","0355703001"]
    
    xbandMatching = False
    xbandTimer = None
    xbandInit = False
    openXband = False

    killer = GracefulKiller()

    dial_tone_enabled = "--disable-dial-tone" not in sys.argv

    # Make sure pppd isn't running
    with open(os.devnull, "wb") as devnull:
        subprocess.call(["sudo", "killall", "pppd"], stderr=devnull)

    device_and_speed, internet_connected = None, False
    # Startup checks, make sure that we don't do anything until
    # we have a modem and internet connection
    while True:
        logger.info("Detecting connection and modem...")
        internet_connected = check_internet_connection()
        device_and_speed = detect_device_and_speed()

        if internet_connected and device_and_speed:
            logger.info("Internet connected and device found!")
            break

        elif not internet_connected:
            logger.warn("Unable to detect an internet connection. Waiting...")
        elif not device_and_speed:
            logger.warn("Unable to find a modem device. Waiting...")

        time.sleep(5)

    modem = Modem(device_and_speed[0], device_and_speed[1], dial_tone_enabled)

    dreamcast_ip = autoconfigure_ppp(modem.device_name, modem.device_speed)

    # Get a port forwarding object, now that we know the DC IP.
    if "--enable-port-forwarding" in sys.argv:
        port_forwarding = PortForwarding(dreamcast_ip, logger)
        port_forwarding.forward_all()
    else:
        port_forwarding = None

    mode = "LISTENING"

    modem.connect()
    if dial_tone_enabled:
        modem.start_dial_tone()

    time_digit_heard = None
    global saturn
    saturn = True
    dcnow = DreamcastNowService()
    while True:
        if killer.kill_now:
            break

        now = datetime.now()

        if mode == "LISTENING":
            
            if xbandMatching == True:
                if xbandInit == False:
                    xband.xbandInit()
                    xbandInit = True
                if time.time() - xbandTimer > 900: #Listen for incoming connections for 15 minutes
                    xbandMatching = False
                    xband.closeXband()
                    openXband = False
                    continue
                if openXband == False:
                    xband.openXband()
                    openXband = True
                xbandResult,opponent = xband.xbandListen(modem)
                if xbandResult == "connected":
                    xband.netlink_exchange("waiting","connected",opponent,ser=modem._serial)
                    logger.info("Xband Disconnected")
                    mode = "LISTENING"
                    modem.connect()
                    modem.start_dial_tone()
                    xbandMatching = False
                    xband.closeXband()
                    openXband = False
                
            
            modem.update()
            char = modem._serial.read(1)
            char = char.strip()
            if not char:
                continue

            if ord(char) == 16:
                # DLE character
                try:
                    parsed = netlink.digit_parser(modem)
                    if parsed == "nada":
                        pass
                    elif isinstance(parsed,dict):
                        client = parsed['client']
                        dial_string = parsed['dial_string']
                        side = parsed['side']
                        logger.info("Heard: %s" % dial_string)
                        
                        if dial_string in xbandnums:
                            logger.info("Calling Xband server")
                            client = "xband"
                            mode = "XBAND ANSWERING"

                        elif dial_string == "00":
                            side = "waiting"
                            client = "direct_dial"
                            saturn = False
                        elif dial_string[0:3] == "859":
                            try:
                                kddi_opponent = dial_string
                                kddi_lookup = "https://dial.redreamcast.net/?phoneNumber=%s" % kddi_opponent
                                response = requests.get(kddi_lookup)
                                response.raise_for_status()
                                ip = response.text
                                if len(ip) == 0:
                                    pass
                                else:
                                    dial_string = ip
                                    logger.info(dial_string)
                                    saturn = False
                                    side = "calling"
                                    client = "direct_dial"
                                    time.sleep(7)
                            except requests.exceptions.HTTPError:
                                pass
                        elif len(dial_string.split('*')) == 5 and dial_string.split('*')[-1] == "1":
                            oppIP = '.'.join(dial_string.split('*')[0:4])
                            client = "xband"
                            mode = "NETLINK ANSWERING"
                            side = "calling"
                        
                       
                        if client == "direct_dial":
                            mode = "NETLINK ANSWERING"
                        elif client == "xband":
                            pass
                        else:
                            mode = "ANSWERING"
                        modem.stop_dial_tone()
                        time_digit_heard = now
                except (TypeError, ValueError):
                    pass
                
        elif mode == "XBAND ANSWERING":
            # print("xband answering")
            if (now - time_digit_heard).total_seconds() > 8.0:
                time_digit_heard = None
                modem.query_modem("ATA", timeout=60, response = "CONNECT")
                xband.xbandServer(modem)
                mode = "LISTENING"
                modem.connect()
                modem.start_dial_tone()
                xbandMatching = True
                xbandTimer = time.time()

        elif mode == "ANSWERING":
            if time_digit_heard is None:
                raise Exception("Impossible code path")
            if (now - time_digit_heard).total_seconds() > 8.0:
                time_digit_heard = None
                modem.answer()
#                modem.disconnect()
#                mode = "CONNECTED"
                mode = "LISTENING"
                modem.start_dial_tone()

        elif mode == "NETLINK ANSWERING":
            if (now - time_digit_heard).total_seconds() > 8.0:
                time_digit_heard = None
                
                try:
                    if client == "xband":
                        xband.init_xband(modem)
                        result = xband.ringPhone(oppIP,modem)
                        if result == "hangup":
                            mode = "LISTENING"
                            modem.connect()
                            modem.start_dial_tone()
                        else:
                            mode = "NETLINK_CONNECTED"
                    else:
                        modem.connect_netlink(speed=57600,timeout=0.01,rtscts = True) #non-blocking version
                        modem.query_modem(b"AT%E0\V1")
                        if saturn:
                            modem.query_modem(b'AT%C0\N3')
                            modem.query_modem(b'AT+MS=V32b,1,14400,14400,14400,14400')
                        modem.query_modem(b"ATA", timeout=120, response = "CONNECT")
                        mode = "NETLINK_CONNECTED"
                except IOError:
                    modem.connect()
                    mode = "LISTENING"
                    modem.start_dial_tone()
        elif mode == "CONNECTED":
            dcnow.go_online(dreamcast_ip)
            
            for line in sh.tail("-f", "/var/log/messages", "-n", "1", _iter=True):
                if "pppd" in line and "Exit" in line:#wait for pppd to execute the ip-down script
                    logger.info("Detected modem hang up, going back to listening")
                    break
            dcnow.go_offline() #changed dcnow to wait 15 seconds for event instead of sleeping. Should be faster.
            mode = "LISTENING"
            # modem = Modem(device_and_speed[0], device_and_speed[1], dial_tone_enabled)
            modem.connect()
            if dial_tone_enabled:
                modem.start_dial_tone()
        elif mode == "NETLINK_CONNECTED":
            if client == "xband":
                xband.netlink_exchange("calling","connected",oppIP,ser=modem._serial)
            else:
                do_netlink(side,dial_string,modem,saturn=saturn)
            logger.info("Netlink Disconnected")
            mode = "LISTENING"
            modem.connect()
            modem.start_dial_tone()
    if port_forwarding is not None:
        port_forwarding.delete_all()
    return 0


def enable_prom_mode_on_wlan0():
    """
        The Pi wifi firmware seems broken, we can only get it to work by enabling
        promiscuous mode.

        This is a hack, we just enable it for wlan0 and ignore errors
    """

    try:
        subprocess.check_call("sudo ifconfig wlan0 promisc".split())
        logging.info("Promiscuous mode set on wlan0")
    except subprocess.CalledProcessError:
        logging.info("Attempted to set promiscuous mode on wlan0 but was unsuccessful")
        logging.info("Probably no wifi connected, or using a different device name")


def main():
    afo_patcher_rule = None
    ttl_rule = None
    dnat_rules = []

    try:
        # Don't do anything until there is an internet connection
        while not check_internet_connection():
            logger.info("Waiting for internet connection...")
            time.sleep(3)
        
        #try auto updates /disabled for now
        updater()
        global xband
        global netlink
        try:
            import xband as xband
            import netlink as netlink
        except ImportError:
            logger.info("couldn't import xband or netlink modules")


        # Dreampi local update check
        dreampi_py_local_update()

        # Try to update the DNS configuration
        update_dns_file()

        # Hack around dodgy Raspberry Pi things
        enable_prom_mode_on_wlan0()

        # Just make sure everything is fine
        restart_dnsmasq()

        config_server.start()
        afo_patcher_rule = start_afo_patching()
        dnat_rules = start_dnat_rules()
        ttl_rule = add_increased_ttl()
        start_service("dcvoip")
        start_service("dcgamespy")
        start_service("dc2k2")
        start_service("dcdaytona")
        return process()
    except:
        logger.exception("Something went wrong...")
        return 1
    finally:
        stop_service("dc2k2")
        stop_service("dcgamespy")
        stop_service("dcvoip")
        stop_service("dcdaytona")
        if afo_patcher_rule is not None:
            stop_afo_patching(afo_patcher_rule)
        if ttl_rule is not None:
            remove_increased_ttl(ttl_rule)
        if dnat_rules is not None:
            for drule in dnat_rules:
                remove_dnat_rule(drule)

        config_server.stop()
        logger.info("Dreampi quit successfully")


if __name__ == "__main__":
    logger.setLevel(logging.INFO)
    syslog_handler = logging.handlers.SysLogHandler(address="/dev/log")
    syslog_handler.setFormatter(
        logging.Formatter("%(name)s[%(process)d]: %(levelname)s %(message)s")
    )
    logger.addHandler(syslog_handler)

    if len(sys.argv) > 1 and "--no-daemon" in sys.argv:
        # logger.addHandler(logging.StreamHandler())
        sys.exit(main())

    daemon = Daemon("/tmp/dreampi.pid", main)

    if len(sys.argv) == 2:
        if sys.argv[1] == "start":
            daemon.start()
        elif sys.argv[1] == "stop":
            daemon.stop()
        elif sys.argv[1] == "restart":
            daemon.restart()
        else:
            sys.exit(2)
        sys.exit(0)
    else:
        print(("Usage: %s start|stop|restart" % sys.argv[0]))
        sys.exit(2)
