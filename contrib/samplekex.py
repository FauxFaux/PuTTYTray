#!/usr/bin/env python

# Example Python script to synthesise the server end of an SSH key exchange.

# This is an output-only script; you run it by means of saying
# something like
#
#   samplekex.py | nc -l 2222 | hex dump utility of your choice
#
# and then connecting PuTTY to port 2222. Being output-only, of
# course, it cannot possibly get the key exchange _right_, so PuTTY
# will terminate with an error when the signature in the final message
# doesn't validate. But everything until then should be processed as
# if it was a normal SSH-2 connection, which means you can use this
# script as a starting point for constructing interestingly malformed
# key exchanges to test bug fixes.

import sys, random
from encodelib import *

# A random Diffie-Hellman group, taken from an SSH server I made a
# test connection to.
groupgen = 5
group = 0xf5d3849d2092fd427b4ebd838ea4830397a55f80b644626320dbbe51e8f63ed88148d787c94e7e67e4f393f26c565e1992b0cff8a47a953439462a4d0ffa5763ef60ff908f8ee6c4f6ef9f32b9ba50f01ad56fe7ebe90876a5cf61813a4ad4ba7ec0704303c9bf887d36abbd6c2aa9545fc2263232927e731060f5c701c96dc34016636df438ce30973715f121d767cfb98b5d09ae7b86fa36a051ad3c2941a295a68e2f583a56bc69913ec9d25abef4fdf1e31ede827a02620db058b9f041da051c8c0f13b132c17ceb893fa7c4cd8d8feebd82c5f9120cb221b8e88c5fe4dc17ca020a535484c92c7d4bee69c7703e1fa9a652d444c80065342c6ec0fac23c24de246e3dee72ca8bc8beccdade2b36771efcc350558268f5352ae53f2f71db62249ad9ac4fabdd6dfb099c6cff8c05bdea894390f9860f011cca046dfeb2f6ef81094e7980be526742706d1f3db920db107409291bb4c11f9a7dcbfaf26d808e6f9fe636b26b939de419129e86b1e632c60ec23b65c815723c5d861af068fd0ac8b37f4c06ecbd5cb2ef069ca8daac5cbd67c6182a65fed656d0dfbbb8a430b1dbac7bd6303bec8de078fe69f443a7bc8131a284d25dc2844f096240bfc61b62e91a87802987659b884c094c68741d29aa5ca19b9457e1f9df61c7dbbb13a61a79e4670b086027f20da2af4f5b020725f8828726379f429178926a1f0ea03f

# An RSA key, generated specially for this script.
rsaexp = 0x10001
rsamod = 0xB98FE0C0BEE1E05B35FDDF5517B3E29D8A9A6A7834378B6783A19536968968F755E341B5822CAE15B465DECB80EE4116CF8D22DB5A6C85444A68D0D45D9D42008329619BE3CAC3B192EF83DD2B75C4BB6B567E11B841073BACE92108DA7E97E543ED7F032F454F7AC3C6D3F27DB34BC9974A85C7963C546662AE300A61CBABEE274481FD041C41D0145704F5FA9C77A5A442CD7A64827BB0F21FB56FDE388B596A20D7A7D1C5F22DA96C6C2171D90A673DABC66596CD99499D75AD82FEFDBE04DEC2CC7E1414A95388F668591B3F4D58249F80489646ED2C318E77D4F4E37EE8A588E79F2960620E6D28BF53653F1C974C91845F0BABFE5D167E1CA7044EE20D

# 16 bytes of random data for the start of KEXINIT.
cookie = "".join([chr(random.randint(0,255)) for i in range(16)])

sys.stdout.write(greeting("SSH-2.0-Example KEX synthesis"))

# Expect client to send KEXINIT

sys.stdout.write(
    clearpkt(SSH2_MSG_KEXINIT,
             cookie,
             name_list(("diffie-hellman-group-exchange-sha256",)), # kex
             name_list(("ssh-rsa",)), # host keys
             name_list(("aes128-ctr",)), # client->server ciphers
             name_list(("aes128-ctr",)), # server->client ciphers
             name_list(("hmac-sha2-256",)), # client->server MACs
             name_list(("hmac-sha2-256",)), # server->client MACs
             name_list(("none",)), # client->server compression
             name_list(("none",)), # server->client compression
             name_list(()), # client->server languages
             name_list(()), # server->client languages
             boolean(False), # first kex packet does not follow
             uint32(0)))

# Expect client to send SSH2_MSG_KEX_DH_GEX_REQUEST(0x1000)

sys.stdout.write(
    clearpkt(SSH2_MSG_KEX_DH_GEX_GROUP,
             mpint(group),
             mpint(groupgen)))

# Expect client to send SSH2_MSG_KEX_DH_GEX_INIT

sys.stdout.write(
    clearpkt(SSH2_MSG_KEX_DH_GEX_REPLY,
             ssh_rsa_key_blob(rsaexp, rsamod),
             mpint(random.randint(2, group-2)),
             ssh_rsa_signature_blob(random.randint(2, rsamod-2))))
