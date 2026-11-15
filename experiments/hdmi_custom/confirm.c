/* SPDX-License-Identifier: MIT. Independent from input APIs and frame rendering. */
#include "confirm.h"
int hc_confirm_sample(hc_confirm *s,unsigned buttons)
{
 if(!s->released){if(!buttons)s->released=1;return 0;}
 if(buttons&0x2000u)s->choice=2;
 else if((buttons&0x4000u)&&!s->choice)s->choice=1;
 if(!buttons&&s->choice){int result=s->choice;s->choice=0;return result;}
 return 0;
}
