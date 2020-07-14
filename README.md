# StrikeLZSS
LZSS compressor & decompressor for *Desert Strike*, *Jungle Strike*, *Urban Strike* (Sega Mega Drive/Genesis).
## Usage
### Editing ROM
Apply master code:
```
Desert Strike - 0FFFC4:4E71
Jungle Strike - 1FF2E0:4E75
 Urban Strike - 1FF0D4:4E75
```
### Searching compressed art
Set the execution (PC) breakpoint at:
```
Desert Strike - $006112
Jungle Strike - $04619C
 Urban Strike - $00762A
```
When execution stops, register `A2` contains the offset in the ROM.
### Program usage
Decompress:
```
StrikeLZSS -d input.bin output.bin -p 0x12AB
```
-p OFFSET - the starting position in the input file (hex - 0xAB or dec - 171).

Compress:
```
StrikeLZSS input.bin output.bin
```
