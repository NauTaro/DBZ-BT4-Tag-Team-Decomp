"""Competitive item policy is carried in authoritative rules, then reset for private rooms."""
import unittest
import test_r12_tag_crashes as probes
from test_tag_effect_lifecycle import function

class ItemPolicyTests(unittest.TestCase):
    def test_production_policy_transitions_preserve_other_rules(self):
        code=r'''
#include <assert.h>
#include <stdint.h>
#define NET_RULE_RESTRICT_ITEMS (1u<<30)
static uint32_t sRules;static int session;
int Port_NetSession(void){return session;}
'''+function('port/src/gs/net.c','rules_items_restrict')+'\n'+function('port/src/gs/net.c','Port_NetItemsRestricted')+r'''
int main(void){
 for(uint32_t base=0x72114A3; base<0x7211500; ++base){
  sRules=base;session=0;rules_items_restrict(1);assert(!Port_NetItemsRestricted());
  session=1;assert(Port_NetItemsRestricted());assert((sRules&~NET_RULE_RESTRICT_ITEMS)==base);
  rules_items_restrict(0);assert(!Port_NetItemsRestricted());assert(sRules==base);
  rules_items_restrict(1);assert(Port_NetItemsRestricted());
  rules_items_restrict(0);assert(!Port_NetItemsRestricted());assert(sRules==base);
 }
}
'''
        probes.ReportedTagCrashes.native(self,code)

if __name__=='__main__':unittest.main()
