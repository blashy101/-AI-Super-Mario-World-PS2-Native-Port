#!/usr/bin/env python3
"""Verify restored native lookup ranges against the user's clean USA ROM."""
import argparse,re
from pathlib import Path
from build_from_rom import validate_rom,ROOT
TABLES=[
 ('vendor/smw/src/smw_01.c','kSprXXX_FixedMovementCheepCheep_SwimmingXSpeed',0x01b01d,4),
 ('vendor/smw/src/smw_05.c','kSpr0EA_Layer2Scroll_DirectionToStartMoving',0x05ca5c,6),
 ('vendor/smw/src/smw_0d.c','kExtObj71_Canvas1_Tiles',0x0de05e,76),
 ('vendor/smw/src/smw_00.c','kPlayerGFXRt_PlayerXYDispIndexIndex',0x00dcec,71),
 ('vendor/smw/src/smw_00.c','kPlayerGFXRt_DATA_00E18E',0x00e18e,71),
]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom',type=Path);a=p.parse_args();rom,_=validate_rom(a.rom,'usa')
 for file,name,address,count in TABLES:
  text=(ROOT/file).read_text();m=re.search(r'static const uint8 '+name+r'\['+str(count)+r'\] = \{([^}]+)\};',text);assert m,name
  data=bytes(int(v,16) for v in re.findall(r'0x([a-fA-F0-9]+)',m[1]));offset=(address>>16)*0x8000+(address&0x7fff)
  assert data==rom[offset:offset+count],name;print(f'PASS: {name}, ${address:06X}, {count} original bytes')
if __name__=='__main__':main()
