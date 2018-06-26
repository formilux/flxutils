#!/bin/sh

# Maps USB serial devices to /dev entries based on vendor, product and serial.

# usage: $0 {add|remove}
# uses $SUBSYSTEM, $DEVPATH, $DEVNAME
# for add, uses $HP_VENDOR, $HP_PRODUCT, $HP_SERIAL
# returns the name of a link to create on add, and the name of files to
# look for on remove.

case "$1:$SUBSYSTEM:$HP_VENDOR:$HP_PRODUCT:$HP_SERIAL" in
  remove:tty:*:*:*)           echo "tty-node*" ;;
  add:tty:0403:6015:DJ00JRHF) echo "tty-node1" ;;
esac
