# Evaluating Trustworthiness of Edge-Based Multi-Tenanted IoT Devices

Internet of Things (IoT) systems are expected to be deployed as solutions to problems in a wide variety of contexts, from building management, to smart city monitoring and to provide support to emergency services. However, many IoT devices are resource constrained and lack the capability or information to compute results for tasks that the IoT devices may be requested to perform. Instead these tasks will need to be offloaded to a server at the Edge of the network for a quick response. As these networks will have multiple organisations providing multiple IoT nodes and Edge nodes with different capabilities, the IoT devices need to know which Edge server they trust to return a timely response to a task.

This repository provides an implementation of such a system to facilitate trust-based task offloading. We provide two example applications and multiple example trust models.

The project was funded by [PETRAS](https://petras-iot.org/project/evaluating-trustworthiness-of-edge-based-multi-tenanted-iot-devices-team/), for more information see [here](https://mbradbury.github.io/projects/project-6-TEAM/).

This system architecture is described in: **Matthew Bradbury**, Arshad Jhumka, and Tim Watson. Trust Assessment in 32 KiB of RAM: Multi-application Trust-based Task Offloading for Resource-constrained IoT Nodes. In *The 36th ACM/SIGAPP Symposium on Applied Computing*, SAC'21, 1–10. Virtual Event, Republic of Korea, 22–26 March 2021. ACM. [doi:10.1145/3412841.3441898](https://doi.org/10.1145/3412841.3441898). [[bibtex](https://github.com/MBradbury/publications/raw/master/bibtex/Bradbury_2021_TrustAssessment32.bib)] [[file](https://github.com/MBradbury/publications/raw/master/papers/SAC-DADS2021.pdf)] [[dataset](https://doi.org/10.5281/zenodo.4312801)].

## Repository Structure

This repository is structured as follows, with the most important implementation being in:

 - The implementation of firmware for IoT and Edge nodes is contained in [/wsn/node](https://github.com/MBradbury/iot-trust-task-alloc/tree/master/wsn/node) and [/wsn/edge](https://github.com/MBradbury/iot-trust-task-alloc/tree/master/wsn/edge) respectively.
 - The implementation of two example resource-rich applications and the root node application is contained in [/resource_rich](https://github.com/MBradbury/iot-trust-task-alloc/tree/master/resource_rich).

Other directories contain supporting code:

 - The [/tools](https://github.com/MBradbury/iot-trust-task-alloc/tree/master/tools) and [/tests/run](https://github.com/MBradbury/iot-trust-task-alloc/tree/master/tests/run) directories contains various tools in order to setup and run experiments
 - The [/analysis](https://github.com/MBradbury/iot-trust-task-alloc/tree/master/analysis) directory contains scripts to analyse and graph results from experiments

# Setup

This system assumes the use of [Zolertia RE-Mote rev.b](https://zolertia.io/product/re-mote/) hardware for IoT deployments. Parts of the implementation depend on the hardware accelerated cryptograpic operations they provide.

## Development

1. Install dependancies

```bash
sudo apt-get install git build-essential gcc-arm-none-eabi python3 texlive-extra-utils cm-super texlive-latex-extra dvipng poppler-utils
python3 -m pip install pexpect
```

2. Download Contiki-NG

```bash
mkdir ~/wsn
cd ~/wsn
git clone -b petras https://github.com/MBradbury/contiki-ng.git
git submodule update --init
```

Edit `~/.bashrc` to add the path to Contiki-NG before the interactivity check:
```bash
export CONTIKING_DIR="~/wsn/contiki-ng"
export COOJA_DIR="$CONTIKING_DIR/tools/cooja"
```

3. Clone this repository

```bash
cd ~/wsn
git clone https://github.com/MBradbury/iot-trust-task-alloc.git
cd iot-trust-task-alloc && git submodule update --init
```

4. Install Wireshark

Install the latest version of wireshark to be able to analyse OSCORE packets. Instructions originated from [here](https://ask.wireshark.org/question/9916/wireshark-302-linux-for-debianubuntu/)

```bash
cd ~
mkdir wireshark
git clone https://gitlab.com/wireshark/wireshark.git -b release-3.4
cd wireshark
sudo tools/debian-setup.sh --install-optional --install-deb-deps --install-test-deps
dpkg-buildpackage -b -uc -us -jauto
cd ..
rm wireshark-{doc,dev,dbg}_*.deb
sudo dpkg -i *.deb
```

Install pyshark version 0.4.3 or greater
```bash
python3 -m pip install --upgrade pyshark
```

If you cannot install from pip, then install from source:
```bash
python3 -m pip install --upgrade git+https://github.com/KimiNewt/pyshark.git#subdirectory=src
```

## Using Wireshark

In order for wireshark to decrypt OSCORE projected packets you will need to provide details on the OSCORE security contexts to Wireshark. OSCORE is only supported on unreleased versions of Wireshark (as of writing this), hence the need to install Wireshark from source.

To setup OSCORE contexts in Wireshark please do the following:
1. Select `Edit`
2. Select `Preferences`
3. Expand `Protocols` and select `OSCORE`
4. Click on `Edit` in order to enter security contexts

The OSCORE contexts will be generated by `analysis/parser/pyshark_pcap.py` in a file called `oscore.contexts.uat`.

## Root Node

1. Install dependancies

Please note that mosquitto needs to be version 1.6 or later as MQTT v5 support is required. If Debian provides an older version you will need to install it from mosquitto's repository
```bash
wget http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key
sudo apt-key add mosquitto-repo.gpg.key
sudo wget http://repo.mosquitto.org/debian/mosquitto-buster.list -P /etc/apt/sources.list.d/
sudo apt-get update
sudo apt-get install mosquitto mosquitto-clients
```

Other dependancies can be installed via:
```bash
sudo apt-get install libcoap2-bin
sudo apt-get install build-essential git
python3 -m pip install cryptography more_itertools
python3 -m pip install --upgrade "git+https://github.com/chrysn/aiocoap#egg=aiocoap[all]"
python3 -m pip install --upgrade "git+https://github.com/eclipse/paho.mqtt.python.git"
python3 -m pip install --upgrade "git+https://github.com/sbtinstruments/asyncio-mqtt.git"
```

2. Clone Contiki-NG

```bash
cd ~
git clone -b petras https://github.com/MBradbury/contiki-ng.git
cd contiki-ng && git submodule update --init
```

Build the tunslip6 executable
```bash
cd ~/contiki-ng/tools/serial-io
make
```

3. Clone this repository

```bash
cd ~
git clone https://github.com/MBradbury/iot-trust-task-alloc.git
cd iot-trust-task-alloc && git submodule update --init
```

## Resource Rich Nodes (Edges) and Resource Constrained Nodes (Monitors)

```bash
sudo apt-get install git python3-pip python3-dev pipenv python3-serial
python3 -m pip install pyserial
```

```bash
cd  ~
git clone https://gitlab.com/cs407-idiots/pi-client.git
cd pi-client
pipenv install
```

## Resource Rich Nodes (Edges)

1. Install dependancies

```bash
python3 -m pip install cbor2 pyroutelib3 more_itertools
python3 -m pip install --upgrade "git+https://github.com/chrysn/aiocoap#egg=aiocoap[all]"
```

2. Clone this repository

```bash
cd ~
git clone https://github.com/MBradbury/iot-trust-task-alloc.git
cd iot-trust-task-alloc && git submodule update --init
```


# Instructions to Deploy

1. Configure and build

Edit `~/wsn/iot-trust-task-alloc/tools/setup.py` to specify the nodes used in the network then run it.


```bash
cd ~/wsn/iot-trust-task-alloc
./tools/setup.py <trust-model>
```

2. On Root

Set up tun0 interface.
```bash
cd ~/contiki-ng/tools/serial-io
sudo ./tunslip6 -s /dev/ttyUSB0 fd00::1/64
```

Once `tunslip` is running, mosquitto needs to be restarted:
```bash
sudo service mosquitto restart
```

The keyserver and mqtt-coap-bridge now needs to be started:
```bash
cd ~/iot-trust-task-alloc/resource_rich/root
./root_server.py -k keystore
```

You might want to consider running in developer mode with warnings on:
```bash
python3 -X dev -W default root_server.py -k keystore
```

3. On Observer for Sensor Nodes

```bash
cd pi-client && pipenv shell
```

Find the device
```bash
./tools/motelist-zolertia
-------------- ---------------- ---------------------------------------------
Reference      Device           Description
-------------- ---------------- ---------------------------------------------
ZOL-RM02-B1002325 /dev/ttyUSB0     Silicon Labs Zolertia RE-Mote platform
```

Flash and run the terminal
```bash
./flash.py "/dev/ttyUSB0" node.bin zolertia contiki &&  ./tools/pyterm -b 115200 -p /dev/ttyUSB0
```

3. On Observer for Edge Nodes

```bash
cd pi-client && pipenv shell
```

Find the device
```bash
./tools/motelist-zolertia
-------------- ---------------- ---------------------------------------------
Reference      Device           Description
-------------- ---------------- ---------------------------------------------
ZOL-RM02-B1002325 /dev/ttyUSB0     Silicon Labs Zolertia RE-Mote platform
```

Flash and run the terminal
```bash
./flash.py "/dev/ttyUSB0" edge.bin zolertia contiki
```

Now start up the Edge bridge:
```bash
~/iot-trust-task-alloc/resource_rich/applications
./edge_bridge.py
```

Plus any applications that are desired:
```bash
./monitoring.py
./routing.py
```

# Instructions to analyse results

## Obtaining Results

In order to fetch results from the devices run:
```bash
./tools/fetch_results.py results/<log-dir>
```

## Generate pcap

If the binaries were compiled with `--with-pcap` then there will be a `*.packet.log` file for each device. This now needs to be converted to a pcap file.

pcaps can either be converted individually using `./tools/regenerate_pcap.py` or processed for all packet logs in a directory using `./tools/regenerate_pcaps.py`. When a large number of packets have been collected, it will be necessary to disable the timeout with `--timeout None`.

Individually:
```bash
./tools/regenerate_pcap.py results/2021-02-03-am-dadspp-one-good-one-bad/edge.wsn6.packet.log --timeout None
```

Batch:
```bash
./tools/regenerate_pcaps.py results/2021-02-03-am-dadspp-one-good-one-bad/ --timeout None
```

Once pcaps have been generated Wireshark can be used to view them via:
```bash
wireshark -o:oscore.contexts=results/2021-02-03-am-dadspp-one-good-one-bad/keystore/oscore.contexts.uat results/2021-02-03-am-dadspp-one-good-one-bad/edge.wsn6.packet.log.pcap
```

## Graphing Results

There are a variety of tools to graph the results

### Graphing Messages Sent and Received

To graph the number of bytes sent and received, use the following:
```bash
./analysis/graph/messages.py --log-dir results/2021-02-03-pm-dadspp-one-good-one-bad/
```
This tool will categorise the messages.

### Trust Choose Over Time

To graph trust ranking and to whom tasks were sent over time, use the following:
```bash
./analysis/graph/trust_choose_evolution.py --log-dir results/2021-02-03-pm-dadspp-one-good-one-bad/
```
