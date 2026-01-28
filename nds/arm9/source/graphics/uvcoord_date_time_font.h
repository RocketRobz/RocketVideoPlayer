/*======================================================================

DATE_TIME_FONT texture coordinates

======================================================================*/

#ifndef DATE_TIME_FONT__H
#define DATE_TIME_FONT__H
#define DATE_TIME_FONT_NUM_IMAGES  0xE

// U,V,Width,Height

#define TEXT_DTY 8

// The U coordinates are invalid
static constexpr u16 date_time_font_texcoords[] = {
    0, 0, 4, TEXT_DTY, // SPACE
    8, 0, 8, TEXT_DTY, // SOLIDUS
    16, 0, 8, TEXT_DTY, // DIGIT ZERO
    24, 0, 8, TEXT_DTY, // DIGIT ONE
    32, 0, 8, TEXT_DTY, // DIGIT TWO
    40, 0, 8, TEXT_DTY, // DIGIT THREE
    48, 0, 8, TEXT_DTY, // DIGIT FOUR
    56, 0, 8, TEXT_DTY, // DIGIT FIVE
    64, 0, 8, TEXT_DTY, // DIGIT SIX
    72, 0, 8, TEXT_DTY, // DIGIT SEVEN
    80, 0, 8, TEXT_DTY, // DIGIT EIGHT
    88, 0, 8, TEXT_DTY, // DIGIT NINE
    96, 0, 4, TEXT_DTY, // COLON
    104, 0, 4, TEXT_DTY, // SEMICOLON

};

static constexpr unsigned short int date_time_utf16_lookup_table[] = {
0x20,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,
};


#endif
