#ifndef PORT_BT4_GOKU_COSTUMES_H
#define PORT_BT4_GOKU_COSTUMES_H

/* A range outside the disc and the port's added stage and song files. */
#define BT4_GOKU_COSTUME_PART_INDEX 131072
#define BT4_GOKU_COSTUME_FILE_FIRST (0x20000 + BT4_GOKU_COSTUME_PART_INDEX)
#define BT4_GOKU_COSTUME_FILE_COUNT 56
#define BT4_GOKU_COSTUME_PATH "tbo/goku-costumes/"

int Port_GokuCostumesReady(void);

#endif
