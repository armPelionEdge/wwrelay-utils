#!/bin/bash
# This has to be on ext4 fs USB stick. NTFS is not detected by our gateway
# /etc/init.d/deviceOS-watchdog humanhalt
# /etc/init.d/deviceOS-watchdog humanhalt

# killall node
ledcontrol=/wigwag/system/bin/led
source "/wigwag/system/lib/bash/relaystatics.sh"
function grabip() {
    ifconfig > ifconfig.txt
}
function grabip2() {
    ifconfig > ifconfig2.txt
}
function dhcpthis(){
    udhcpc -n
}
$ledcontrol 5 5 5
udhcpc eth0
# /etc/init.d/devjssupport start


#--------Commands go here

grabip
dhcpthis
grabip2
# sleep 15
# curl http://localhost:3000/start
IP=$(ifconfig | grep -A 2 -E 'wlan|eth|wlp|enp' | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1')
curl -X POST -H 'Content-type: application/json' --data '{"text":"IP='$IP', RELAYID='$relayID'"}' https://hooks.slack.com/services/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
# /etc/init.d/relayterm start
# rm -rf /userdata/etc/devicejs/db/
sleep 5
$ledcontrol 0 10 0
