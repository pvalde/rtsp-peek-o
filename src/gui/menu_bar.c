#include "menu_bar_private.h"
#include <stddef.h>

void menu_quit(struct nk_context *nk_ctx, int *running) {

    nk_layout_row_dynamic(nk_ctx, 25, 1);
    if (nk_menu_item_label(nk_ctx, "Exit", NK_TEXT_LEFT)) {
        *running = 0;
    }
}
