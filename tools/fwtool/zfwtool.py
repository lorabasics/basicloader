#!/usr/bin/env python3

# Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
#
# This file is subject to the terms and conditions defined in file 'LICENSE',
# which is part of this source code package.

from typing import Any,Callable,Dict,IO,List,Optional,Union

import click
import io
import json
import struct
import zipfile

from binascii import crc32
from contextlib import contextmanager
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
    def from_file(infile:Union[str,IO]) -> 'ZFWArchive':
        with zipfile.ZipFile(infile, 'r') as zfw:
            with zfw.open('info.json', 'r') as f:
                info = json.load(io.TextIOWrapper(f))
            with zfw.open('firmware.bin', 'r') as f:
                fw = Firmware(f, base=info.get('baseaddr'))
        return ZFWArchive(fw, ZFWArchive._filter_reserved(info))

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
    zfw = ZFWArchive.from_file(zfwfile)
    zfw.fw.tofile(firmware)

@click.command(help='Print information about a ZFW archive, where ZFWFILE is the input file')
@click.argument('ZFWFILE', type=click.File(mode='rb'))
def info(zfwfile:IO) -> None:
    zfw = ZFWArchive.from_file(zfwfile)
    fw = zfw.fw
    print('  CRC: 0x%08x (%s)' % (fw.hcrc, 'ok' if fw.hcrc == fw.crc else 'invalid'))
    print(' Size: 0x%08x: %d bytes (%s)' % (fw.hsize, fw.hsize, 'ok' if fw.hsize == fw.size else 'invalid'))
    print(' Base: %s' % ('0x%08x' % fw.base) if fw.base is not None else 'not specified')
    print(' Meta: %s' % ', '.join('%s=%r' % (k,v) for k,v in zfw.meta.items()))

@click.group()
def cli() -> None:
    pass
cli.add_command(create)
cli.add_command(export)
cli.add_command(info)

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
