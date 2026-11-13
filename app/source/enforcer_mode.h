/* Shared, host-testable preference decoding and correction policy. */
#ifndef ENFORCER_MODE_H
#define ENFORCER_MODE_H
static inline unsigned enforcer_target(const char *data,unsigned size)
{return size==2&&data[1]=='\n'&&data[0]=='5'?5u:4u;}
static inline int enforcer_should_correct(unsigned target,unsigned resolution,unsigned scan)
{return (target==4||target==5)&&(resolution==4||resolution==5)&&resolution!=target&&scan==0;}
#endif
