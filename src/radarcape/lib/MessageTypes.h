#ifndef MESSAGETYPES_H_
#define MESSAGETYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

const unsigned int i_msg_ts              = 0;
const unsigned int i_msg_siglev          = 6;
const unsigned int i_msg_frame           = 6 + 1;
const unsigned int i_msg_len_ac          = 6 + 1 + 2;
const unsigned int i_msg_len_modes_short = 6 + 1 + 7;
const unsigned int i_msg_len_modes_long  = 6 + 1 + 14;
const unsigned int i_msg_maxlen          = 6 + 1 + 14;
const unsigned int i_msg_maxlen_escaped  = 2 + 2 * i_msg_maxlen;

enum MessageType_T
{
    MessageType_Unknown     = 0x00,
    MessageType_AVR         = 0x01,
    MessageType_AVRMLAT     = 0x02,
    MessageType_AVRStatus   = 0x03,
    MessageType_AVRMStatus  = 0x04,
    MessageType_ModeAC      = 0x31,
    MessageType_ModeSShort  = 0x32,
    MessageType_ModeSLong   = 0x33,
    MessageType_Status      = 0x34,
    MessageType_ExtLocation = 0x35,
    MessageType_MlatResult  = 0x36
};



#ifdef __cplusplus
}
#endif

#endif // MESSAGETYPES_H_
