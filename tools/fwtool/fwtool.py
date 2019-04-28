#!/usr/bin/env python3

# Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
#
# This file is subject to the terms and conditions defined in file 'LICENSE',
# which is part of this source code package.

from typing import Any,BinaryIO,Callable,List,Optional,Union

import argparse
import struct
import sys
import os

from intelhex import IntelHex
from binascii import crc32

SPA = argparse._SubParsersAction    # type alias
NS = argparse.Namespace             # type alias
AP = argparse.ArgumentParser        # type alias

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

class Firmware:
    SIZE_MAGIC = 0xff1234ff

    def __init__(self, fw:Union[bytearray,bytes,str,BinaryIO], be:Optional[bool]=None) -> None:
        self.base = None # type: Optional[int]
        if isinstance(fw, str):
            if fw.lower().endswith('.hex'):
                ih = IntelHex()
                ih.loadhex(fw)
                self.base = ih.minaddr()
                self.fw = bytearray(ih.tobinstr())
            else:
                with open(fw, 'rb') as f:
                    self.fw = bytearray(f.read())
        elif isinstance(fw, bytes):
            self.fw = bytearray(fw)
        elif isinstance(fw, bytearray):
            self.fw = fw
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

    def tofile(self, outfile:Union[str,BinaryIO], fmt:Optional[str]=None) -> None:
        if fmt is None and isinstance(outfile, str) and outfile.lower().endswith('.hex'):
            fmt = 'hex'
        else:
            fmt = 'bin'
        ih = IntelHex()
        ih.puts(self.base or 0, bytes(self.fw))
        ih.tofile(outfile, fmt)

    def verify(self) -> None:
        if self.hcrc != self.crc or self.hsize != self.size:
            raise ValueError("invalid firmware file")

    def __str__(self) -> str:
        return 'Firmware<%s,crc=0x%08x,size=%d>' % ('be' if self.be else 'le', self.crc, len(self.fw))

class Update:
    TYPE_PLAIN   = 0
    TYPE_LZ4     = 1
    TYPE_LZ4DICT = 2

    def __init__(self, fwsize:int, fwcrc:int, hwid:int, uptype:int, data:bytes, be:bool) -> None:
        self.fwsize = fwsize
        self.fwcrc = fwcrc
        self.hwid = hwid
        self.uptype = uptype
        self.data = data
        self.ep = '>' if be else '<'

        if (len(self.data) & 3) != 0:
            raise ValueError('invalid firmware update data length')

    def tobytes(self) -> bytes:
        hdr2 = struct.pack(self.ep + 'IIIHBB', self.fwcrc, self.fwsize, 0, 0, self.uptype, 0)
        crc = crc32(hdr2 + self.data)
        hdr1 = struct.pack(self.ep + 'II', crc, 24 + len(self.data))
        return hdr1 + hdr2 + self.data

    def tofile(self, outfile:str) -> None:
        with open(outfile, 'wb') as f:
            f.write(self.tobytes())

    @staticmethod
    def create(uptype:int, fw:Firmware, **kwargs:Any) -> 'Update':
        fw.verify()
        if uptype == Update.TYPE_PLAIN:
            return Update(fw.size, fw.crc, 0, uptype, bytes(fw.fw), fw.be)
        else:
            raise NotImplementedError()

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

        crc = crc32(upd[8:])

        if be is None:
            (lcrc,lsz) = struct.unpack_from('<II', upd)
            (bcrc,bsz) = struct.unpack_from('>II', upd)
            if (lcrc == crc and lsz == size):
                be = False
            elif (bcrc == crc and bsz == size):
                be = True
            else:
                raise ValueError('could not determine firmware endianness')

        (hcrc, hsize) = struct.unpack_from(('>' if be else '<') + 'II', upd, 0)

        if hcrc != crc:
            raise ValueError('invalid update crc')
        if hsize != size:
            raise ValueError('update size mismatch')

        raise NotImplementedError()

class Command:
    def create_parser(self, subs:SPA, name:str, desc:str) -> AP:
        p = subs.add_parser(name, description=desc, help=desc)
        p.set_defaults(func=self.main)
        return p

    def main(self, args:NS) -> int:
        raise NotImplementedError()

    @staticmethod
    def patch_value_options(p:AP) -> None:
        for arg, fmt, cnv, hlp in [
                ('int32', 'i', lambda x: int(x, 0), '32-bit signed integer'),
                ('uint32', 'I', lambda x: int(x, 0), '32-bit unsigned integer'),
                ]:
            p.add_argument('--patch-' + arg, type=PatchSpec.factory(fmt, cnv),
                    dest='patchspec', action='append', help='patch a ' + hlp)

class MkUpdateCommand(Command):
    def __init__(self, subs:SPA, prefix:str='') -> None:
        p = super().create_parser(subs, 'mkupdate', 'Create a firmware update file')
        p.add_argument('FWFILE', type=str, help='the firmware file to process')
        p.add_argument('UPFILE', type=str, help='the update file to create')
        self.prefix = prefix

    def main(self, args:NS) -> int:
        print('%sProcessing %s' % (self.prefix, args.FWFILE))
        fw = Firmware(args.FWFILE)
        up = Update.create(Update.TYPE_PLAIN, fw)
        print('%sCreating %s' % (self.prefix, args.UPFILE))
        up.tofile(args.UPFILE)
        return 0

class PatchFwCommand(Command):
    def __init__(self, subs:SPA, prefix:str='') -> None:
        p = super().create_parser(subs, 'patch', 'Patch a firmware file with CRC and length.')
        p.add_argument('FWFILE', type=str, help='the firmware to process')
        p.add_argument('--check-only', action='store_true', help='only check CRC and length values, do not patch')
        p.add_argument('--zfw', type=str, help='create a ZFW archive')
        Command.patch_value_options(p)
        self.prefix = prefix

    def main(self, args:NS) -> int:
        print('%sProcessing %s' % (self.prefix, args.FWFILE))
        fw = Firmware(args.FWFILE)
        if not args.check_only:
            if args.patchspec:
                fw.patch_values(args.patchspec)
            fw.patch()
            fw.tofile(args.FWFILE)
        print('%sCRC:  0x%08x (%s)' % (self.prefix, fw.hcrc, 'ok' if fw.hcrc == fw.crc else 'invalid'))
        print('%sSize: 0x%08x: %d bytes (%s)' % (self.prefix, fw.hsize, fw.hsize, 'ok' if fw.hsize == fw.size else 'invalid'))
        return 0 if fw.hcrc == fw.crc and fw.hsize == fw.size else 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    subs = parser.add_subparsers(dest='COMMAND')
    subs.required = True

    prefix = os.environ.get('STDOUT_PREFIX', '')

    PatchFwCommand(subs, prefix)
    MkUpdateCommand(subs, prefix)

    args = parser.parse_args()
    sys.exit(args.func(args))
