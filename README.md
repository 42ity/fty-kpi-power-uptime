# fty-kpi-power-uptime

Agent fty-kpi-power-uptime detects UPSes in each DC and computes uptime of the DC from changes in their status.

## How to build

To build fty-kpi-power-uptime project run:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=usr -DBUILD_TESTING=On ..
make
sudo make install
```

## How to run

To run fty-kpi-power-uptime project:

* from within the build tree, run:

```bash
./build/agent/fty-kpi-power-uptime
```

For the other options available, refer to the manual page of fty-kpi-power-uptime

* from an installed base, using systemd, run:

```bash
systemctl start fty-kpi-power-uptime
```

### Configuration file

Configuration file - fty-kpi-power-uptime.cfg - is currently ignored.

Agent has a state file stored in /var/lib/fty/fty-kpi-power-uptime/state.

## Architecture

### Overview

fty-kpi-power-uptime has 1 actor:

* fty-kpi-power-uptime-server: main actor

(Malamute address is "fty-kpi-power-uptime").

After every 100 requests, agent stores its state into the state file.

## Protocols

### Published metrics

Agent doesn't publish any metrics.

### Published alerts

Agent doesn't publish any alerts.

### Mailbox requests

Agent fty-kpi-power-uptime can be requested for:

* uptime info

#### Uptime info

The USER peer sends the following message using MAILBOX SEND to
FTY-KPI-POWER-UPTIME-SERVER ("fty-kpi-power-uptime") peer:

* UPTIME/dc - request uptime info for datacenter 'dc'

where
* '/' indicates a multipart string message
* 'dc' MUST be name of a datacenter
* any subject accepted.

The FTY-KPI-POWER-UPTIME-SERVER peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* UPTIME/total/offline
* ERROR/reason

where
* '/' indicates a multipart frame message
* 'total' is how long the datacenter exists (in milliseconds)
* 'offline' is how many milliseconds at least one of its UPSes was offline
* 'reason' is string detailing reason for error
* subject of the message same as for the request.

### Stream subscriptions

Agent is subscribed to ASSETS stream (for datacenter messages).

If agent regularly check UPS metrics, to know the UPS is protecting some datacenter.

If it does, the agent stores its status and updates total and offline time for its datacenter.

If agent receives an asset, agent checks that it's a datacenter and stores the UPSes for specified datacenter.
