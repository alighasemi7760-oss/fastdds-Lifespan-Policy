#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import time
from mininet.net import Mininet
from mininet.node import OVSSwitch
from mininet.link import TCLink
from mininet.cli import CLI
from mininet.log import setLogLevel, info

def create_ddil_network():
    # راه‌اندازی شبکه بدون نیاز به Controller خارجی
    net = Mininet(link=TCLink)

    info('*** Adding hosts (DDS Publisher and Subscriber)\n')
    h1 = net.addHost('h1', ip='10.0.0.1/24') # Publisher
    h2 = net.addHost('h2', ip='10.0.0.2/24') # Subscriber

    info('*** Adding switch (Standalone mode)\n')
    # تنظیم سوئیچ روی حالت failMode='standalone' باعث می‌شود سوئیچ بدون کنترلر هم مثل یک سوییچ عادی کار کند
    s1 = net.addSwitch('s1', cls=OVSSwitch, failMode='standalone')

    info('*** Creating links with initial DDIL conditions (Low Bandwidth & High Delay)\n')
    link_opts = dict(bw=1, delay='200ms', loss=5, use_tbf=True)
    net.addLink(h1, s1, **link_opts)
    net.addLink(h2, s1)

    info('*** Starting network\n')
    net.start()

    h1_interface = h1.defaultIntf()

    try:
        info('\n*** DDIL Simulation Started. Monitoring Lifespan Policy Behavior...\n')
        
        info('==> Phase 1: Low Bandwidth (1Mbps) & High Delay (200ms). Testing normal operation...\n')
        time.sleep(15)

        info('==> Phase 2: Intermittent Connection (High Loss: 40%, Jittery Delay)...\n')
        h1.cmd(f'tc qdisc change dev {h1_interface} root netem delay 400ms 100ms loss 40%')
        time.sleep(20)

        info('==> Phase 3: Total Disconnection (Link DOWN)...\n')
        net.configLinkStatus('h1', 's1', 'down')
        time.sleep(15) 

        info('==> Phase 4: Connection Restored (Link UP). Testing Adaptive QoS recovery...\n')
        net.configLinkStatus('h1', 's1', 'up')
        h1.cmd(f'tc qdisc add dev {h1_interface} root netem delay 200ms loss 5%')
        time.sleep(15)

    except KeyboardInterrupt:
        info('\n*** Simulation interrupted by user.\n')
    
    finally:
        info('*** Starting CLI for manual testing and verification\n')
        CLI(net)
        info('*** Stopping network\n')
        net.stop()

if __name__ == '__main__':
    setLogLevel('info')
    # پاکسازی محیط‌های قبلی Mininet قبل از شروع برای جلوگیری از تداخل سوئیچ‌ها
    import os
    os.system('sudo mn -c > /dev/null 2>&1')
    create_ddil_network()
