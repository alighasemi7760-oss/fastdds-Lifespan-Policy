#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import time
import os
from mininet.net import Mininet
from mininet.node import OVSSwitch
from mininet.link import TCLink
from mininet.cli import CLI
from mininet.log import setLogLevel, info

def create_ddil_network():
    # Clean up any leftover Mininet resources before starting
    os.system('sudo mn -c > /dev/null 2>&1')

    # Initialize network without external controller
    net = Mininet(link=TCLink)

    info('*** Adding hosts (DDS Publisher and Subscriber)\n')
    h1 = net.addHost('h1', ip='10.0.0.1/24') # Publisher
    h2 = net.addHost('h2', ip='10.0.0.2/24') # Subscriber

    info('*** Adding switch (Standalone mode)\n')
    s1 = net.addSwitch('s1', cls=OVSSwitch, failMode='standalone')

    info('*** Creating links with initial DDIL conditions (Low Bandwidth & High Delay)\n')
    link_opts = dict(bw=1, delay='200ms', loss=5, use_tbf=True)
    net.addLink(h1, s1, **link_opts)
    net.addLink(h2, s1)

    info('*** Starting network\n')
    net.start()

    h1_interface = h1.defaultIntf()

    try:
        info('\n*** Starting DDS Applications on Hosts...\n')
        
        # Start Subscriber on h2 in background
        info('==> Launching Subscriber on h2...\n')
        h2.cmd('/root/dds_qos_project/build/DataSample subscriber > /root/dds_qos_project/sub_test.log 2>&1 &')
        time.sleep(2) # Give subscriber time to initialize

        # Start Publisher on h1 in background
        info('==> Launching Publisher on h1...\n')
        h1.cmd('/root/dds_qos_project/build/DataSample publisher > /root/dds_qos_project/pub_test.log 2>&1 &')
        time.sleep(2) # Allow DDS discovery to match entities

        info('\n*** DDIL Simulation Started. Monitoring Lifespan Policy Behavior...\n')

        # Phase 1: Normal DDIL operation
        info('==> Phase 1: Low Bandwidth (1Mbps) & High Delay (200ms). Testing normal operation...\n')
        time.sleep(15)

        # Phase 2: Intermittent connection with severe loss and jitter
        info('==> Phase 2: Intermittent Connection (High Loss: 40%, Jittery Delay)...\n')
        h1.cmd(f'tc qdisc change dev {h1_interface} root netem delay 400ms 100ms loss 40%')
        time.sleep(20)

        # Phase 3: Complete network outage (Link Down)
        # Samples accumulated during this 15s will expire due to 5s Lifespan QoS policy
        info('==> Phase 3: Total Disconnection (Link DOWN)...\n')
        net.configLinkStatus('h1', 's1', 'down')
        time.sleep(15)

        # Phase 4: Connection restored (Link Up)
        info('==> Phase 4: Connection Restored (Link UP). Testing Adaptive QoS recovery...\n')
        net.configLinkStatus('h1', 's1', 'up')
        # Use 'replace' to prevent RTNETLINK 'File exists' error
        h1.cmd(f'tc qdisc replace dev {h1_interface} root netem delay 200ms loss 5%')
        time.sleep(15)

        # Gracefully stop DDS applications after test completion
        info('\n*** Stopping DDS processes...\n')
        h1.cmd('pkill -f DataSample')
        h2.cmd('pkill -f DataSample')

    except KeyboardInterrupt:
        info('\n*** Simulation interrupted by user.\n')

    finally:
        info('*** Starting CLI for manual testing and verification\n')
        CLI(net)
        info('*** Stopping network\n')
        net.stop()

if __name__ == '__main__':
    setLogLevel('info')
    create_ddil_network()
