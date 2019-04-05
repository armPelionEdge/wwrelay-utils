#!/bin/bash

START_EDGE_CORE="/etc/init.d/mbed-edge-core start"

function run_edge_core() {
	while true; do
        if ! pgrep -x "edge-core" > /dev/null
        then
            # Only start edge-core if maestro is running
            if pgrep "maestro" > /dev/null; then
                $START_EDGE_CORE &
                sleep 5
                kill $(ps aux | grep 'mbed-devicejs-bridge' | awk '{print $2}');
                kill $(ps aux | grep '/wigwag/mbed/pt-example' | awk '{print $2}');
                sleep 5
                /wigwag/mbed/pt-example -n pt-example --endpoint-postfix=-$(cat /sys/class/net/eth0/address) >> /var/log/pt-example.log 2>&1
                kill $(ps aux | grep '/wigwag/mbed/blept-example' | awk '{print $2}');
                sleep 5
                /etc/init.d/mept-ble start
            fi
        fi
        sleep 5
	done
}

# Let this script handle it for MVP1.
# If edge-core is started by init script then we cant start pt-example
# So kill edge-core and let this script start it in order
killall edge-core
run_edge_core