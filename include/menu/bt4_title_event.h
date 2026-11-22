#ifndef MENU_BT4_TITLE_EVENT_H
#define MENU_BT4_TITLE_EVENT_H

/* Host streams outside the disc's AFS partitions. */
enum {
    BT4_TITLE_OST = -1000,
    BT4_TITLE_GREETING = -1001,
    BT4_TITLE_CONFIRM = -1002,
    BT4_TITLE_ART = -1003,
};

#ifdef PORT
int Port_TitleEventPath(int asset, int japanese, char *path, int size);
#endif

#endif
