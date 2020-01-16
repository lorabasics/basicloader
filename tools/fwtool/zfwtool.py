#!/usr/bin/env python3

# Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
#
# This file is subject to the terms and conditions defined in file 'LICENSE',
# which is part of this source code package.

from typing import Any,BinaryIO,Callable,Dict,IO,List,Optional,Union

import click
import io
import json
import lz4.block
import struct
import zipfile

from Crypto.Hash import SHA256
from Crypto.PublicKey import ECC
from Crypto.Signature import DSS

from binascii import crc32
from contextlib import contextmanager
from hashlib import sha256
from intelhex import IntelHex

class PatchSpec:
    def __init__(self, off:int, val:Any, fmt:str) -> None:
        self.off = off
        self.val = val
        self.fmt = fmt

    @staticmethod
    def factory(fmt:str, convert:Callable[[Any],Any]) -> Callable[[str], 'PatchSpec']:
        def patchspec(spec:str) -> 'PatchSpec':
            off, val = spec.split(':')
            return PatchSpec(int(off, 0), convert(val), fmt)
        return patchspec

class IntParam(click.ParamType):
    name = 'number'

    def convert(self, value:Optional[str], param:Optional[click.Parameter], ctx:Optional[click.Context]) -> Any:
        if value is None:
            return None
        try:
            return int(value, 0)
        except:
            self.fail('%s is not a valid integer' % value)

class Firmware:
    SIZE_MAGIC = 0xff1234ff

    def __init__(self, fw:Union[bytearray,bytes,str,IO], base:Optional[int]=None, be:Optional[bool]=None) -> None:
        self.base = base
        if isinstance(fw, str):
            if fw.lower().endswith('.hex'):
                self._loadhex(fw)
            else:
                with open(fw, 'rb') as f:
                    self.fw = bytearray(f.read())
        elif isinstance(fw, bytes):
            self.fw = bytearray(fw)
        elif isinstance(fw, bytearray):
            self.fw = fw
        else:
            if hasattr(fw, 'name') and fw.name.lower().endswith('.hex'):
                self._loadhex(io.TextIOWrapper(fw))
            else:
                self.fw = bytearray(fw.read())

        self.size = len(self.fw)
        if self.size < 8 or (self.size & 3) != 0:
            raise ValueError('invalid firmware length')

        self.crc = crc32(self.fw[8:])

        if be is None:
            (lcrc,lsz) = struct.unpack_from('<II', self.fw)
            (bcrc,bsz) = struct.unpack_from('>II', self.fw)
            if (lcrc == self.crc and lsz == self.size) or lsz == Firmware.SIZE_MAGIC:
                self.be = False
            elif (bcrc == self.crc and bsz == self.size) or bsz == Firmware.SIZE_MAGIC:
                self.be = True
            else:
                raise ValueError('could not determine firmware endianness')
        else:
            self.be = be
        self.ep = '>' if be else '<'

        (self.hcrc, self.hsize) = struct.unpack_from(self.ep + 'II', self.fw)

    @staticmethod
    def _ishex(fw:Union[str,IO]) -> bool:
        if isinstance(fw, str):
            return fw.lower().endswith('.hex')
        elif hasattr(fw, 'name') and fw.name is not None:
            return fw.name.lower().endswith('.hex')
        else:
            # TODO we could peek at the file before giving up...
            return False

    def _loadhex(self, fw:Union[str,IO]) -> None:
        ih = IntelHex()
        ih.fromfile(fw, format='hex')
        self.fw = bytearray(ih.tobinstr())
        if self.base is None:
            self.base = ih.minaddr()
        else:
            if self.base != ih.minaddr():
                raise ValueError('inconsistent base address')

    def patch(self, crc:bool=True, size:bool=True) -> None:
        if crc:
            struct.pack_into(self.ep + 'I', self.fw, 0, self.crc)
            self.hcrc = self.crc
        if size:
            struct.pack_into(self.ep + 'I', self.fw, 4, self.size)
            self.hsize = self.size

    def patch_value(self, off:int, val:Any, fmt:str) -> None:
        struct.pack_into(self.ep + fmt, self.fw, off, val)
        self.crc = crc32(self.fw[8:])

    def patch_values(self, specs:List[PatchSpec]) -> None:
        for ps in specs:
            self.patch_value(ps.off, ps.val, ps.fmt)

    def tofile(self, outfile:Union[str,IO]) -> None:
        ih = IntelHex()
        ih.puts(self.base or 0, bytes(self.fw))
        if Firmware._ishex(outfile):
            if not isinstance(outfile, str):
                outfile = io.TextIOWrapper(outfile)
            ih.write_hex_file(outfile)
        else:
            ih.tobinfile(outfile)

    def verify(self) -> None:
        if self.hcrc != self.crc or self.hsize != self.size:
            raise ValueError("invalid firmware file")

    def __repr__(self) -> str:
        return 'Firmware<%s,crc=0x%08x,size=%d%s>' % ('be' if self.be else 'le', self.crc, len(self.fw),
                (',base=0x%08x' % self.base) if self.base is not None else '')

class ZFWArchive:
    META_RESERVED = ['baseaddr']

    def __init__(self, fw:Firmware, meta:Optional[Dict[str,Any]]=None) -> None:
        self.fw = fw
        #self.meta:Dict[str,Any] = meta or {}
        self.meta = meta or {}

    @staticmethod
    def _filter_reserved(d:Dict[str,Any]) -> Dict[str,Any]:
        return dict((k,v) for k,v in d.items() if k not in ZFWArchive.META_RESERVED)

    def write(self, outfile:Union[str,IO]) -> None:
        info = ZFWArchive._filter_reserved(self.meta)
        if self.fw.base is not None:
            info['baseaddr'] = self.fw.base
        with zipfile.ZipFile(outfile, 'w') as zfw:
            with zfw.open('firmware.bin', 'w') as f:
                self.fw.tofile(f)
            with zfw.open('info.json', 'w') as f:
                json.dump(info, io.TextIOWrapper(f))

    @staticmethod
    def fromfile(infile:Union[str,IO]) -> 'ZFWArchive':
        with zipfile.ZipFile(infile, 'r') as zfw:
            with zfw.open('info.json', 'r') as f:
                info = json.load(io.TextIOWrapper(f))
            with zfw.open('firmware.bin', 'r') as f:
                fw = Firmware(f, base=info.get('baseaddr'))
        return ZFWArchive(fw, ZFWArchive._filter_reserved(info))

class Update:
    TYPE_PLAIN    = 0
    TYPE_LZ4      = 1
    TYPE_LZ4DELTA = 2

    def __init__(self, fwsize:int, fwcrc:int, hwid:int, uptype:int, data:bytes, sigblob:bytes, be:bool) -> None:
        self.fwsize = fwsize
        self.fwcrc = fwcrc
        self.hwid = hwid
        self.uptype = uptype
        self.data = data
        self.sigblob = sigblob
        self.ep = '>' if be else '<'

        if (len(self.data) & 3) != 0:
            raise ValueError('invalid firmware update data length')

    def sign(self, signkey:ECC.EccKey, overwrite:bool=False) -> None:
        if overwrite:
            self.sigblob = b''
        sigsz = signkey.pointQ.size_in_bytes() * 2
        d,m = divmod(len(self.sigblob), sigsz)
        if m:
            self.sigblob += b'\0' * (sigsz - m)
        h = SHA256.new(self.tobytes(include_sigblob=False))
        self.sigblob += DSS.new(signkey, 'deterministic-rfc6979').sign(h)

    def tobytes(self, include_sigblob:bool=True) -> bytes:
        hdr2 = struct.pack(self.ep + 'IIIHBB', self.fwcrc, self.fwsize, 0, 0, self.uptype, 0)
        crc = crc32(hdr2 + self.data)
        hdr1 = struct.pack(self.ep + 'II', crc, 24 + len(self.data))
        update = hdr1 + hdr2 + self.data
        if include_sigblob:
            update += self.sigblob
        return update

    def tofile(self, outfile:Union[str,IO]) -> None:
        if isinstance(outfile, str):
            outfile = open(outfile, 'wb')
        outfile.write(self.tobytes())

    def unpack(self, ref:Firmware=None) -> Firmware:
        if self.uptype == Update.TYPE_PLAIN:
            fw = Firmware(self.data)
        elif self.uptype == Update.TYPE_LZ4:
            pad = self.data[-1]
            enc = self.data[:-pad]
            plain = lz4.block.decompress(enc, uncompressed_size=2*self.fwsize)
            fw = Firmware(plain)
        elif self.uptype == Update.TYPE_LZ4DELTA:
            ref.verify()
            (refcrc, refsize, blksz) = struct.unpack(self.ep + 'III', self.data[0:12])
            if refcrc != ref.crc or refsize != ref.size:
                raise ValueError("referenced firmware crc/size does not match")
            blockdata = self.data[12:]
            state = bytearray(max(self.fwsize, len(ref.fw)))
            state[:len(ref.fw)] = ref.fw
            while len(blockdata):
                (blkhash, blkidx, dictidx, dictlen, lz4len) = struct.unpack('8sBBHH', blockdata[:14])
                lz4data = blockdata[14 : 14 + lz4len]
                blockdata = blockdata[(14 + lz4len + 3) & ~3:]
                b = lz4.block.decompress(lz4data, uncompressed_size=blksz, dict=state[dictidx*blksz : dictidx*blksz + dictlen])
                if sha256(b).digest()[:8] != blkhash:
                    raise ValueError("bad block hash")
                #print(' blk #%02d: size=%d, hash=%s, dictidx=%02d, dictlen=%d, lz4len=%d'
                #      % (blkidx, len(b), blkhash.hex(), dictidx, dictlen, lz4len))
                state[blkidx*blksz : blkidx*blksz + len(b)] = b
            fw = Firmware(state[:self.fwsize])
        else:
            raise ValueError("unknown update type")
        fw.verify()
        return fw

    def verify(self, fw:Firmware, ref:Firmware=None) -> None:
        fw.verify()
        if self.fwcrc != fw.crc or self.fwsize != fw.size:
            raise ValueError("firmware mismatch")
        sfw = self.unpack(ref)
        if sfw.fw != fw.fw:
            raise ValueError("firmware content mismatch")

    @staticmethod
    def lz4enc(fw:bytes, wordpad=False, **kwargs) -> bytes:
        enc = lz4.block.compress(fw, mode='high_compression', compression=12, store_size=False, return_bytearray=True, **kwargs)
        if wordpad:
            pad = 4 - (len(enc) & 3)
            enc += bytearray([pad] * pad)
        return enc

    @staticmethod
    def createPlain(fw:Firmware) -> 'Update':
        fw.verify()
        return Update(fw.size, fw.crc, 0, Update.TYPE_PLAIN, bytes(fw.fw), b'', fw.be)

    @staticmethod
    def createCompressed(fw:Firmware) -> 'Update':
        fw.verify()
        return Update(fw.size, fw.crc, 0, Update.TYPE_LZ4, Update.lz4enc(bytes(fw.fw), wordpad=True), b'', fw.be)

    @staticmethod
    def createDelta(fw:Firmware, ref:Firmware, blksz:int) -> 'Update':
        fw.verify()
        ref.verify()
        nblocks = (len(fw.fw) + blksz - 1) // blksz
        state = bytearray(max(len(fw.fw), len(ref.fw)))
        state[:len(ref.fw)] = ref.fw
        updata = struct.pack(fw.ep + 'III', ref.crc, ref.size, blksz) # delta header
        if len(fw.fw) < len(ref.fw):
            blockrange = range(nblocks) # forwards
        else:
            blockrange = reversed(range(nblocks)) # backwards
        for blkidx in blockrange:
            fwblock = fw.fw[blkidx*blksz : (blkidx+1)*blksz] # last block might be shorter than blksz
            if fwblock != state[blkidx*blksz : blkidx*blksz + len(fwblock)]:
                blkhash = sha256(fwblock).digest()[:8]
                dictlen = min(len(ref.fw), 64*1024 - blksz)
                dictidx = max(0, min(blkidx - ((dictlen + blksz - 1) // blksz - 1) // 2, (len(ref.fw) - dictlen + blksz - 1) // blksz))
                dictlen = min(len(ref.fw) - dictidx * blksz, dictlen)
                lz4data = Update.lz4enc(fwblock, dict=bytes(state[dictidx*blksz : dictidx*blksz + dictlen]))
                updata += struct.pack(fw.ep + '8sBBHH', blkhash, blkidx, dictidx, dictlen, len(lz4data))
                updata += lz4data
                updata += bytearray((4 - (len(updata) & 3)) & 3) # align to word boundary
                state[blkidx*blksz : blkidx*blksz + len(fwblock)] = fwblock
                #print(' blk #%02d: size=%d, hash=%s, dictidx=%02d, dictlen=%d, lz4len=%d'
                #      % (blkidx, len(fwblock), blkhash.hex(), dictidx, dictlen, len(lz4data)))
        return Update(fw.size, fw.crc, 0, Update.TYPE_LZ4DELTA, bytes(updata), b'', fw.be)

    @staticmethod
    def fromfile(upf:Union[bytes,str,BinaryIO], be:Optional[bool]=None) -> 'Update':
        if isinstance(upf, str):
            with open(upf, 'rb') as f:
                upd = bytes(f.read())
        elif isinstance(upf, bytes):
            upd = upf
        else:
            upd = upf.read()

        size = len(upd)
        if size < 24 or (size & 3) != 0:
            raise ValueError('invalid update length')

        if be is None:
            (lcrc,lsz) = struct.unpack_from('<II', upd)
            (bcrc,bsz) = struct.unpack_from('>II', upd)
            if (lsz <= size and crc32(upd[8:lsz]) == lcrc):
                be = False
            elif (bsz <= size and crc32(upd[8:bsz]) == bcrc):
                be = True
            else:
                raise ValueError('could not determine firmware endianness')

        (hcrc, hsize) = struct.unpack_from(('>' if be else '<') + 'II', upd, 0)
        if hsize > size:
            raise ValueError('update size mismatch')
        crc = crc32(upd[8:hsize])
        if hcrc != crc:
            raise ValueError('invalid update crc')

        fwcrc, fwsize, hwidi, hwidh, uptype = struct.unpack_from(
                ('>' if be else '<') + 'IIIHB', upd, 8);

        return Update(fwsize, fwcrc, 0, uptype, upd[24:hsize], upd[hsize:], be)


@click.command(help='Create a ZFW archive, where FIRMWARE is the input file and ZFWFILE is the output file')
@click.option('--base', type=IntParam(), help='base address')
@click.option('--patch', is_flag=True, help='patch firmware size and CRC')
@click.option('--meta', type=click.Tuple([str,str]), multiple=True, help='add metadata')
@click.argument('FIRMWARE', type=click.File(mode='rb'))
@click.argument('ZFWFILE', type=click.File(mode='wb'))
def create(firmware:IO, zfwfile:IO, **kwargs:Any) -> None:
    fw = Firmware(firmware, base=kwargs['base'])
    if kwargs['patch']:
        fw.patch()
    meta:Dict[str,Any] = {}
    if kwargs['meta']:
        meta.update({ k:v for k,v in kwargs['meta'] })
    zfw = ZFWArchive(fw, meta=meta)
    zfw.write(zfwfile)

@click.command(help='Export a firmeare file from a ZFW archive, where ZFWFILE is the input file and FIRMWARE is the output file')
@click.argument('ZFWFILE', type=click.File(mode='rb'))
@click.argument('FIRMWARE', type=click.File(mode='wb'))
def export(zfwfile:IO, firmware:IO) -> None:
    zfw = ZFWArchive.fromfile(zfwfile)
    zfw.fw.tofile(firmware)

@click.command(help='Print information about a ZFW archive, where ZFWFILE is the input file')
@click.argument('ZFWFILE', type=click.File(mode='rb'))
def info(zfwfile:IO) -> None:
    zfw = ZFWArchive.fromfile(zfwfile)
    fw = zfw.fw
    print('  CRC: 0x%08x (%s)' % (fw.hcrc, 'ok' if fw.hcrc == fw.crc else 'invalid'))
    print(' Size: 0x%08x: %d bytes (%s)' % (fw.hsize, fw.hsize, 'ok' if fw.hsize == fw.size else 'invalid'))
    print(' Base: %s' % ('0x%08x' % fw.base) if fw.base is not None else 'not specified')
    print(' Meta: %s' % ', '.join('%s=%r' % (k,v) for k,v in zfw.meta.items()))

@click.command(help='Create a firmware update file, where ZFWFILE is the input file and UPFILE is the output file')
@click.argument('ZFWFILE', type=click.File(mode='rb'))
@click.argument('UPFILE', type=click.File(mode='wb'))
@click.option('-p', '--plain', is_flag=True, help='create plain uncompressed update')
@click.option('-d', '--deltafile', type=click.File(mode='rb'), help='create delta update using this firmware file as reference')
@click.option('-b', '--blksz', type=int, help='block size for delta update', default=4096)
@click.option('-s', '--signkey', type=click.File(mode='rb'), help='sign update with this key')
@click.option('--passphrase', help='passphrase for signing key')
def mkupdate(zfwfile:IO, upfile:IO, **kwargs:Any) -> None:
    fw = ZFWArchive.fromfile(zfwfile).fw
    if kwargs['plain']:
        up = Update.createPlain(fw)
        up.verify(fw)
    elif kwargs['deltafile']:
        rf = ZFWArchive.fromfile(kwargs['deltafile']).fw
        up = Update.createDelta(fw, rf, kwargs['blksz'])
        up.verify(fw, rf)
    else:
        up = Update.createCompressed(fw)
        up.verify(fw)

    if kwargs['signkey']:
        pp = kwargs['passphrase']
        if pp is not None and len(pp) == 0:
            pp = click.prompt(f"Enter passphrase for key {kwargs['signkey'].name}", hide_input=True)
        eckey = ECC.import_key(kwargs['signkey'].read(), pp)
        up.sign(eckey)

    up.tofile(upfile)
    print(' firmware size %d, update size %d, ratio %d%%'
            % (len(fw.fw), len(up.data), len(up.data) * 100 / len(fw.fw)))

@click.group()
def cli() -> None:
    pass
cli.add_command(create)
cli.add_command(export)
cli.add_command(info)
cli.add_command(mkupdate)

#    @staticmethod
#    def patch_value_options(p:AP) -> None:
#        for arg, fmt, cnv, hlp in [
#                ('int32', 'i', lambda x: int(x, 0), '32-bit signed integer'),
#                ('uint32', 'I', lambda x: int(x, 0), '32-bit unsigned integer'),
#                ]:
#            p.add_argument('--patch-' + arg, type=PatchSpec.factory(fmt, cnv),
#                    dest='patchspec', action='append', help='patch a ' + hlp)
#


if __name__ == '__main__':
    cli()
