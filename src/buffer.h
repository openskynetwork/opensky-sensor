#ifndef _HAVE_BUFFER_H
#define _HAVE_BUFFER_H

#include <stdlib.h>
#include <message.h>
#include <component.h>

extern struct Component BUF_comp;

void BUF_flush();

void BUF_fillStatistics();

struct MSG_Message * BUF_newMessage();
void BUF_commitMessage(struct MSG_Message * msg);
void BUF_abortMessage(struct MSG_Message * msg);

const struct MSG_Message * BUF_getMessage();
const struct MSG_Message * BUF_getMessageTimeout(uint32_t timeout_ms);
void BUF_releaseMessage(const struct MSG_Message * msg);
void BUF_putMessage(const struct MSG_Message * msg);

#endif
