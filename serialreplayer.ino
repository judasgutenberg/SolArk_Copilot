#include <Arduino.h>

const char serialDump[] PROGMEM =
"I (130690112) MB: 0x3ffbb5fc   e2 04 59 14 11 00 63 00  25 00 90 00 00 00 00 00  |..Y...c.%.......|\n"
"I (130690122) MB: 0x3ffbb60c   27 00 4b 00 70 17 70 17  00 00 10 01 00 00 00 00  |'.K.p.p.........|\n"
"I (130690132) MB: 0x3ffbb61c   00 00 00 00                                       |....|\n"
"I (130690242) MB: <1> Characteristic #3 10s_2 (2) value = (0x00000000) read successful. : 16\n"
"I (130690242) MB: 0x3ffbb5bc   00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|\n"
"I (130695262) HTTP_POST: HTTP POST successful\n"
"I (130695262) HTTP_POST: HTTP_EVENT_DISCONNECTED\n"
"I (130699712) MB: <1> Characteristic #5 30s_0 (0) value = (0x00010003) read successful. : 38\n"
"I (130699712) MB: 0x3ffbb5bc   03 00 01 00 01 02 31 32  36 30 37 31 32 34 39 31  |......1260712491|\n"
"I (130699722) MB: 0x3ffbb5cc   00 00 00 00 00 00 17 07  00 00 11 52 30 14 00 00  |...........R0...|\n"
"I (130699732) MB: 0x3ffbb5dc   80 38 01 00 03 02                                 |.8....|\n"
"I (130699832) MB: <1> Characteristic #6 30s_1 (1) value = (0x00010000) read successful. : 2\n"
"I (130699832) MB: 0x3ffbb5bc   00 00                                             |..|\n"
"I (130699922) MB: <1> Characteristic #7 30s_2 (2) value = (0x000a00ff) read successful. : 10\n"
"I (130699932) MB: 0x3ffbb5bc   ff 00 0a 00 0b 19 0a 13  1b 2d                    |.........-|\n"
"I (130700092) MB: <1> Characteristic #8 30s_3 (3) value = (0x1f400001) read successful. : 68\n"
"I (130700092) MB: 0x3ffbb5bc   01 00 40 1f 00 00 3a 07  ba 13 c0 12 00 00 10 27  |..@...:........'|\n"
"I (130700092) MB: 0x3ffbb5cc   b8 0b 40 1f d0 07 52 03  00 00 00 00 e8 03 00 00  |..@...R.........|\n"
"I (130700102) MB: 0x3ffbb5dc   00 00 00 00 01 00 00 00  01 00 00 00 00 00 00 00  |................|\n"
"I (130700112) MB: 0x3ffbb5ec   00 00 00 00 00 00 00 00  e0 2e 00 00 00 00 00 00  |................|\n"
"I (130700122) MB: 0x3ffbb5fc   00 00 00 00                                       |....|\n"
"I (130700422) MB: <1> Characteristic #9 30s_4 (4) value = (0x15e00000) read successful. : 200\n"
"I (130700432) MB: 0x3ffbb5bc   00 00 e0 15 e0 15 e0 15  68 01 c0 12 14 00 1e 00  |........h.......|\n"
"I (130700432) MB: 0x3ffbb5cc   00 00 00 00 64 00 64 00  00 00 00 00 01 00 05 00  |....d.d.........|\n"
"I (130700442) MB: 0x3ffbb5dc   de 03 14 00 15 00 23 00  ec 13 50 14 00 14 02 28  |......#...P....(|\n"
"I (130700452) MB: 0x3ffbb5ec   00 00 24 13 32 00 28 00  14 14 15 00 64 00 00 00  |..$.2.(.....d...|\n"
"I (130700462) MB: 0x3ffbb5fc   01 00 00 00 00 00 00 00  ec 13 50 00 18 15 5a 00  |..........P...Z.|\n"
"I (130700472) MB: 0x3ffbb60c   00 00 f4 01 03 00 00 00  01 00 00  10s_0 (0) value = (0xfff70002) read successful. : 132\n"
"I (130769822) MB: 0x3ffbb5bc   02 00 f7 ff 00 00 00 00  49 d3 00 00 00 00 00 00  |........I.......|\n"
"I (130769832) MB: 0x3ffbb5cc   00 00 00 00 00 00 0e 00  10 00 1a b5 00 00 cc ac  |................|\n"
"I (130769832) MB: 0x3ffbb5dc   00 00 1f 00 00 00 e0 16  00 00 00 00 00 00 00 00  |................|\n"
"I (130769842) MB: 0x3ffbb5ec   00 00 16 00 6a ea 00 00  00 00 00 00 00 00 ba 05  |....j...........|\n"
"I (130769852) MB: 0x3ffbb5fc   30 05 00 00 00 00 d0 07  00 00 53 45 01 00 00 00  |0.........SE....|\n"
"I (130769862) MB: 0x3ffbb60c   00 00 01 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|\n"
"I (130769872) MB: 0x3ffbb61c   00 01 01 00 b7 0b 01 00  f5 09 05 00 00 00 00 00  |................|\n"
"I (130769882) MB: 0x3ffbb62c   00 00 00 00 00 00 00 00  00 00 00 00 00 00 1e 0e  |................|\n"
"I (130769892) MB: 0x3ffbb63c   a0 1f 2d 00                                       |..-.|\n"
"I (130770102) MB: <1> Characteristic #2 10s_1 (1) value = (0x00100034) read successful. : 100\n"
"I (130770102) MB: 0x3ffbb5bc   34 00 10 00 23 00 75 09  b0 04 b0 04 60 09 b0 04  |4...#.u.....`...|\n"
"I (130770102) MB: 0x3ffbb5cc   b0 04 00 00 00 00 00 00  00 00 00 00 8c 00 50 00  |..............P.|\n"
"I (130770112) MB: 0x3ffbb5dc   00 00 00 00 00 00 00 00  00 00 00 00 00 00 63 00  |..............c.|\n"
"I (130770122) MB: 0x3ffbb5ec   00 00 63 00 63 00 00 00  63 00 63 00 00 00 36 00  |..c.c...c.c...6.|\n"
"I (130770132) MB: 0x3ffbb5fc   e2 04 59 14 11 00 63 00  26 00 86 00 00 00 00 00  |..Y...c.&.......|\n"
"I (130770142) MB: 0x3ffbb60c   2c 00 55 00 70 17 70 17  00 00 10 01 00 00 00 00  |,.U.p.p.........|\n"
"I (130770152) MB: 0x3ffbb61c   00 00 00 00                                       |....|\n";



#define BAUD_RATE 115200
#define LINE_DELAY_MS 20
#define LOOP_DELAY_MS 2000


void setup() {
  Serial.begin(115200);
  delay(1000); // let USB settle
}

void loop() {
  replayDumpLinewise();
}

void replayDump() {
  static uint16_t idx = 0;

  if (serialDump[idx] == '\0') {
    delay(LOOP_DELAY_MS);
    idx = 0;
    return;
  }

  Serial.write(pgm_read_byte(&serialDump[idx++]));
}

void replayDumpLinewise() {
  static uint16_t idx = 0;

  while (true) {
    char c = pgm_read_byte(&serialDump[idx++]);
    if (c == '\0') {
      delay(LOOP_DELAY_MS);
      idx = 0;
      return;
    }

    Serial.write(c);
    if (c == '\n') {
      delay(LINE_DELAY_MS);
      return;
    }
  }
}

 
