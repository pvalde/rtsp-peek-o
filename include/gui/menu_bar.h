#ifndef MENU_BAR_H
#define MENU_BAR_H
#include "nuklear.h"

void menu_quit(struct nk_context *nk_ctx, int *running);
void example_menu_execute(struct nk_context *nk_ctx);

#endif // MENU_BAR_H
