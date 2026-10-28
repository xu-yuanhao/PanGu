/* PanGu vector welcome mark. All raster/animation math is native Assembly.
 * Coordinates use 16.16 SVG units; phase 0=closed, 65536=original V2.
 * Cells are [Braille mask, edge coverage], caller-owned, row-major.
 * Raster returns 0 for invalid dimensions/capacity, otherwise width*height.
 * Aspect is cell pixel height/width in 16.16, normally 131072 (2:1).
 */
#ifndef PANGU_TUI_LOGO_H
#define PANGU_TUI_LOGO_H
#include <stddef.h>
#include <stdint.h>
enum { TUI_LOGO_BRAILLE=1u, TUI_LOGO_TRUECOLOR=2u, TUI_LOGO_REDUCED_MOTION=4u };
int tui_logo_contains(int x, int y, unsigned phase);
size_t tui_logo_raster(unsigned char *cells, unsigned width, unsigned height,
                       unsigned aspect, unsigned phase, size_t capacity);
/* Platform/environment glue: aspect, capability and theme discovery only. */
void tui_logo_environment(unsigned *aspect, unsigned *flags, unsigned *fg, unsigned *bg);
#endif
