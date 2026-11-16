#ifndef TAG_MENU_MOVIE_H
#define TAG_MENU_MOVIE_H
#include <stdint.h>
/* Renderer thread only. Returned pixels belong to the upload queue, not the
 * decoder. serial is unchanged when no new picture is ready. */
unsigned char *TagMenuMovie_Pixels(uint32_t *width, uint32_t *height, uint32_t *serial);
void TagMenuMovie_Maintain(void);
void TagMenuMovie_Stop(void);
#endif
