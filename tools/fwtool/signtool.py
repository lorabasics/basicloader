import sys
import struct
from Crypto.Hash import SHA256
from Crypto.PublicKey import ECC
from Crypto.Signature import DSS

if len(sys.argv) != 5:
    print('usage: signtool UPFILE KEYFILE PASSPHRASE OUTFILE')
else:
    upd = open(sys.argv[1], 'rb').read()
    usz = struct.unpack('<I', upd[4:8])[0]
    key = ECC.import_key(open(sys.argv[2]).read(), passphrase=sys.argv[3])

    hash = SHA256.new(upd[:usz])

    signer = DSS.new(key, 'deterministic-rfc6979')
    sig = signer.sign(hash)

    open(sys.argv[4], 'wb').write(upd + sig)
