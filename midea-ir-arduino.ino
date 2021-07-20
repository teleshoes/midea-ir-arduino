//Midea A/C IR Blaster
//Copyright 2021 Elliot Wolk
//License: GPLv3 or later

#include <IRLibAll.h>
#include <IRLib_HashRaw.h>
#include <IRLibCombo.h>

/*
 * Midea A/C IR Blaster
 * flash to an Arduino Nano or similar, with an I/R bulb on D3 pin
 *
 * send data via serial, e.g.:
 *   echo '#101100100100110110111011010001001111100000000111*' > /dev/ttyUSB0
 *
 * disable DTR first to avoid reset, e.g.:
 *   stty -F /dev/ttyUSB* -hupcl
 *
 * commands:
 *    #   start building the main segment
 *    @   start building the ext segment
 *    1   append a 1 to the current segment
 *    0   append a 0 to the current segment
 *    *   send the main segment TWICE, and then send the ext segment if started
 *
 * a segment is fixed header of
 *   (mark, space, mark, space, mark) = (0, 5000, 4400, 4300, 500)
 *   followed by 48 bits of (space, mark) pairs,
 *   with 1=(1500, 500) and 0=(500, 500)
 *
 * an IR command is one main segment, repeated twice,
 *   followed by an optional extension segment
 *
 * the main segment is:
 *   ID1 ID2 ~ID1 ~ID2 FAN STATE ~FAN ~STATE TEMP MODE ~TEMP ~MODE
 *      where ID1    = 1011
 *            ID2    = 0010
 *            ~<ANY> = the bitwise negation of the nibble, e.g.: 1101 => 0010
 *            FAN    = 1011 | 1001 | 0101 | 0011 | 0111
 *                     (auto, low, med, high, off)
 *            TEMP   = 0000 | 0001 | 0011 | 0010 | 0110 | 0111 | 0101 | 0100 |
 *                     1100 | 1101 | 1001 | 1000 | 1010 | 1011 | 1111 | 1110
 *                     (17-30, same, off)
 *            STATE  = 1111 | 1011
 *                     (on, off)
 *                     NOTE: ignored on some A/Cs
 *            MODE   = 1000 | 0000 | 1100 | 0100 | 0000
 *                     (auto, cool, heat, fan, off)
 *                     NOTE: on some A/Cs, mode must be set to 0000/cool to turn off
 *   e.g.:
 *     mode=cool, fan=high, temp=23C  (state is always off)
 *        1011  0010  0100  1101  0011  1011  1100  0100  0101  0000  1010  1111
 *        ID1   ID2  ~1011 ~0010  high  off  ~0011 ~1011  23C   cool ~0101 ~0000
 *
 * the ext segment is:
 *   EXT_ID_BYTE FAN_PCT_BYTE SLEEP_PLUSTEMP_BYTE LOWTEMP_UNITF_BYTE EMPTY_BYTE CSUM_BYTE
 *      where EXT_ID_BYTE         = 11010101
 *            FAN_PCT_BYTE        = decimal int between 0-100 (inclusive), or 102 for auto,
 *                                  as an 8bit bitstring
 *            SLEEP_PLUSTEMP_BYTE = S0P00000
 *            S                   = the SLEEP_BIT, 1 for sleep=on, 0 for sleep=off
 *            P                   = the PLUSTEMP_BIT, 1 for higher fahrenheit temp
 *            LOWTEMP_UNITF_BYTE  = 000L000F
 *            L                   = the LOWTEMP_BIT, 1 for low temp (16C/60F), 0 for normal
 *            F                   = the UNITF_BIT, 1 for fahrenheit, 0 for celsius
 *            EMPTY_BYTE          = 00000000
 *            CSUM_BYTE           = sum of the 5 other bytes, modulus 256,
 *                                  as an 8bit bitstring
 *   e.g.:
 *     fan=88%, temp=61F (17C, lowtemp, plustemp), sleep=yes
 *        1101  0101  0101  1000  1010  0000  0001  0001  0000  0000  1101  1110
 *        EXT_ID      fan=88%     sleep+plus  low+unitF   emptyByte    checksum
 *        (213     +  88       +  160      +  17      +   0)  % 256 = 222
 */

IRsendRaw sender;

uint16_t mideaSegments[2][101];
unsigned int mideaSegmentLen = 101;

unsigned int curSeg = -1;
unsigned int curIdx = -1;

void setup() {
  Serial.begin(9600);
  Serial.println("start");
}

void loop() {
  while(Serial.available() > 0){
    char c = Serial.read();
    if(c == '#'){
      curSeg = 0;
      curIdx = 0;
      mideaSegments[curSeg][curIdx++] = 0;    //mark
      mideaSegments[curSeg][curIdx++] = 5000; //space
      mideaSegments[curSeg][curIdx++] = 4400; //mark
      mideaSegments[curSeg][curIdx++] = 4300; //space
      mideaSegments[curSeg][curIdx++] = 500;  //mark
    }else if(c == '@'){
      curSeg = 1;
      curIdx = 0;
      mideaSegments[curSeg][curIdx++] = 0;    //mark
      mideaSegments[curSeg][curIdx++] = 5000; //space
      mideaSegments[curSeg][curIdx++] = 4400; //mark
      mideaSegments[curSeg][curIdx++] = 4300; //space
      mideaSegments[curSeg][curIdx++] = 500;  //mark
    }else if(c == '1'){
      mideaSegments[curSeg][curIdx++] = 1500; //space
      mideaSegments[curSeg][curIdx++] = 500;  //mark
    }else if(c == '0'){
      mideaSegments[curSeg][curIdx++] = 500;  //space
      mideaSegments[curSeg][curIdx++] = 500;  //mark
    }else if(c == '*'){
      sender.send(mideaSegments[0], mideaSegmentLen, 36);
      sender.send(mideaSegments[0], mideaSegmentLen, 36);
      if(curSeg == 1){
        sender.send(mideaSegments[1], mideaSegmentLen, 36);
      }
      Serial.println("sent");
    }
  }
}
