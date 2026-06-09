#ifndef LAYOUT_H
#define LAYOUT_H

#include "SDL_rect.h"

typedef struct {
    int rows;
    int columns;
} Layout;

/**
 * @brief Computes the position and size of a grid cell in a renderer.
 *
 * Maps a linear index (row-major order) into a 2D grid defined by the
 * given layout, and writes the resulting rectangle into @p out_pos_rect.
 *
 * The rendering area is divided evenly into `layout.rows` ×
 * `layout.columns` cells. Each cell has equal width and height. A vertical
 * offset (`padding_top`) is applied to all rows.
 *
 * Index mapping:
 * - Row    = index / layout.columns
 * - Column = index % layout.columns
 *
 * @param[out] out_pos_rect     Output SDL_Rect to store computed position/size.
 * @param[in]  index            Linear index in row-major order.
 * @param[in]  layout           Grid layout (must have rows > 0 and columns >
 * 0).
 * @param[in]  renderer_height  Total height of the rendering area.
 * @param[in]  renderer_width   Total width of the rendering area.
 * @param[in]  padding_top      Vertical offset applied before grid rendering.
 *
 * @return 0   Success.
 * @return -1  Index exceeds grid capacity (rows * columns).
 * @return -2  Invalid layout (rows < 1 or columns < 1).
 * @return -3  Negative index.
 * @return -4  NULL pointer passed for @p out_pos_rect.
 *
 * @note Uses floating-point division and rounding for positioning.
 * @note If rows or columns equals 1, the full dimension is used on that axis.
 */

int set_pos(SDL_Rect *out_pos_rect, int index, Layout layout,
            int renderer_height, int renderer_width, int padding_top);

#endif // LAYOUT_H
