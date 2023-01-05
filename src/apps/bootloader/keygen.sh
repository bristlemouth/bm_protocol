#!/bin/bash

IMGTOOL_PATH=../../third_party/mcuboot/scripts/imgtool.py

echo "Generating signing/encryption keys for development use"

echo "Generating ed25519 key"
# To generate signing key without a password
python3 $IMGTOOL_PATH keygen -k ed25519_key.pem -t ed25519
# To use a password
# python3 $IMGTOOL_PATH keygen -k ed25519_key.pem -t ed25519 -p

# To get public key
python3 $IMGTOOL_PATH getpub -k ed25519_key.pem > ed25519_pub_key.c


echo "Generating x25519 key"
# To generate encryption key without a password
python3 $IMGTOOL_PATH keygen -t x25519 -k x25519_key.pem
# To use a password
# python3 $IMGTOOL_PATH keygen -t x25519 -k x25519_key.pem -p

# To generate private key for encryption/decryption
python3 $IMGTOOL_PATH getpriv -k x25519_key.pem > x25519_priv_key.c

echo "done!"
