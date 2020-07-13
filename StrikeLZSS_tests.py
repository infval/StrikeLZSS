#!/usr/bin/env python3
from pathlib import Path
import os
import zlib

def get_crc(filename):
    return zlib.crc32(Path(filename).read_bytes())

roms = (
    "Desert Strike - Return to the Gulf (UE) [!].gen",
    "Jungle Strike (UE) [!].gen",
    "Urban Strike (UE) [!].gen"
)

offsets = (
    (
        0xAB92A, 0xD5444, 0xA8E62, 0xDF768,
        0xE19FE, 0xEE4A8, 0xF1686, 0xE4108,
        0xE678C, 0xE9646, 0xEC542, 0xF188A,
        0xF481A, 0xAB08C, 0xAF4A4, 0xB05FE,
        0x1B02A, 0xB0A2A, 0xB3832, 0xB4F1C
    ),
    (
        0x1BD52A, 0x1BBE84, 0x0A64C8, 0x08547C,
        0x1BC250, 0x0A7C0C, 0x0AD69C, 0x0B0936,
        0x0B2C3C, 0x0BADB2, 0x0B249C, 0x0AE830,
        0x0B4D7E, 0x0B658C, 0x0BB300, 0x0BCB12,
        0x07EEA6, 0x08356E, 0x07FC22, 0x0BEE98,
        0x0BF83E, 0x1B907E, 0x1BA94A, 0x1D5080,
        0x0C0F84, 0x0D4FAE, 0x0D5AA4, 0x0D68BE,
        0x0D727A, 0x0D7C3A, 0x0D89DA, 0x0527BC,
        0x1C4D7E, 0x1DC7A0
    ),
    (
        0x1CB37A, 0x1CA8F4, 0x07D98C, 0x1CACA2,
        0x07E52A, 0x07ED46, 0x07F7EC, 0x081FBE,
        0x08E838, 0x0827CA, 0x1C9A2C, 0x1CA7FE,
        0x1E3F7A, 0x0AC330, 0x0AE620, 0x049C74
    )
)

failed = False
for rom, offs in zip(roms, offsets):
    for pos in offs:
        command = "StrikeLZSS -d \"{}\" test.bin -p 0x{:X}".format(rom, pos)
        print("#", command)
        os.system(command)
        print()
        crc1 = get_crc("test.bin")
        command = "StrikeLZSS -c test.bin test.bin".format(rom, pos)
        print("# Compress")
        os.system(command)
        print()
        # Check CRC
        command = "StrikeLZSS -d test.bin test.bin >nul".format(rom, pos)
        os.system(command)
        crc2 = get_crc("test.bin")
        if crc1 != crc2:
            print("!!! Wrong Decompression !!!")
            failed = True

input("<<< Tests ended >>> Result: {}".format("Failed" if failed else "OK"))
