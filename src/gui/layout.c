#include "layout.h"

int set_pos(SDL_Rect *out_pos_rect, int index, Layout layout,
            int renderer_height, int renderer_width, int padding_top) {

    if (!out_pos_rect) {
        return -4; // NULL pointer
    }

    if ((layout.columns < 1) || (layout.rows < 1)) {
        return -2; // invalid layout values.
    }

    if (index < 0) {
        return -3; // invalid index value.
    }

    if ((index) >= (layout.rows * layout.columns)) {
        return -1; // no space to render
    }

    // 0 indicates first row/column

    // ROW POS = INDEX / N_OF_COLS
    int grid_position_row =
        index / layout.columns; // division by 0 is not possible.

    // COL POS = POS % N_OF_COLS
    int grid_position_column = index % layout.columns;

    float row_weight =
        layout.rows > 1 ? ((float)renderer_height - padding_top) / layout.rows
                        : (float)renderer_height - padding_top;
    float column_weight = layout.columns > 1
                              ? (float)renderer_width / layout.columns
                              : (float)renderer_width;

    out_pos_rect->x = (int)(round)(column_weight * (grid_position_column));
    out_pos_rect->y =
        (int)(round)((row_weight * (grid_position_row)) + padding_top);
    out_pos_rect->w = column_weight;
    out_pos_rect->h = row_weight;

    return 0;
}
