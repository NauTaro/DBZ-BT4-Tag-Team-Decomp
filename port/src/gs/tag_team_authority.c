/* Compile the production room kernel natively without a second implementation
 * of input deadlines, neutral controls, epoch checks or synchronization. */
#define TT_KERNEL_NATIVE 1
#include "../../../cloudflare/kernel.c"
