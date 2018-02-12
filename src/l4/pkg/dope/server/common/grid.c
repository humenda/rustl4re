/*
 * \brief   DOpE Grid widget module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * The Grid  layout  widget  enables the  placement
 * of  multiple  child-widgets on a  grid.  Row and
 * column  sizes  can  be  specified  absolutely or
 * weighted.   Each  widget  can  be   individually
 * positioned using padding, spanning and alignment
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */


struct grid;
#define WIDGET struct grid

#include "dopestd.h"
#include "widget_data.h"
#include "widget_help.h"
#include "background.h"
#include "gfx.h"
#include "widman.h"
#include "script.h"
#include "grid.h"
#include "redraw.h"
#include "list_macros.h"
#include "keycodes.h"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

static struct widman_services     *widman;
static struct gfx_services        *gfx;
static struct background_services *bg;
static struct script_services     *script;
static struct redraw_services     *redraw;

#define GRID_UPDATE_CELLMAP    0x08

struct section;
struct section {
	int   fixed;           /* fixed size or -1 (free)           */
	int   size;            /* row size in pixels                */
	int   offset;          /* position relative to grid parent  */
	int   index;           /* index of row/column               */
	int   min;             /* minimal size of section           */
	int   max;             /* maximal size of section           */
	WIDGET *minforce;      /* widget that enforced the minsize  */
	WIDGET *maxforce;      /* widget that enforced the maxsize  */
	float weight;          /* weight of row/column              */
	struct section *next;  /* next row/column in connected list */
};


struct cell;
struct cell {
	struct section *row;     /* first row used by the cell            */
	struct section *col;     /* first column used be the cell         */
	s16 row_span;            /* number of rows used by the cell       */
	s16 col_span;            /* number of colums used by the cell     */
	u16 sticky;              /* alignment of widget inside its cell   */
	s16 pad_x;               /* hor.distance of widget to cell border */
	s16 pad_y;               /* ver.distance of widget to cell border */
	WIDGET *wid;             /* associated widget                     */
	struct cell *next;       /* next cell in connected cell-list      */
};


struct grid_data {
	struct section *rows;           /* list of rows                  */
	struct section *cols;           /* list of columns               */
	int             num_rows;       /* number of rows                */
	int             num_cols;       /* number of columns             */
	struct cell    *cells;          /* list of cells                 */
	struct cell   **cellmap;        /* grid map with cell references */
	u32             cellmapsize;    /* current size of cellmap       */
	u32             update;         /* grid specific update flags    */
};


int init_grid(struct dope_services *d);
void print_grid_info(GRID *g);


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

void print_grid_info(GRID *g) {

	struct section *sec;
	struct cell *cc;
	WIDGET *cw;

	printf("Grid info:\n");
	if (!g) printf(" grid is zero!\n");
	
	printf(" xpos   = %d\n", (int)g->wd->x);
	printf(" ypos   = %d\n", (int)g->wd->y);
	printf(" width  = %d\n", (int)g->wd->w);
	printf(" height = %d\n", (int)g->wd->h);
	printf(" row-sections:\n");
	sec = g->gd->rows;
	while (sec) {
		printf("  index: %d\n", sec->index);
		printf("   offset:%d\n", sec->offset);
		printf("   size:  %d\n", sec->size);
		printf("   weight:%f\n",  sec->weight);
		sec=sec->next;
	}
	printf(" column-sections:\n");
	sec = g->gd->cols;
	while (sec) {
		printf("  index: %d\n", sec->index);
		printf("   offset:%d\n", sec->offset);
		printf("   size:  %d\n", sec->size);
		printf("   weight:%f\n",  sec->weight);
		sec = sec->next;
	}
	printf(" child-widgets:\n");
	cc = g->gd->cells;
	while (cc) {
		cw = cc->wid;
		if (cw) {
			printf("  position (");
			if (cc->row) printf("%u", cc->row->index);
			else printf("unknown");
			printf(",");
			if (cc->col) printf("%u", cc->col->index);
			else printf("unknown");
			printf(")\n");
			printf("   xpos   = %lu\n", cw->gen->get_x(cw));
			printf("   ypos   = %lu\n", cw->gen->get_y(cw));
			printf("   width  = %lu\n", cw->gen->get_w(cw));
			printf("   height = %lu\n", cw->gen->get_h(cw));
		}
		cc = cc->next;
		printf("aaa\n");
	}
	printf("ok.\n");
}


/*** RETURN THE POSITION OF AN ELEMENT IN A SECTION LIST ***/
static inline int get_section_num(struct section *curr, struct section *s) {
	int i;
	for (i=0; curr; i++) {
		if (curr == s) return i;
		curr = curr->next;
	}
	return 0;
}


/*** SET ELEMENT OF THE CELLMAP ***/
static void cellmap_set(GRID *g, int x, int y, struct cell *value) {

	/* check agains grid boundaries */
	if (x >= g->gd->num_cols) return;
	if (y >= g->gd->num_rows) return;

	g->gd->cellmap[y*g->gd->num_cols + x] = value;
}


/*** INSERT REFERENCES TO CELLS INTO CELLMAP ***/
static inline void update_cellmap(GRID *g) {
	struct cell *curr = g->gd->cells;
	int i, j;

	memset(g->gd->cellmap, 0, g->gd->num_rows * g->gd->num_cols * sizeof(struct cell *));
	while (curr) {
		for (j=0; j<curr->row_span; j++) for (i=0; i<curr->col_span; i++) {
			cellmap_set(g, get_section_num(g->gd->cols, curr->col) + i,
		                   get_section_num(g->gd->rows, curr->row) + j, curr);
		}
		curr = curr->next;
	}
}


/*** REALLOCATE CELLMAP ***
 *
 * This function must be called if the number of cells or
 * rows changes.
 */
static void realloc_cellmap(GRID *g) {
	u32 map_size = g->gd->num_rows * g->gd->num_cols;

	/* check if there is enough space in current cellmap */
	if (map_size > g->gd->cellmapsize) {
	
		/* allocate new cellmap in a greedy way to prevent instand reallocations */
		if (g->gd->cellmap) free(g->gd->cellmap);
		g->gd->cellmapsize = 2 * map_size;
		g->gd->cellmap = (struct cell **)zalloc(g->gd->cellmapsize * sizeof(struct cell *));
	}
	update_cellmap(g);
}


/*** CREATE NEW ROW/COLUMN SECTION WITH DEFAULT VALUES ***/
static struct section *new_section(u32 index) {
	struct section *new = zalloc(sizeof(struct section));

	INFO(printf("Grid(new_section): index = %lu\n", index);)
	if (!new) {
		INFO(printf("Grid(new_section): out of memory!\n");)
		return NULL;
	}
	/* set default properties */
	new->fixed  = -1;
	new->size   = 64.0;
	new->offset = 0;
	new->index  = index;
	new->weight = 1.0;
	new->next   = NULL;
	return new;
}


/*** RETURN SECTION STRUCTURE BY ITS INDEX ***
 *
 * \return  section struct or NULL if there is no section with such index
 */
static struct section *get_section(struct section *curr, s32 idx) {
	while (curr) {
		if (curr->index == idx) return curr;
		curr = curr->next;
	}
	return NULL;
}


/*** CREATES A NEW SECTION AND INSERTS IT INTO SECTION LIST ***
 *
 * \param head   head of destination section list
 * \return       newly created section or NULL if section could not be created
 */
static struct section *insert_section(struct section **head, s32 idx) {
	struct section *new = new_section(idx);
	struct section *curr;

	if (!new) return NULL;

	/* if section list has no elements yet, set first element */
	if (*head == NULL) return *head = new;

	/* if index of first element is higher than new index */
	if ((*head)->index > idx) {
		new->next = *head;
		return *head = new;
	}

	/* search for the needed index */
	curr = *head;
	while (curr->next) {
		if (curr->next->index > idx) break;
		curr = curr->next;
	}

	/* insert new element */
	new->next = curr->next;
	return curr->next = new;
}


/*** RETURNS ROW SECTION STRUCTURE BY INDEX ***
 *
 * A new row section is created automaticaly if needed.
 */
static struct section *get_row(GRID *g, s32 row_idx) {
	struct section *ret = get_section(g->gd->rows, row_idx);
	if (!ret) {
		ret = insert_section(&g->gd->rows, row_idx);
		if (ret) {
			g->gd->num_rows++;
			realloc_cellmap(g);
		}
	}
	return ret;
}


/*** RETURNS COLUMN SECTION STRUCTURE BY INDEX ***
 *
 * A new column section is created automaticaly if needed.
 */
static struct section *get_column(GRID *g, s32 col_idx) {
	struct section *ret = get_section(g->gd->cols, col_idx);
	if (!ret) {
		ret = insert_section(&g->gd->cols, col_idx);
		if (ret) {
			g->gd->num_cols++;
			realloc_cellmap(g);
		}
	}
	return ret;
}


/*** RETURNS THE GRID-CELL THAT IS ASSOCIATED WITH A GIVEN WIDGET ***/
static struct cell *get_cell(GRID *g, WIDGET *w) {
	struct cell *curr;
	if (!g || !w) return NULL;
	if (w->gen->get_parent(w) != g) return NULL;
	curr = g->gd->cells;
	while (curr) {
		if (curr->wid == w) return curr;
		curr = curr->next;
	}
	return NULL;
}


/*** CALCULATE THE SIZES OF ROWS/COLUMNS-SECTIONS ***
 *
 * \param seclist     begin of section list
 * \param sum_size    desired overall size
 * \param num_secs    max.number of sections to modify
 */
static void  calc_section_sizes(struct section *seclist,
                                s32 sum_size, u32 num_secs) {
	struct section *curr;
	float sum_weights  = 0.0;
	s32   sum_weighted = sum_size;
	s32   i;

	/* sum weights of weighted sections */
	for (curr=seclist, i=num_secs; (i--) && (curr); ) {
		if (curr->fixed < 0) {
			curr->size = -1;
			sum_weights += curr->weight;
		}
		curr = curr->next;
	}
	
	/* define sections with fixed sizes and sum fixed sizes */
	for (curr=seclist, i=num_secs; (i--) && (curr); ) {
		if (curr->fixed >= 0) {
			curr->size = curr->fixed;
			if (curr->size < curr->min) curr->size = curr->min;
			if (curr->size > curr->max) curr->size = curr->max;
			sum_weighted -= curr->size;
		}
		curr = curr->next;
	}
	
	/* enforce max constrains to weighted sections */
	for (curr=seclist, i=num_secs; (i--) && (curr); ) {
		if (curr->size == -1) {
			int size = (sum_weighted*curr->weight)/sum_weights;
			if (size > curr->max) {
				curr->size = curr->max;
				sum_weighted -= curr->size;
				sum_weights  -= curr->weight;

				/* weights changed - so lets restart from the beginning of the list */
				curr = seclist;
				i = num_secs;
				continue;
			}
		}
		curr = curr->next;
	}
	
	/* apply min constrains to weighted sections */
	for (curr=seclist, i=num_secs; (i--) && (curr); ) {
		if (curr->size == -1) {
			int size = (sum_weighted*curr->weight)/sum_weights;
			if (size < curr->min) {
				curr->size = curr->min;
				sum_weighted -= curr->size;
				sum_weights  -= curr->weight;

				/* weights changed - so lets restart from the beginning of the list */
				curr = seclist;
				i = num_secs;
				continue;
			}
		}
		curr = curr->next;
	}
	
	/* balance remaining weighted sections */
	for (curr=seclist, i=num_secs; (i--) && (curr); ) {
		if (curr->size == -1) {

			/*
			 * We decrement the remaining size (in pixels) and
			 * weight by the currently consumed values to avoid
			 * accumulating rounding errors.
			 */
			curr->size = (sum_weighted*curr->weight)/sum_weights;
			sum_weighted -= curr->size;
			sum_weights  -= curr->weight;
		}
		curr = curr->next;
	}
}


/*** CALCULATE OFFSETS OF SECTIONS RELATIVE TO THE FIRST SECTION ***/
static void calc_section_offsets(struct section *curr) {
	s32 curr_offset = 0;
	if (!curr) return;
	while (curr) {
		curr->offset = curr_offset;
		curr_offset = curr_offset + curr->size;
		curr = curr->next;
	}
}


/*** RETURN SIZE OF THE SPECIFIED NUMBER OF NEIGHBOUR SECTIONS ***/
static s32 get_section_size(struct section *curr, u32 num_sections) {
	if (!curr || !num_sections) return 0;
	if (num_sections > 1)
		return curr->size + get_section_size(get_section(curr, curr->index+1), num_sections-1);
	else
		return curr->size;
}


/*** CALCULATE SUM OF SECTION MIN SIZES ***/
static s32 get_sections_minsum(struct section *s) {
	s32 min = 0;
	while (s) { min += s->min; s = s->next; }
	return min;
}


/*** CALCULATE SUM OF SECTION MAX SIZES ***/
static s32 get_sections_maxsum(struct section *s) {
	s32 max = 0;
	while (s) { max += s->max; s = s->next; }
	return max;
}


/*** SET POSITION AND SIZE OF A WIDGET INSIDE THE GIVEN CELL AREA ***
 *
 * \param w        widget to configure
 * \param cx, cy   cell position
 * \param cw, ch   cell size
 * \param sticky   sticky bits that define the alignment of the
 *                 widget inside the cell.
 */
static void position_widget(WIDGET *w, int cx, int cy, int cw, int ch, int sticky) {
	int wid_x, wid_y, wid_w, wid_h;

	/* set default size */
	wid_w = w->wd->min_w;
	wid_h = w->wd->min_h;

	/* check if widget can be stretched to cell size */
	if ((sticky & GRID_STICKY_EAST) && (sticky & GRID_STICKY_WEST)) {
		wid_w = (w->wd->max_w < cw) ? w->wd->max_w : cw;
		sticky &= ~(GRID_STICKY_EAST | GRID_STICKY_WEST);
	}

	if ((sticky & GRID_STICKY_NORTH) && (sticky & GRID_STICKY_SOUTH)) {
		wid_h = (w->wd->max_h < ch) ? w->wd->max_h : ch;
		sticky &= ~(GRID_STICKY_NORTH | GRID_STICKY_SOUTH);
	}
	
	/* default widget position is centered */
	wid_x = cx + ((cw - wid_w)>>1);
	wid_y = cy + ((ch - wid_h)>>1);

	/* align widget inside cell area */
	if (sticky & GRID_STICKY_WEST)  wid_x = cx;
	if (sticky & GRID_STICKY_NORTH) wid_y = cy;
	if (sticky & GRID_STICKY_EAST)  wid_x = cx + cw - wid_w;
	if (sticky & GRID_STICKY_SOUTH) wid_y = cy + ch - wid_h;
	
	/* update widget position attributes */
	w->gen->set_x(w, wid_x);
	w->gen->set_y(w, wid_y);
	if (wid_w >= 0) w->gen->set_w(w, wid_w);
	if (wid_h >= 0) w->gen->set_h(w, wid_h);
	w->gen->updatepos(w);
}


/*** SET POSITIONS OF GRID WIDGETS TO ITS CELLS POSITIONS ***/
static void place_widgets(GRID *g) {
	struct cell *cc;        /* current cell */
	WIDGET *cw;             /* current widget */
	s32 cell_x, cell_y, cell_w, cell_h;

	cc = g->gd->cells;
	while (cc) {
		if (cc->col && cc->row) {
			cell_x = cc->col->offset + cc->pad_x;
			cell_y = cc->row->offset + cc->pad_y;
			cell_w = get_section_size(cc->col, cc->col_span) - (float)(2*cc->pad_x);
			cell_h = get_section_size(cc->row, cc->row_span) - (float)(2*cc->pad_y);
			cw = cc->wid;
			if (cw) position_widget(cw, cell_x, cell_y, cell_w, cell_h, cc->sticky);
		}
		cc = cc->next;
	}
}

/*** CALCULATE RANGE OF SECTIONS THAT ARE VISIBLE AT A SPECIFIED PIXEL RANGE ***
 *
 * \param cs    list of sections
 * \param min   start of visible pixel range
 * \param max   end of visible pixel range
 * \param beg   result: index of first visible section
 * \param end   result: index of last visible section
 * \returns     1 if there are visible sections,
 *              0 if range does not contain any visible sections
 */
static inline int calc_visible_sections(struct section *cs, int min, int max,
                                        int *beg, int *end) {
	int i = 0;

	/* skip invisible sections at the beginning of section list */
	for (; cs && (cs->offset + cs->size < min); cs = cs->next, i++);
	if (!cs) return 0;
	*beg = i;

	/* search last visible section */
	for (; cs && (cs->offset < max); cs = cs->next, i++);

	/* i is the index after the last visible section */
	*end = i - 1;
	if (*beg > *end) return 0;

	return 1;
}


/*** UTILITY: REMOVE WIDGETS THAT ARE IN THE AREA AS THE SPECIFIED CELL ***
 *
 * \param new  This widget is the only one that should be left untouched
 */
static void erase_overlapping_cells(GRID *g, struct cell *new) {
	struct cell *cc = g->gd->cells;
	if (!new || !new->row || !new->col) return;

	/* remove widgets that occupy the same grid position */
	while (cc) {
		struct cell *nc = cc->next;

		if ((cc->row) && (cc->row->index == new->row->index)
		 && (cc->col) && (cc->col->index == new->col->index)
		 && (cc->wid) && (cc->wid != new->wid)) {
			g->grid->remove(g, cc->wid);
			g->gd->update |= GRID_UPDATE_CELLMAP;
		}
		cc = nc;
	}
}


/******************************
 *** GENERAL WIDGET METHODS ***
 ******************************/


static int grid_draw(GRID *g, struct gfx_ds *ds, long x, long y, WIDGET *origin) {
	s32  cx1 = gfx->get_clip_x(ds);
	s32  cy1 = gfx->get_clip_y(ds);
	s32  cx2 = gfx->get_clip_w(ds) + cx1;
	s32  cy2 = gfx->get_clip_h(ds) + cy1;
	int  row_beg, row_end, col_beg, col_end; /* grid range to process           */
	int  x1, y1 = 0, x2, y2 = 0;             /* current cell area               */
	int  cx=0, cy;                           /* last processed row/column index */
	struct cell *cc;                         /* current cell                    */
	int  i, j;
	int  ret = 0;

	if (origin == g) return 1;
	if ((cx1 > cx2) || (cy1 > cy2)) return 0;

	x += g->wd->x;
	y += g->wd->y;

	/* determine visible cells of the grid */
	if (!calc_visible_sections(g->gd->cols, cx1-x, cx2-x, &col_beg, &col_end)
	 || !calc_visible_sections(g->gd->rows, cy1-y, cy2-y, &row_beg, &row_end)) {

		/* if there are no visible sections at all we just draw the background */
		ret |= g->gen->draw_bg(g, ds, cx1, cy1, cx2 - cx1 + 1, cy2 - cy1 + 1, origin, 0);
		return ret;
	}

	/*
	 * The drawing method skips empty grid cells until a widget is hit.
	 * It then fills the skipped gap with the background pattern, draws
	 * the widget and its surrounding within its cell, and then continues
	 * with the next grid position line by line. The variables cx and cy
	 * mark the last processed grid index respectively the beginning of
	 * the current gap.
	 */
	for (j = row_beg, cy = cy1; j <= row_end; j++) {

		for (i = col_beg, cx = cx1; i <= col_end; i++) {

			/* fetch current cell from cell map, skip empty positions */
			cc = g->gd->cellmap[g->gd->num_cols*j + i];
			if (!cc || !cc->wid || !cc->col || !cc->row) continue;

			/* determine cell area */
			x1 = cc->col->offset + x;
			y1 = cc->row->offset + y;
			x2 = cc->col->size + x1 - 1;
			y2 = cc->row->size + y1 - 1;

			/*
			 * For the first cell of current row make sure that the space
			 * above the current row is filled with background.
			 */
			if ((cx == cx1) && (cy != y1)) {
				ret |= g->gen->draw_bg(g, ds, cx1, cy, cx2 - cx1 + 1, y1 - cy + 1, origin, 0);
				cy = y1;
			}

			/*
			 * If widget handles its background by itself we still
			 * need to fill the gaps between the widget and its
			 * cell borders with the background pattern.
			 */
			if (cc->wid->wd->flags & WID_FLAGS_CONCEALING) {

				/* determine gaps between widget and its cell border */
				int t = cc->wid->wd->y - cc->row->offset;
				int l = cc->wid->wd->x - cc->col->offset;
				int r = cc->col->offset + cc->col->size - cc->wid->wd->x - cc->wid->wd->w;
				int b = cc->row->offset + cc->row->size - cc->wid->wd->y - cc->wid->wd->h;

				/* limit gaps to positive values */
				if (t < 0) t = 0;
				if (l < 0) l = 0;
				if (b < 0) b = 0;
				if (r < 0) r = 0;

				/* fill background from last cursor position to left widget border */
				ret |= g->gen->draw_bg(g, ds, cx, y1, x1 + l - cx, y2 - y1 + 1, origin, 0);

				/* fill gaps at top and bottom with background */
				if (t) ret |= g->gen->draw_bg(g, ds, x1 + l, y1, x2 - x1 + 1 - l - r, t, origin, 0);
				if (b) ret |= g->gen->draw_bg(g, ds, x1 + l, y2 - b + 1, x2 - x1 + 1 - l - r, b, origin, 0);

				/* set cursor to right border of widget */
				cx = x2 - r + 1;

			/* draw background for non-concealing widgets */
			} else {
				ret |= g->gen->draw_bg(g, ds, cx, y1, x2 - cx + 1, y2 - y1 + 1, origin, 0);
				cx = x2 + 1;
			}

			gfx->push_clipping(ds, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
			ret |= cc->wid->gen->draw(cc->wid, ds, x, y, origin);
			gfx->pop_clipping(ds);
		}

		/* draw background at the right of the last cell of current row */
		if (cx != cx1) {
			ret |= g->gen->draw_bg(g, ds, cx, y1, cx2 - cx, y2 - y1 + 1, origin, 0);
			cy = y2 + 1;
		}
	}

	/* draw background at the bottom of the last row */
	ret |= g->gen->draw_bg(g, ds, cx1, cy, cx2 - cx1 + 1, cy2 - cy, origin, 0);

	return ret;
}


static WIDGET *grid_find(GRID *g, long x, long y) {
	struct cell *cc;
	WIDGET *cw;
	WIDGET *result;

	x -= g->wd->x;
	y -= g->wd->y;

	/* check if position is inside the window */
	if ((x >= 0) && (y >= 0) && (x < g->wd->w) && (y < g->wd->h)) {

		/* go through all cells and check their widgets */
		cc = g->gd->cells;
		while (cc) {
			cw = cc->wid;
			if (cw) {
				result = cw->gen->find(cw, x, y);
				if (result) return result;
			}
			cc = cc->next;
		}
		return g;
	}
	return NULL;
}


/*** UPDATE CELLMAP WHEN PLACEMENT CHANGED ***/
static void (*orig_update) (GRID *);
static void grid_update(GRID *g) {
	if (g->gd->update & GRID_UPDATE_CELLMAP) {
		g->wd->update |= WID_UPDATE_MINMAX;
		update_cellmap(g);
	}
	orig_update(g);
	g->gd->update = 0;
}


/*** UPDATE POSITION AND SIZE ***
 *
 * When size changes, we must reposition the child widgets.
 */
static void (*orig_updatepos) (GRID *g);
static void grid_updatepos(GRID *g) {

	/* calculate sizes of rows/columns */
	calc_section_sizes(g->gd->cols, g->wd->w, 99999);
	calc_section_sizes(g->gd->rows, g->wd->h, 99999);

	/* calculate offsets of rows/columns */
	calc_section_offsets(g->gd->cols);
	calc_section_offsets(g->gd->rows);
	
	/* set child widget positions */
	place_widgets(g);
	orig_updatepos(g);
}


/*** DETERMINE MIN/MAX SIZE OF GRID WIDGET ***
 *
 * The min/max properties of a Grid depend on the min/max properties of
 * its child widgets.
 *
 * FIXME: The span placement does not work, yet.
 */
static void grid_calc_minmax(GRID *g) {
	struct cell *c;
	struct section *s;
	WIDGET *cw;

	/* if grid has no content let it have any size */
	if (!g->gd->rows || !g->gd->cols) {
		g->wd->min_w = g->wd->min_h = 0;
		return;
	}

	/* assign initial min/max values to all rows and columns */
	s = g->gd->rows;
	while (s) {
		if (s->fixed < 0) {
			s->min = 0; s->max = 999999;
		} else {
			s->min = s->max = s->fixed;
		}
		s = s->next;
	}
	
	s = g->gd->cols;
	while (s) {
		if (s->fixed < 0) {
			s->min = 0; s->max = 999999;
		} else {
			s->min = s->max = s->fixed;
		}
		s = s->next;
	}
	
	/*
	 * Calculate min/max values for columns/rows by finding the highest min
	 * and lowest max value of the widgets inside a column/row.
	 */

	c = g->gd->cells;
	while (c) {
		int cw_min, cw_max;
		cw = c->wid;
//		if ((c->row) && (c->row->fixed < 0)) {
		if (c->row) {
			cw_min = cw->gen->get_min_h(cw) + 2*c->pad_y;
			cw_max = cw->gen->get_max_h(cw) + 2*c->pad_y;
			if (cw_min >= c->row->min) {
				c->row->min = cw_min;
				c->row->minforce = cw;
			}
			if ((cw_max <= c->row->max)
			 && (c->sticky & GRID_STICKY_NORTH)
			 && (c->sticky & GRID_STICKY_SOUTH)) {
				c->row->max = cw_max;
				c->row->maxforce = cw;
			}
		}
//		if ((c->col) && (c->col->fixed < 0)) {
		if (c->col) {
			cw_min = cw->gen->get_min_w(cw) + 2*c->pad_x;
			cw_max = cw->gen->get_max_w(cw) + 2*c->pad_x;
			if (cw_min >= c->col->min) {
				c->col->min = cw_min;
				c->col->minforce = cw;
			}
		if ((cw_max <= c->col->max)
			 && (c->sticky & GRID_STICKY_EAST)
			 && (c->sticky & GRID_STICKY_WEST)) {
				c->col->max = cw_max;
				c->col->maxforce = cw;
			}
		}
		c = c->next;
	}

	/* check if min > max - in this case min is stronger than max */
	s = g->gd->rows;
	while (s) {
		if (s->min < s->fixed) s->min = s->fixed;
		if (s->min > s->max)   s->max = s->min;
		s = s->next;
	}
	s = g->gd->cols;
	while (s) {
		if (s->min < s->fixed) s->min = s->fixed;
		if (s->min > s->max)   s->max = s->min;
		s = s->next;
	}

	g->wd->min_w = get_sections_minsum(g->gd->cols);
	g->wd->max_w = get_sections_maxsum(g->gd->cols);

	g->wd->min_h = get_sections_minsum(g->gd->rows);
	g->wd->max_h = get_sections_maxsum(g->gd->rows);
}


static int grid_do_layout(GRID *g, WIDGET *child) {
	struct cell *c;
	struct section *row, *col;
	int w, h, cx, cy, cw, ch;
	int ox, oy, ow, oh;
	int force_update = 0;

	/* if child is new, get familar with it */ 
	if (child->wd->update & WID_UPDATE_NEWCHILD) {
		force_update = 1;
		child->wd->update &= ~WID_UPDATE_NEWCHILD;
	}

	/* check if size of child is in its valid min/max range */
	w = child->wd->w;
	h = child->wd->h;

	if (w <= child->wd->min_w) w = child->wd->min_w;
	if (w >= child->wd->max_w) w = child->wd->max_w;
	if (h <= child->wd->min_h) h = child->wd->min_h;
	if (h >= child->wd->max_h) h = child->wd->max_h;

	/* if min/max of child changed dramatically, relayout grid */
	if (!(c = get_cell(g, child))) return 0;

	col = c->col;
	row = c->row;

	/*
	 * If a min size a the child conflicts with a current max
	 * constraint of a non-fixed row/column, we need to recalculate
	 * the grid layout.
	 */
	if ((col && col->fixed < 0 && child->wd->min_w + 2*c->pad_x > col->max)
	 || (row && row->fixed < 0 && child->wd->min_h + 2*c->pad_y > row->max))
		force_update = 1;

	/*
	 * If child constrained the min/max properties of its row
	 * or column we have to revisit the constraints by updating
	 * the grid.
	 */
	if ((col && col->minforce == child && child->wd->min_w > col->min)
	 || (col && col->maxforce == child && child->wd->max_w < col->max)
	 || (row && row->minforce == child && child->wd->min_h > row->min)
	 || (row && row->maxforce == child && child->wd->max_h < row->max))
		force_update = 1;

	if (force_update) {
		g->wd->update |= WID_UPDATE_MINMAX;
		g->gen->update(g);
		return 0;
	}

	/* remember old widget position */
	ox = child->wd->x;
	oy = child->wd->y;
	ow = child->wd->w;
	oh = child->wd->h;
	
	/* place child cosy within its cell */
	cx = col ? col->offset + c->pad_x : 0;
	cy = row ? row->offset + c->pad_y : 0;
	cw = col ? col->size - 2*c->pad_x : 0;
	ch = row ? row->size - 2*c->pad_y : 0;
	position_widget(child, cx, cy, cw, ch, c->sticky);

	/* determine area to update */
	ox = MIN(ox, child->wd->x);
	oy = MIN(oy, child->wd->y);
	ow = MAX(ow, child->wd->w);
	oh = MAX(oh, child->wd->h);
	redraw->draw_widgetarea(g, ox, oy, ox+ow-1, oy+oh-1);

	return 0;
}


/*** FREE GRID DATA ***/
static void grid_free_data(GRID *g) {
	struct cell *cc = g->gd->cells, *nc;

	/* decrement ref counters of all children */
	while (cc) {
		nc = cc->next;
		if (cc->wid) cc->wid->gen->release(cc->wid);
		cc = nc;
	}
	
	/* free cell list and section lists */
	FREE_CONNECTED_LIST(struct cell,    g->gd->cells, free);
	FREE_CONNECTED_LIST(struct section, g->gd->rows,  free);
	FREE_CONNECTED_LIST(struct section, g->gd->cols,  free);
	
	/* free cell map */
	free(g->gd->cellmap);
}


/*** DETERMINE POSITION OF WIDGET WITHIN GRID ***/
static inline int get_position(GRID *g, WIDGET *cw, int *out_x, int *out_y) {
	struct cell *cc;
	int x, y;

	/* go through the cellmap */
	for (y = 0; y < g->gd->num_rows; y++) for (x = 0; x < g->gd->num_cols; x++) {
		cc = g->gd->cellmap[g->gd->num_cols*y + x];
		if (cc && (cc->wid == cw)) {
			*out_x = x;
			*out_y = y;
			return 1;
		}
	}
	return 0;
}


/*** DETERMINE NEXT KEYBOARD FOCUS WIDGET WITHIN THE GRID ***
 *
 * \param dir  direction of search (1 = forward, -1 = backward)
 * \param off  start offset
 */
static inline WIDGET *get_next_kfocus(GRID *g, int x, int y, int dir, int off) {
	int max = g->gd->num_cols * g->gd->num_rows;
	int offset;
	WIDGET *nw;
	struct cell *cc;

	/* determine start offset in cellmap to begin the search */
	offset = (y*g->gd->num_cols + x + off) % max;
	for (; (offset >= 0) && (offset < max); offset += dir) {

		cc = g->gd->cellmap[offset];
		if (cc && cc->wid && (nw = cc->wid->gen->first_kfocus(cc->wid)))
			return nw;
	}
	return NULL;
}


/*** HANDLE EVENT ***
 *
 * We only need to evaluate events that refer to the keyboard focus.
 * All other events get passed to the parent
 */
static void (*orig_handle_event) (GRID *e, EVENT *ev, WIDGET *from);
static void grid_handle_event(GRID *g, EVENT *ev, WIDGET *from) {
	int x, y;
	WIDGET *nw;

	if (!from
	 || (ev->type != L4RE_EV_KEY || ev->value != 1)
	 || (ev->code != L4RE_KEY_TAB)) {
		orig_handle_event(g, ev, from);
		return;
	}

	/* determine current keyboard focus position */
	if (get_position(g, from, &x, &y)) {
		nw = get_next_kfocus(g, x, y, 1, 1);
		if (nw) {
			nw->gen->focus(nw);
			return;
		}
	}

	orig_handle_event(g, ev, from);
}


/*** RETURN FIRST CHILD WIDGET THAT CAN TAKE THE KEYBOARD FOCUS ***/
static WIDGET *grid_first_kfocus(GRID *g) {
	return get_next_kfocus(g, 0, 0, 1, 0);
}


/*** RETURN WIDGET TYPE IDENTIFIER ***/
static char *grid_get_type(GRID *g) {
  (void)g;
	return "Grid";
}

static struct widget_methods gen_methods;


/*****************************
 *** GRID SPECIFIC METHODS ***
 *****************************/

/*** ADD NEW CHILD WIDGET TO GRID ***/
static void grid_add(GRID *g, WIDGET *new_elem) {
	struct cell *new_cell;

	/* abort if there is an cyclic parent relationship */
	if (!new_elem || (new_elem && new_elem->gen->related_to(new_elem, g))) return;

	/* free child from previous parent relationship */
	if (new_elem->gen->get_parent(new_elem)) new_elem->gen->release(new_elem);

	/*
	 * Create new cell with default values.
	 * The widget has no row/column information yet.
	 * It will appear on screen as soon as the row/column values are defined.
	 */
	new_cell = (struct cell *)zalloc(sizeof(struct cell));
	if (!new_cell) {
		INFO(printf("Grid(grid_add): out of memory during creation of a grid cell\n");)
		return;
	}

	new_cell->row = NULL;
	new_cell->col = NULL;
	new_cell->row_span = 1;
	new_cell->col_span = 1;
	new_cell->sticky =  GRID_STICKY_NORTH | GRID_STICKY_SOUTH |
	                    GRID_STICKY_EAST | GRID_STICKY_WEST;
	new_cell->pad_x = 0;
	new_cell->pad_y = 0;
	new_cell->wid = new_elem;
	new_elem->gen->inc_ref(new_elem);
	new_elem->gen->set_parent(new_elem, g);
	new_cell->next = g->gd->cells;
	g->gd->cells = new_cell;

	g->gd->update |= GRID_UPDATE_CELLMAP;
}


/*** REMOVE CHILD WIDGET FROM GRID ***/
static void grid_remove(GRID *g, WIDGET *element) {
	struct cell *prev_cell;
	struct cell *cell = get_cell(g, element);

	if (!cell) return;
	if (!element) return;
	if (element->gen->get_parent(element) != g) return;
	
	element->gen->set_parent(element, NULL);
	element->gen->dec_ref(element);
	cell->wid = NULL;

	g->gd->update |= GRID_UPDATE_CELLMAP;
	g->wd->update |= WID_UPDATE_MINMAX;

	/* is cell first element of cell list? */
	if (cell == g->gd->cells) {
		g->gd->cells = cell->next;
		free(cell);

	/* find previous cell in cell-list */
	} else {
		prev_cell = g->gd->cells;
		while (prev_cell->next) {
			if (prev_cell->next == cell) {
				prev_cell->next = cell->next;
				free(cell);
				break;
			}
			prev_cell = prev_cell->next;
		}
	}
	gen_methods.update(g);
}


/*** GET/SET ROW OF A CHILD ***/
static void grid_set_row(GRID *g, WIDGET *w, s32 row_idx) {
	struct cell *cell = get_cell(g, w);
	if (!cell) return;
	cell->row = get_row(g, row_idx);
	if (cell->row && cell->col) g->gd->update |= GRID_UPDATE_CELLMAP;
}


/*** GET/SET COLUMN OF A CHILD ***/
static void grid_set_col(GRID *g, WIDGET *w, s32 col_idx) {
	struct cell *cell = get_cell(g, w);
	if (!cell) return;
	cell->col = get_column(g, col_idx);
	if (cell->row && cell->col) g->gd->update |= GRID_UPDATE_CELLMAP;
}


/*** GET/SET ROWSPAN OF A CHILD ***/
static void grid_set_row_span(GRID *g, WIDGET *w, s32 num_rows) {
	struct cell *cell = get_cell(g, w);
	if (!cell) return;
	cell->row_span = num_rows;
	g->gd->update |= GRID_UPDATE_CELLMAP;
}


/*** GET/SET COLUMNSPAN OF A CHILD ***/
static void grid_set_col_span(GRID *g, WIDGET *w, s32 num_cols) {
	struct cell *cell = get_cell(g, w);
	if (!cell) return;
	cell->col_span = num_cols;
	g->gd->update = g->gd->update | GRID_UPDATE_CELLMAP;
}


/*** GET/SET PAD-X OF A CHILD ***/
static void grid_set_pad_x(GRID *g, WIDGET *w, s32 pad_x) {
	struct cell *cell = get_cell(g, w);
	if (!cell) return;
	cell->pad_x = pad_x;
	g->wd->update |= WID_UPDATE_MINMAX;
}


/*** GET/SET PAD-Y OF A CHILD ***/
static void grid_set_pad_y(GRID *g, WIDGET *w, s32 pad_y) {
	struct cell *cell = get_cell(g, w);
	if (!cell) return;
	cell->pad_y = pad_y;
	g->wd->update |= WID_UPDATE_MINMAX;
}


/*** GET/SET ALIGNMENT OF A CHILD INSIDE ITS CELL ***/
static void grid_set_sticky(GRID *g, WIDGET *w, s32 sticky) {
	struct cell *cell = get_cell(g, w);
	if (!cell) return;
	cell->sticky = sticky;
	g->wd->update |= WID_UPDATE_MINMAX;
}


/*** GET/SET ROW PIXEL SIZE ***/
static void grid_set_row_h(GRID *g, u32 row_idx, u32 row_height) {
	struct section *row = get_row(g, row_idx);
	if (!row) return;
	row->fixed = row_height;
	g->wd->update |= WID_UPDATE_MINMAX;
}


/*** GET/SET COLUMN PIXEL SIZE ***/
static void grid_set_col_w(GRID *g, u32 col_idx, u32 col_width) {
	struct section *col = get_column(g, col_idx);
	if (!col) return;
	col->fixed = col_width;
	g->wd->update |= WID_UPDATE_MINMAX;
}


/*** GET/SET ROW WEIGHT ***/
static void grid_set_row_weight(GRID *g, u32 row_idx, float row_weight) {
	struct section *row = get_row(g, row_idx);
	if (!row) return;
	row->weight = row_weight;
	row->fixed  = -1;
	g->wd->update |= WID_UPDATE_MINMAX;
}


/*** GET/SET COLUMN WEIGHT ***/
static void grid_set_col_weight(GRID *g, u32 col_idx, float col_weight) {
	struct section *col = get_column(g, col_idx);
	if (!col) return;
	col->weight = col_weight;
	col->fixed  = -1;
	g->wd->update |= WID_UPDATE_MINMAX;
}


static struct grid_methods grid_methods = {
	grid_add,
	grid_remove,
	grid_set_row,
	grid_set_col,
};


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static GRID *create(void) {

	GRID *new = ALLOC_WIDGET(struct grid);
	SET_WIDGET_DEFAULTS(new, struct grid, &grid_methods);

	/* set grid specific widget attributes */
	new->wd->flags |=  WID_FLAGS_CONCEALING;

	return new;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct grid_services services = {
	create
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/


static void script_column_config(GRID *g, long index, float weight, long width) {

	/*
	 * If no arguments are specified and the column does not yet exists,
	 * create a column with default width.
	 */
	if (weight == -1 && width == -1
	 && !get_section(g->gd->cols, index)) weight = 1.0;

	if (width!=-1) grid_set_col_w(g, index, width);
	else if (weight > 0.0) grid_set_col_weight(g, index, weight);
	gen_methods.update(g);
}


static void script_row_config(GRID *g, long index, float weight, long width) {

	/*
	 * If no arguments are specified and the row does not yet exists,
	 * create a row with default width.
	 */
	if (weight == -1 && width == -1
	 && !get_section(g->gd->rows, index)) weight = 1.0;

	if (width!=-1) grid_set_row_h(g, index, width);
	else if (weight > 0.0) grid_set_row_weight(g, index, weight);
	gen_methods.update(g);
}


static void script_place_widget(GRID *g, WIDGET *w, long col, long row,
                                long col_span, long row_span,
                                long pad_x, long pad_y,
                                char *align) {
	if (!w) return;

	/* check if widget is a child of the grid - if not, try to adopt it */
	if (!w->gen->get_parent(w)) grid_add(g, w);

	if (col != 9999) grid_set_col(g, w, col);
	if (row != 9999) grid_set_row(g, w, row);
	if (col_span != 9999) grid_set_col_span(g, w, col_span);
	if (row_span != 9999) grid_set_row_span(g, w, row_span);
	if (pad_x != 9999) grid_set_pad_x(g, w, pad_x);
	if (pad_y != 9999) grid_set_pad_y(g, w, pad_y);

	/* set alignment */
	if ((align) && (*align)) {
		int sticky = 0;
		do {
			switch (*align) {
			case 'c': break;
			case 'n': sticky |= GRID_STICKY_NORTH; break;
			case 's': sticky |= GRID_STICKY_SOUTH; break;
			case 'e': sticky |= GRID_STICKY_EAST; break;
			case 'w': sticky |= GRID_STICKY_WEST; break;
			default: sticky = -1;
			}

		} while (*(++align) != 0);
		grid_set_sticky(g, w, sticky);
	}

	/* sweep the market for the newly placed widget */
	erase_overlapping_cells(g, get_cell(g, w));

	g->wd->update |= WID_UPDATE_MINMAX;
	w->wd->update |= WID_UPDATE_NEWCHILD;
	gen_methods.update(g);
}


static void build_script_lang(void) {
	void *widtype;

	widtype = script->reg_widget_type("Grid", (void *(*)(void))create);

	script->reg_widget_method(widtype, "void columnconfig(long index,float weight=-1.0,long size=-1)", &script_column_config);
	script->reg_widget_method(widtype, "void rowconfig(long index,float weight=-1.0,long size=-1)", &script_row_config);
	script->reg_widget_method(widtype, "void place(Widget child,long column=9999,long row=9999,long columnspan=9999,long rowspan=9999,long padx=9999,long pady=9999,string align=\"\")", script_place_widget);
	script->reg_widget_method(widtype, "void add(Widget child)", grid_add);
	script->reg_widget_method(widtype, "void remove(Widget child)", grid_remove);

	widman->build_script_lang(widtype, &gen_methods);
}


int init_grid(struct dope_services *d) {

	widman = d->get_module("WidgetManager 1.0");
	gfx    = d->get_module("Gfx 1.0");
	bg     = d->get_module("Background 1.0");
	script = d->get_module("Script 1.0");
	redraw = d->get_module("RedrawManager 1.0");

	/* define general widget functions */
	widman->default_widget_methods(&gen_methods);

	orig_updatepos    = gen_methods.updatepos;
	orig_update       = gen_methods.update;
	orig_handle_event = gen_methods.handle_event;

	gen_methods.get_type     = grid_get_type;
	gen_methods.draw         = grid_draw;
	gen_methods.find         = grid_find;
	gen_methods.updatepos    = grid_updatepos;
	gen_methods.calc_minmax  = grid_calc_minmax;
	gen_methods.do_layout    = grid_do_layout;
	gen_methods.update       = grid_update;
	gen_methods.free_data    = grid_free_data;
	gen_methods.remove_child = grid_remove;
	gen_methods.first_kfocus = grid_first_kfocus;
	gen_methods.handle_event = grid_handle_event;

	build_script_lang();

	d->register_module("Grid 1.0", &services);
	return 1;
}
