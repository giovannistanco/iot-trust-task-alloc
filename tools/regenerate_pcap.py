#!/usr/bin/env python3

import os
#import json
import itertools
from datetime import datetime

#from resource_rich.root.keystore import Keystore

#import pyshark
from common.inmem_capture import LinkTypes, InMemCapture
from common.packet_log_processor import PacketLogProcessor

#hostname_to_ips = {
#    "wsn1": "fd00::1",
#    "wsn2": "fd00::212:4b00:14d5:2bd6", # 00:12:4B:00:14:D5:2B:D6
#    "wsn3": "fd00::212:4b00:14d5:2ddb", # 00:12:4B:00:14:D5:2D:DB
#    "wsn4": "fd00::212:4b00:14d5:2be6", # 00:12:4B:00:14:D5:2B:E6
#    "wsn5": "fd00::212:4b00:14b5:da27", # 00:12:4B:00:14:B5:DA:27
#    "wsn6": "fd00::212:4b00:14d5:2f05", # 00:12:4B:00:14:D5:2F:05
#}

#def write_oscore_context_pref(log_dir):
#    # Need to write a file that allows wireshark to decrypt OSCORE messages
#    # File is comma separated with # as a comment, e.g.,
#    """
#    # This file is automatically generated, DO NOT MODIFY.
#    "1","1","1","1","","AES-CCM-16-64-128 (CCM*)"
#    "1","1","1","1","","AES-CCM-16-64-128 (CCM*)"
#    """
#    # Parameters are:
#    # Sender ID, Recipient ID, Master Secret, Master Salt, Algorithm
#    # See: https://github.com/wireshark/wireshark/blob/master-3.2/epan/dissectors/packet-oscore.c#L828
#
#    aiocoap_to_tshark_algorithm = {
#        "AES-CCM-16-64-128": "AES-CCM-16-64-128 (CCM*)"
#    }
#
#    keystore = Keystore(f"{log_dir}/keystore")
#
#    with open(os.path.abspath(f'{log_dir}/keystore/oscore.contexts.uat'), "w") as tshark_oscore_conf:
#
#        salt = "642b2b8e9d0c4263924ceafcf7038b26"
#        oscore_id_context = ""
#        algorithm = "AES-CCM-16-64-128"
#
#        """for context in os.listdir(f"{log_dir}/keystore/oscore-contexts"):
#            with open(f"{log_dir}/keystore/oscore-contexts/{context}/secret.json") as secret:
#                ctxdet = json.load(secret)
#
#            salt = ctxdet["salt_hex"]
#            algorithm = ctxdet["algorithm"]
#
#            line = [
#                ctxdet["sender-id_hex"],
#                ctxdet["recipient-id_hex"],
#                ctxdet["secret_hex"],
#                ctxdet["salt_hex"],
#                "",
#                aiocoap_to_tshark_algorithm[ctxdet["algorithm"]]
#            ]
#
#            print(','.join(f'"{v}"' for v in line), file=tshark_oscore_conf)"""
#
#        for sender, recipient in itertools.permutations(hostname_to_ips.values(), 2):
#            if sender == recipient:
#                continue
#
#            line = [
#                keystore.oscore_ident(sender).hex(),
#                keystore.oscore_ident(recipient).hex(),
#                keystore.shared_secret(sender, recipient).hex(),
#                salt,
#                oscore_id_context,
#                aiocoap_to_tshark_algorithm[algorithm]
#            ]
#
#            print(','.join(f'"{v}"' for v in line), file=tshark_oscore_conf)


def main(source, dest):
    #write_oscore_context_pref(log_dir)

    #override_prefs={
    #    'oscore.contexts': os.path.abspath(f'{log_dir}/keystore/oscore.contexts.uat')
    #}
    #override_prefs=override_prefs, 

    plp = PacketLogProcessor()
    with open(source, "r") as f:
        l, kinds, times = plp.process_all(f)

    custom_parameters = {"-w": dest}

    with InMemCapture(debug=True, linktype=LinkTypes.IEEE802_15_4_NOFCS, custom_parameters=custom_parameters) as c:
        # This will not reassemble fragments into a single packet
        packets = c.parse_packets(l, times)

        if len(packets) != len(kinds):
            raise RuntimeError("Invalid length")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description='Regenerate pcap files from the raw log')
    parser.add_argument('--source', type=str, required=True, help='The file which contains the raw pcap log output')
    parser.add_argument('--dest', type=str, required=True, help='The output pcap file')

    args = parser.parse_args()

    main(args.source, args.dest)