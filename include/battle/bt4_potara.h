#ifndef BATTLE_BT4_POTARA_H
#define BATTLE_BT4_POTARA_H
#include "types.h"
/* B14 Rev2 common item table: 114..124 select the eleven aura palettes,
   133 preserves intact clothes and 134 starts with damaged clothes.
   These same items also grant initial Ki in the ISO. Quick Match and Ranked
   keep their palette bits without that mechanical bonus. Private matches
   retain the complete synchronized equipped set. */
static inline int Bt4Potara_VisualItem(unsigned id){return (id>=114&&id<=124)||id==133||id==134;}
static inline void Bt4Potara_OnlineAbilities(s32 *ability){
    ability[0]=ability[1]=0;
    ability[2]&=0x007FE000u; /* aura bits 0x4D..0x56 */
    ability[3]&=0x0C100000u; /* aura 0x74, clothing 0x7A and 0x7B */
}
#endif
