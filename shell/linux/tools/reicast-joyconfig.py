#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import print_function

import sys
import re
import os
if sys.version_info < (3, 0):
    import ConfigParser as configparser
    INPUT_FUNC = raw_input
else:
    import configparser
    INPUT_FUNC = input

try:
    import evdev
except ImportError:
    print("Can't import evdev module. Please install it.")
    print("You can do this via:")
    print("  pip install evdev")
    sys.exit(1)

DEV_ID_PATTERN = re.compile('(\d+)')
DREAMCAST_BUTTONS = ['A', 'B', 'C', 'D', 'X', 'Y', 'Z', 'START']
DREAMCAST_DPAD = [('X', 'LEFT', 'RIGHT'), ('Y', 'UP', 'DOWN')]
DREAMCAST_TRIGGERS = ['TRIGGER_LEFT', 'TRIGGER_RIGHT']
DREAMCAST_STICK_AXES = [('X', 'left'), ('Y', 'up')]
SECTIONS = ["emulator", "dreamcast", "compat"]


def list_devices():
    devices = [(int(DEV_ID_PATTERN.findall(fn)[0]), evdev.InputDevice(fn))
               for fn in evdev.list_devices()]

    dev_id_len = len(str(max([dev_id for dev_id, dev in devices])))

    for dev_id, dev in sorted(devices, key=lambda x: x[0]):
        print("%s: %s (%s, %s)" %
              (str(dev_id).rjust(dev_id_len), dev.name, dev.fn, dev.phys))


def clear_events(dev):
    try:
        event = dev.read_one()
        while(event is not None):
            event = dev.read_one()
    except (OSError, IOError):
        # BlockingIOErrors should only occur if someone uses the evdev
        # module < v0.4.4. BlockingIOError inherits from OSError, so we
        # catch that for Python 2 compatibility. We also catch IOError,
        # just in case.
        pass


def read_button(dev):
    for event in dev.read_loop():
        if event.type == evdev.ecodes.EV_KEY and event.value == 0:
            break
    return event


def read_axis(dev, absinfos):
    axis_inverted = False
    for event in dev.read_loop():
        if event.type == evdev.ecodes.EV_ABS and \
           event.value in (absinfos[event.code].min, absinfos[event.code].max):
            if event.value == absinfos[event.code].max:
                axis_inverted = True
            break
    return (event, axis_inverted)


def read_axis_or_key(dev, absinfos):
    axis_inverted = False
    for event in dev.read_loop():
        if event.type == evdev.ecodes.EV_KEY and event.value == 0:
            break
        elif (event.type == evdev.ecodes.EV_ABS and event.value in
              (absinfos[event.code].min, absinfos[event.code].max)):
            if event.value == absinfos[event.code].max:
                axis_inverted = True
            break
    return (event, axis_inverted)


def print_mapped_button(name, event):
    try:
        code_id = evdev.ecodes.BTN[event.code]
    except (IndexError, KeyError):
        try:
            code_id = evdev.ecodes.KEY[event.code]
        except (IndexError, KeyError):
            code_id = None
    if type(code_id) is list:
        code_id = code_id[0]
    code_id = (' (%s)' % code_id) if code_id else ''
    print("%s mapped to %d%s." % (name, event.code, code_id))


def print_mapped_axis(name, event, axis_inverted=False):
    try:
        code_id = evdev.ecodes.ABS[event.code]
    except (IndexError, KeyError):
        code_id = None
    if type(code_id) is list:
        code_id = code_id[0]
    code_id = (' (%s)' % code_id) if code_id else ''
    inv = (' (inverted)' if axis_inverted else '')
    print("%s mapped to %d%s%s." % (name, event.code, code_id, inv))


def setup_device(dev_id):
    print("Using device %d..." % dev_id)
    fn = "/dev/input/event%d" % dev_id
    dev = evdev.InputDevice(fn)
    print("Name: %s" % dev.name)
    print("File: %s" % dev.fn)
    print("Phys: %s" % dev.phys)

    cap = dev.capabilities(verbose=False, absinfo=True)
    try:
        absinfos = dict(cap[evdev.ecodes.EV_ABS])
    except KeyError:
        absinfos = dict()

    mapping = configparser.RawConfigParser()
    for section in SECTIONS:
        mapping.add_section(section)
    mapping.set("emulator", "mapping_name", dev.name)

    # Emulator escape button
    if ask_yes_no("Do you want to map a button to exit the emulator"):
        clear_events(dev)
        print("Press the that button now...")
        event = read_button(dev)
        mapping.set("emulator", "btn_escape", event.code)
        print_mapped_button("emulator escape button", event)

    # Regular dreamcast buttons
    for button in DREAMCAST_BUTTONS:
        if ask_yes_no("Do you want to map the %s button?" % button):
            clear_events(dev)
            print("Press the %s button now..." % button)
            event = read_button(dev)
            mapping.set("dreamcast", "btn_%s" % button.lower(), event.code)
            print_mapped_button("%s button" % button, event)

    # DPads
    for i in range(1, 3):
        if ask_yes_no("Do you want to map DPad %d?" % i):
            for axis, button1, button2 in DREAMCAST_DPAD:
                clear_events(dev)
                print("Press the %s button of DPad %d now..." % (button1, i))
                event, axis_inverted = read_axis_or_key(dev, absinfos)
                if event.type == evdev.ecodes.EV_ABS:
                    axisname = "axis_dpad%d_%s" % (i, axis.lower())
                    mapping.set("compat", axisname, event.code)
                    mapping.set("compat", "%s_inverted" % axisname, "yes" if axis_inverted else "no")
                    print_mapped_axis("%s axis of DPad %d" % (axis, i), event, axis_inverted)
                else:
                    buttonconf = "btn_dpad%d_%%s" % i
                    mapping.set("dreamcast", buttonconf % button1.lower(), event.code)
                    print_mapped_button("%s button of DPad %d" % (button1, i), event)
                    clear_events(dev)
                    print("Press the %s button of DPad %d now..." % (button2, i))
                    event = read_button(dev)
                    mapping.set("dreamcast", buttonconf % button2.lower(), event.code)
                    print_mapped_button("%s button of DPad %d" % (button2, i), event)

    # Triggers
    for trigger in DREAMCAST_TRIGGERS:
        if ask_yes_no("Do you want to map %s?" % trigger):
            clear_events(dev)
            print("Press the %s now..." % trigger)
            event, axis_inverted = read_axis_or_key(dev, absinfos)
            axis_inverted = not axis_inverted
            if event.type == evdev.ecodes.EV_ABS:
                axisname = "axis_%s" % trigger.lower()
                mapping.set("dreamcast", axisname, event.code)
                mapping.set("compat", "%s_inverted" % axisname, "yes" if axis_inverted else "no")
                print_mapped_axis("analog %s" % trigger, event, axis_inverted)
            else:
                mapping.set("compat", "btn_%s" % trigger.lower(), event.code)
                print_mapped_button("digital %s" % trigger, event)

    # Stick
    if ask_yes_no("Do you want to map the analog stick?"):
        for axis, axis_dir in DREAMCAST_STICK_AXES:
            clear_events(dev)
            print("Please move the analog stick as far %s as possible now..." % axis_dir)
            event, axis_inverted = read_axis(dev, absinfos)
            axisname = "axis_%s" % axis.lower()
            mapping.set("dreamcast", axisname, event.code)
            mapping.set("compat", "%s_inverted" % axisname, "yes" if axis_inverted else "no")
            print_mapped_axis(axis, event, axis_inverted)

    for section in SECTIONS:
        if not mapping.options(section):
            mapping.remove_section(section)

    return mapping


def ask_yes_no(question, default=True):
    valid = {"yes": True, "y": True, "ye": True,
             "no": False, "n": False}
    prompt = "Y/n" if default else "y/N"

    while True:
        print("%s [%s] " % (question, prompt), end='')
        choice = INPUT_FUNC().lower()
        if choice == '':
            return default
        if choice in valid:
            return valid[choice]
        else:
            print("Please respond with 'yes' or 'no'")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(
      description='Reicast evdev controller mapping editor')
    parser.add_argument('-d', '--device-id', action='store', type=int,
                        default=-1, help='device-id to map.')
    parser.add_argument('-f', '--file', action='store', default=None,
                        help='write mapping to this file')
    args = parser.parse_args()

    if args.device_id < 0:
        list_devices()
        dev_id = int(input("Please enter the device id: "))
    else:
        dev_id = args.device_id

    mapping = setup_device(dev_id)

    if args.file:
        with open(args.file, "w") as f:
            mapping.write(f)
        print("\nMapping file saved to: %s\n" % os.path.abspath(args.file))
    else:
        print("\nHere's your mapping file:\n")
        mapping.write(sys.stdout)

    sys.exit(0)
