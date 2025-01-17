#!/usr/bin/env python3

import os
import glob
from datetime import datetime
import re
import ipaddress
import ast
from dataclasses import dataclass
from enum import Enum, IntEnum
from typing import Union
import pathlib
from collections import defaultdict, Counter
from pprint import pprint

from analysis.parser.common import parse_contiki

class ChallengeResponseType(IntEnum):
    NO_ACK = 0
    TIMEOUT = 1
    RESPONSE = 2

@dataclass(frozen=True)
class ChallengeResponse:
    kind: ChallengeResponseType
    good: bool

@dataclass(frozen=True)
class EdgeResourceTM:
    epoch: int
    bad: bool

class ThroughputDirection(IntEnum):
    IN = 0
    OUT = 1

@dataclass(frozen=True)
class Throughput:
    capability: str
    direction: ThroughputDirection
    throughput: float

@dataclass(frozen=True)
class ThroughputTM:
    mean: float
    var: float
    n: int

@dataclass(frozen=True)
class LastPingTM:
    ping: int

@dataclass(frozen=True)
class TrustModelUpdate:
    time: datetime
    edge_id: str
    cr: ChallengeResponse
    tm_from: EdgeResourceTM
    tm_to: EdgeResourceTM

@dataclass(frozen=True)
class LastPingUpdate:
    time: datetime
    edge_id: str
    tm_from: EdgeResourceTM
    tm_to: EdgeResourceTM

@dataclass(frozen=True)
class BadPingUpdate:
    time: datetime
    edge_id: str
    last_ping: int

@dataclass(frozen=True)
class Coordinate:
    latitude: float
    longitude: float

@dataclass(frozen=True)
class MonitoringTask:
    length: int

@dataclass(frozen=True)
class RoutingTask:
    length: int
    source: Coordinate
    destination: Coordinate

@dataclass(frozen=True)
class Task:
    target: ipaddress.IPv6Address
    time: datetime
    details: Union[MonitoringTask, RoutingTask]

@dataclass(frozen=True)
class TrustValue:
    target: str
    capability: str
    time: datetime
    value: float

class TrustChoose:
    pass

class TrustChooseBanded(TrustChoose):
    def __init__(self):
        self.values = []

class ReputationReceiveResult(Enum):
    SUCCESS = 0
    MISSING_KEY = 1
    OOM = 2
    QUEUE_FAIL = 3
    VERIFY_OOM_FAIL = 4

class ReputationSendResult(Enum):
    SUCCESS = 0
    SERIALISE_FAIL = 1
    QUEUE_FAIL = 2
    SIGN_FAIl = 3
    SEND_FAIL = 4
    OOM = 5

class KeystoreAddResult(Enum):
    SUCCESS = 0
    OOM1 = 1
    FAILED_FREE = 2
    OOM2 = 3
    ENCODE_FAIL = 4
    ENQUEUE_FAIL = 5
    VERIFY_FAIL = 6

class ChallengeResponseAnalyser:
    RE_TRUST_UPDATING_CR = re.compile(r'Updating Edge ([0-9A-Za-z]+) TM cr \(type=([0-9]),good=([01])\): EdgeResourceTM\(epoch=([0-9]+),(blacklisted|bad)=([01])\) -> EdgeResourceTM\(epoch=([0-9]+),(blacklisted|bad)=([01])\)')

    RE_TRUST_UPDATING_THROUGHPUT = re.compile(r'Updating Edge ([0-9A-Za-z]+) capability ([A-Za-z]+) TM throughput ([A-Za-z]+) \(([0-9]+) bytes/tick\): N\(mean=([0-9\.]+),var=([0-9\.]+),n=([0-9]+)\) -> N\(mean=([0-9\.]+),var=([0-9\.]+),n=([0-9]+)\)')
    RE_TRUST_UPDATING_LAST_PING = re.compile(r'Updating Edge ([0-9A-Za-z]+) TM last ping: ([0-9])+ -> ([0-9])')
    RE_TRUST_UPDATING_BAD_PING = re.compile(r'Setting trust value to 0 as we haven\'t received a ping recently \(time since last ping = ([0-9]+) sec')
    #RE_TRUST_UPDATING_TASK_SUBMISSION = re.compile(r'Updating Edge ([0-9A-Za-z]+) capability ([0-9A-Za-z]+) TM task_submission \(req=([0-9]+), coap=([0-9]+)\): Beta\(alpha=([0-9]+),beta=([0-9]+)\) -> Beta\(alpha=([0-9]+),beta=([0-9]+)\)')
    #RE_TRUST_UPDATING_TASK_RESULT = re.compile(r'Updating Edge ([0-9A-Za-z]+) capability ([0-9A-Za-z]+) TM task_result \(result=([0-9]+)\): Beta\(alpha=([0-9]+),beta=([0-9]+)\) -> Beta\(alpha=([0-9]+),beta=([0-9]+)\)')
    #RE_TRUST_UPDATING_RESULT_QUALITY = re.compile(r'Updating Edge ([0-9A-Za-z]+) capability ([0-9A-Za-z]+) TM result_quality \(good=([0-9]+)\): Beta\(alpha=([0-9]+),beta=([0-9]+)\) -> Beta\(alpha=([0-9]+),beta=([0-9]+)\)')


    RE_ROUTING_GENERATED = re.compile(r'Generated message \(len=([0-9]+)\) for path from \(([0-9.-]+),([0-9.-]+)\) to \(([0-9.-]+),([0-9.-]+)\)')
    RE_MONITORING_GENERATED = re.compile(r'Generated message \(len=([0-9]+)\)')

    RE_TASK_SENT = re.compile(r'Message sent to coap://\[(.+)\]:5683')

    RE_CHOOSE_BANDED_TRUST_VALUE = re.compile(r'Trust value for edge ([0-9A-Fa-f]+) and capability ([0-9A-Za-z]+)=([0-9.-]+) at ([0-9]+)/([0-9]+)')


    RE_TRUST_RCV_RECEIVED = re.compile(r'Received trust info via POST from (.+) of length [0-9]+ \(mid=([0-9]+)\)')
    RE_TRUST_RCV_MISSING_KEY = re.compile(r'Missing public key, need to request it \(mid=([0-9]+)\)')
    RE_TRUST_RCV_OOM = re.compile(r'res_trust_post_handler: out of memory \(mid=([0-9]+)\)')
    RE_TRUST_RCV_VERIFY_FAILED = re.compile(r'res_trust_post_handler: queue verify failed \(mid=([0-9]+)\)')
    RE_TRUST_RCV_SUCCESS = re.compile(r'res_trust_post_handler: successfully queued trust to be verified from (.+) \(mid=([0-9]+)\)')

    RE_KEYSTORE_ADD_OOM1 = re.compile(r'keystore_add: out of memory \(1st\) for ([0-9A-Fa-f]+)')
    RE_KEYSTORE_ADD_FAILED_FREE = re.compile(r'Failed to free space for the certificate ([0-9A-Fa-f]+)')
    RE_KEYSTORE_ADD_OOM2 = re.compile(r'keystore_add: out of memory \(2nd\) for ([0-9A-Fa-f]+)')
    RE_KEYSTORE_ADD_ENCODE_FAIL = re.compile(r'keystore_add: encode failed [0-9]+ > [0-9]+ for ([0-9A-Fa-f]+)')
    RE_KEYSTORE_ADD_ENQUEUE_FAIL = re.compile(r'keystore_add: enqueue failed for ([0-9A-Fa-f]+)')
    RE_KEYSTORE_ADD_VERIFY_FAIL = re.compile(r'Failed to verify public key for ([0-9A-Fa-f]+) \(sig verification failed\)')
    RE_KEYSTORE_ADD_VERIFY_SUCCESS = re.compile(r'Successfully verified public key for ([0-9A-Fa-f]+)')

    KEYSTORE_ADD_TO_RESULT = {
        RE_KEYSTORE_ADD_OOM1: KeystoreAddResult.OOM1,
        RE_KEYSTORE_ADD_FAILED_FREE: KeystoreAddResult.FAILED_FREE,
        RE_KEYSTORE_ADD_OOM2: KeystoreAddResult.OOM2,
        RE_KEYSTORE_ADD_ENCODE_FAIL: KeystoreAddResult.ENCODE_FAIL,
        RE_KEYSTORE_ADD_ENQUEUE_FAIL: KeystoreAddResult.ENQUEUE_FAIL,
        RE_KEYSTORE_ADD_VERIFY_FAIL: KeystoreAddResult.VERIFY_FAIL,
        RE_KEYSTORE_ADD_VERIFY_SUCCESS: KeystoreAddResult.SUCCESS,
    }

    def __init__(self, hostname):
        self.hostname = hostname

        self.start_time = None
        self.end_time = None

        self.tm_updates = []

        self.tasks = []
        self._pending_tasks = {}

        self.trust_choose = None

        self.reputation_receive_from = {}
        self.reputation_receive_result = defaultdict(Counter)

        self.reputation_send_count = Counter()

        self.keystore_add_count = defaultdict(Counter)
        self.keystore_add_first_time = {}

    def analyse(self, f):
        for (time, log_level, module, line) in parse_contiki(f):

            if self.start_time is None:
                self.start_time = time

            self.end_time = time

            if module == "trust-comm":
                if line.startswith("Updating Edge"):
                    if "TM cr" in line:
                        self._process_updating_edge_cr(time, log_level, module, line)
                    elif "TM throughput" in line:
                        self._process_updating_edge_throughput(time, log_level, module, line)
                    # elif "TM last ping" in line:
                    #     self._process_updating_edge_last_ping(time, log_level, module, line)
                    # elif "TM task_submission" in line:
                    #     self._process_updating_edge_task_submission(time, log_level, module, line)
                    # elif "TM task_result" in line:
                    #     self._process_updating_edge_task_result(time, log_level, module, line)
                    # elif "TM result_quality" in line:
                    #     self._process_updating_edge_result_quality(time, log_level, module, line)
                    else:
                        # Other trust model
                        pass
                else:
                    pass

            elif module == "A-cr":
                pass

            elif module == "A-routing":
                if line.startswith("Generated message"):
                    m = self.RE_ROUTING_GENERATED.match(line)
                    m_len = int(m.group(1))
                    m_source_lat = float(m.group(2))
                    m_source_lon = float(m.group(3))
                    m_dest_lat = float(m.group(4))
                    m_dest_lon = float(m.group(5))

                    source = Coordinate(m_source_lat, m_source_lon)
                    destination = Coordinate(m_dest_lat, m_dest_lon)

                    self._pending_tasks[module] = RoutingTask(m_len, source, destination)

                elif line.startswith("Message sent to"):
                    task_details = self._pending_tasks[module]
                    del self._pending_tasks[module]

                    m = self.RE_TASK_SENT.match(line)
                    m_target = ipaddress.ip_address(m.group(1))

                    t = Task(m_target, time, task_details)
                    self.tasks.append(t)

            elif module == "A-envmon":
                if line.startswith("Generated message"):
                    m = self.RE_MONITORING_GENERATED.match(line)
                    m_len = int(m.group(1))

                    self._pending_tasks[module] = MonitoringTask(m_len)

                elif line.startswith("Message sent to"):
                    task_details = self._pending_tasks[module]
                    del self._pending_tasks[module]

                    m = self.RE_TASK_SENT.match(line)
                    m_target = ipaddress.ip_address(m.group(1))

                    t = Task(m_target, time, task_details)
                    self.tasks.append(t)

            elif module == "trust-band":
                if self.trust_choose is None:
                    self.trust_choose = TrustChooseBanded()

                if line.startswith("Trust value for edge"):
                    m = self.RE_CHOOSE_BANDED_TRUST_VALUE.match(line)
                    m_edge = m.group(1)
                    m_capability = m.group(2)
                    m_value = float(m.group(3))

                    v = TrustValue(m_edge, m_capability, time, m_value)
                    self.trust_choose.values.append(v)

            # trust dissemination as reputation
            elif module == "trust":
                #print((time, log_level, module, line))

                if line.startswith("Received trust info via POST from"):
                    m = self.RE_TRUST_RCV_RECEIVED.match(line)
                    m_coap_addr = m.group(1)
                    m_mid = int(m.group(2))

                    assert m_mid not in self.reputation_receive_from

                    self.reputation_receive_from[m_mid] = m_coap_addr

                elif line.startswith("Missing public key, need to request it"):
                    m = self.RE_TRUST_RCV_MISSING_KEY.match(line)
                    m_mid = int(m.group(1))

                    self.reputation_receive_result[self.reputation_receive_from[m_mid]][ReputationReceiveResult.MISSING_KEY] += 1

                    del self.reputation_receive_from[m_mid]

                elif line.startswith("res_trust_post_handler: out of memory"):
                    m = self.RE_TRUST_RCV_OOM.match(line)
                    m_mid = int(m.group(1))

                    self.reputation_receive_result[self.reputation_receive_from[m_mid]][ReputationReceiveResult.OOM] += 1

                    del self.reputation_receive_from[m_mid]

                elif line.startswith("res_trust_post_handler: queue verify failed"):
                    m = self.RE_TRUST_RCV_VERIFY_FAILED.match(line)
                    m_mid = int(m.group(1))

                    self.reputation_receive_result[self.reputation_receive_from[m_mid]][ReputationReceiveResult.VERIFY_OOM_FAIL] += 1

                    del self.reputation_receive_from[m_mid]

                elif line.startswith("res_trust_post_handler: successfully queued trust to be verified from"):
                    m = self.RE_TRUST_RCV_SUCCESS.match(line)
                    m_coap_addr = m.group(1)
                    m_mid = int(m.group(2))

                    assert self.reputation_receive_from[m_mid] == m_coap_addr

                    self.reputation_receive_result[self.reputation_receive_from[m_mid]][ReputationReceiveResult.SUCCESS] += 1

                    del self.reputation_receive_from[m_mid]

                elif line == "Periodic broadcast of trust information disabled":
                    self.reputation_send_count = None

                elif line == "Cannot allocate memory for periodic_action trust request":
                    self.reputation_send_count[ReputationSendResult.OOM] += 1

                elif line.startswith("trust periodic_action: serialise_trust failed"):
                    self.reputation_send_count[ReputationSendResult.SERIALISE_FAIL] += 1

                elif line.startswith("trust periodic_action: Unable to sign message"):
                    self.reputation_send_count[ReputationSendResult.QUEUE_FAIL] += 1

                elif line.startswith("trust_tx_continue: Sign of trust information failed"):
                    self.reputation_send_count[ReputationSendResult.SIGN_FAIl] += 1

                elif line.startswith("trust_tx_continue: coap_send_request trust failed"):
                    self.reputation_send_count[ReputationSendResult.SEND_FAIL] += 1

                elif line.startswith("trust_tx_continue: coap_send_request trust done"):
                    self.reputation_send_count[ReputationSendResult.SUCCESS] += 1

            elif module == "keystore":
                for (re_keystore_add, status) in self.KEYSTORE_ADD_TO_RESULT.items():
                    m = re_keystore_add.match(line)
                    if m:
                        m_eui64 = m.group(1)
                        self.keystore_add_count[m_eui64][status] += 1

                        if (m_eui64, status) not in self.keystore_add_first_time:
                            self.keystore_add_first_time[(m_eui64, status)] = time

                        break

            #if module not in ("A-cr", "trust-comm"):
            #    continue

            #print((time, module, line))

    def _process_updating_edge_cr(self, time, log_level, module, line):
        # print(line)
        m = self.RE_TRUST_UPDATING_CR.match(line)
        if m is None:
            raise RuntimeError(f"Failed to parse '{line}'")

        m_edge_id = m.group(1)
        m_cr_type = ChallengeResponseType(int(m.group(2)))
        m_cr_good = True if int(m.group(3)) == 1 else False
        m_tm_from_epoch = int(m.group(4))
        m_tm_from_bad = True if int(m.group(5)) == 1 else False
        m_tm_to_epoch = int(m.group(6))
        m_tm_to_bad = True if int(m.group(7)) == 1 else False

        cr = ChallengeResponse(m_cr_type, m_cr_good)

        tm_from = EdgeResourceTM(m_tm_from_epoch, m_tm_from_bad)
        tm_to = EdgeResourceTM(m_tm_to_epoch, m_tm_to_bad)

        u = TrustModelUpdate(time, m_edge_id, cr, tm_from, tm_to)

        self.tm_updates.append(u)

    def _process_updating_edge_throughput(self, time, log_level, module, line):
        # print(line)
        m = self.RE_TRUST_UPDATING_THROUGHPUT.match(line)
        if m is None:
            raise RuntimeError(f"Failed to parse '{line}'")
        m_edge_id = m.group(1)  #e.g. f4ce36b29cb59ade
        m_capability_type = m.group(2) #e.g. envmon
        m_direction = m.group(3) #e.g. out
        m_throughput = m.group(4) #e.g.
        m_previous_mean = m.group(5) #e.g. 42.437496
        m_previous_var = m.group(6) #e.g. 425.286224
        m_previous_number = m.group(7) #e.g. 32
        m_current_mean = m.group(8) #e.g. 43.242420
        m_current_var = m.group(9) #e.g. 433.376831
        m_current_number = m.group(10) #e.g. 33

        cr = Throughput(m_capability_type, m_direction, m_throughput)
        tm_from = ThroughputTM(m_previous_mean, m_previous_var, m_previous_number)
        tm_to = ThroughputTM(m_current_mean, m_current_var, m_current_number)

        u = TrustModelUpdate(time, m_edge_id, cr, tm_from, tm_to)

        self.tm_updates.append(u)

    def _process_updating_edge_last_ping(self, time, log_level, module, line):
        m = self.RE_TRUST_UPDATING_LAST_PING.match(line)
        if m is None:
            raise RuntimeError(f"Failed to parse '{line}'")
        m_edge_id = m.group(1)  #e.g. f4ce36b29cb59ade
        m_previous_ping = m.group(2) #e.g. 272676
        m_current_ping = m.group(3) #e.g. 275228
        tm_from = LastPingTM(m_previous_ping)
        tm_to = LastPingTM(m_current_ping)

        u = LastPingUpdate(time, m_edge_id, tm_from, tm_to)
        #u = TrustModelUpdate(time, m_edge_id, cr, tm_from, tm_to) #we don't have cr

        self.tm_updates.append(u)

    def _process_updating_edge_bad_ping(self, time, log_level, module, line):
        m = self.RE_TRUST_UPDATING_BAD_PING.match(line)
        if m is None:
            raise RuntimeError(f"Failed to parse '{line}'")
        m_last_ping = m.group(1)  #e.g. 18

        # u = TrustModelUpdate(time, m_edge_id, cr, tm_from, tm_to) ####################################################### cr, tm_from, tm_to?
        u = BadPingUpdate(time, m_edge_id, last_ping)
        self.tm_updates.append(u)

    """def _process_updating_edge_task_submission(self, time, log_level, module, line):
        m = self.RE_TRUST_UPDATING_TASK_SUBMISSION.match(line)
        if m is None:
            raise RuntimeError(f"Failed to parse '{line}'")
        m_edge_id = m.group(1)  #e.g. f4ce36b29cb59ade
        m_capability_type = m.group(2) #e.g. envmon
        m_req = m.group(3) #e.g. 0
        m_coap = m.group(4) #e.g. 69
        m_previous_alpha = m.group(5) #e.g. 7
        m_previous_beta = m.group(6) #e.g. 1
        m_current_alpha = m.group(7) #e.g. 8
        m_current_beta = m.group(8) #e.g. 1

    def _process_updating_edge_task_result(self, time, log_level, module, line):
        m = self.RE_TRUST_UPDATING_TASK_RESULT.match(line)
        if m is None:
            raise RuntimeError(f"Failed to parse '{line}'")
        m_edge_id = m.group(1)  #e.g. f4ce36b29cb59ade
        m_capability_type = m.group(2) #e.g. envmon
        m_result = m.group(3) #e.g. 0
        m_previous_alpha = m.group(4) #e.g. 7
        m_previous_beta = m.group(5) #e.g. 1
        m_current_alpha = m.group(6) #e.g. 8
        m_current_beta = m.group(7) #e.g. 1

    def _process_updating_edge_result_quality(self, time, log_level, module, line):
        m = self.RE_TRUST_UPDATING_RESULT_QUALITY.match(line)
        if m is None:
            raise RuntimeError(f"Failed to parse '{line}'")
        m_edge_id = m.group(1)  #e.g. f4ce36b29cb59ade
        m_capability_type = m.group(2) #e.g. envmon
        m_result_quality = m.group(3) #e.g. 1
        m_previous_alpha = m.group(4) #e.g. 7
        m_previous_beta = m.group(5) #e.g. 1
        m_current_alpha = m.group(6) #e.g. 8
        m_current_beta = m.group(7) #e.g. 1"""



def main(log_dir: pathlib.Path):
    print(f"Looking for results in {log_dir}")

    gs = log_dir.glob("*.pyterm.log")

    results = {}

    for g in gs:
        print(f"Processing {g}...")
        bg = os.path.basename(g)

        kind, hostname, cr, log = bg.split(".", 3)

        if kind != "wsn":
            print("Can only have challenge_response results from an wsn node")
            continue

        a = ChallengeResponseAnalyser(hostname)

        with open(g, 'r') as f:
            a.analyse(f)

        results[hostname] = a

    return results

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description='Parse Challenge Response')
    parser.add_argument('--log-dir', type=pathlib.Path, default="results", help='The directory which contains the log output')

    args = parser.parse_args()

    results = main(args.log_dir)

    for (hostname, a) in results.items():
        print(hostname)
        print("Reputation send:")
        pprint(a.reputation_send_count)

        print("Reputation receive:")
        pprint(dict(a.reputation_receive_result))

        print("Keystore add:")
        pprint(dict(a.keystore_add_count))

        print("Keystore first:")
        for ((eui64, status), time) in a.keystore_add_first_time.items():
            print(eui64, status, time - a.start_time)

        print("Duration:", a.end_time - a.start_time)

        print()
