#ifndef PORT_RENDER_MARKERS_H
#define PORT_RENDER_MARKERS_H
/* Private metadata above the GS PRIM hardware bits. It travels in each sorted
   triangle packet, following actual boundary geometry rather than a texture. */
#define PORT_PRIM_BOUNDARY (1ULL << 32)
void Port_BoundaryBegin(void);
void Port_BoundaryEnd(void);
int Port_BoundaryDrawing(void);
#endif
