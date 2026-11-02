/* erdtree's T1: the Erdtree over the ruins of Leyndell (erd_page.c). */
#ifndef ERD_PAGE_H
#define ERD_PAGE_H

#include "opus_plugin.h"

/* From opl_init: the still art written to the plugin's folder and the memory its kept pixels need. Without either
 * the page still draws everything that moves. */
void erd_page_init(struct opl_self *self);
/* The engine's T1 page (the display thread, every frame while it shows). */
void erd_page(void);

#endif
