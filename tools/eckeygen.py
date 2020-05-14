#!/usr/bin/env python3

"""
This document outputs the equivalent of the following, but also in a format that can be used in C files.
```bash
openssl ecparam -name prime256v1 -genkey -noout -out private.pem
openssl ec -in private.pem -pubout -out public.pem
```
"""

from textwrap import wrap

from hashlib import sha256

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

# From: https://stackoverflow.com/questions/312443/how-do-you-split-a-list-into-evenly-sized-chunks
def chunks(lst, n):
    """Yield successive n-sized chunks from lst."""
    for i in range(0, len(lst), n):
        yield lst[i:i + n]


def save_key(pk, name):
    """From: https://stackoverflow.com/questions/45146504/python-cryptography-module-save-load-rsa-keys-to-from-file"""

    if name is not None:
        prefix = name.replace(":", "_") + "-"
    else:
        prefix = ""

    pem = pk.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption()
    )
    with open(f"{prefix}private.pem", 'wb') as pem_out:
        pem_out.write(pem)


    pem = pk.public_key().public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )
    with open(f"{prefix}public.pem", 'wb') as pem_out:
        pem_out.write(pem)


def format_individual(number, size, line_group_size=None):
    hex_num = hex(number).upper()[2:]

    wrapped = [f"0x{part}" for part in wrap(hex_num, size)]

    if line_group_size is None:
        return ", ".join(wrapped)
    else:
        chunked = list(chunks(wrapped, line_group_size))
        return ",\n                   ".join([", ".join(chunk) for chunk in chunked])

def contiking_format(private_key):
    public_key_nums = private_key.public_key().public_numbers()
    private_value = private_key.private_numbers().private_value

    private_key_hex_formatted = format_individual(private_value, 8)
    public_key_nums_x_formatted = format_individual(public_key_nums.x, 8)
    public_key_nums_y_formatted = format_individual(public_key_nums.y, 8)

    print(f"const uint32_t private[8] = {{ {private_key_hex_formatted} }};")
    print(f"const uint32_t publicx[8] = {{ {public_key_nums_x_formatted} }};")
    print(f"const uint32_t publicy[8] = {{ {public_key_nums_y_formatted} }};")

    private_key_hex_formatted = format_individual(private_value, 2, line_group_size=8)
    public_key_nums_x_formatted = format_individual(public_key_nums.x, 2, line_group_size=8)
    public_key_nums_y_formatted = format_individual(public_key_nums.y, 2, line_group_size=8)

    print(f"""static const dtls_ecdsa_secp256r1_key_t key_data = {{ // {' '.join(sys.argv)}
    .priv_key  = {{ {private_key_hex_formatted} }},
    .pub_key_x = {{ {public_key_nums_x_formatted} }},
    .pub_key_y = {{ {public_key_nums_y_formatted} }}
}};""")

    print(f"""const dtls_ecdsa_key_t key = {{
    .curve     = DTLS_ECDH_CURVE_SECP256R1,
    .priv_key  = key_data.priv_key,
    .pub_key_x = key_data.pub_key_x,
    .pub_key_y = key_data.pub_key_y,
}};""")

def main(deterministic_string):
    if deterministic_string is None:
        private_key = ec.generate_private_key(ec.SECP256R1(), default_backend())
    else:
        print(f"Generating deterministic key using {deterministic_string}")

        private_value = int.from_bytes(sha256(deterministic_string.encode("utf-8")).digest(), byteorder="little", signed=False)

        private_key = ec.derive_private_key(private_value, ec.SECP256R1(), default_backend())

    contiking_format(private_key)
    save_key(private_key, deterministic_string)

if __name__ == "__main__":
    import argparse
    import sys

    parser = argparse.ArgumentParser(description='ECC Keygen')
    parser.add_argument('-d', '--deterministic', type=str, default=None, help='The deterministic string to use.')

    args = parser.parse_args()

    main(args.deterministic)