/*
 *	Menu selection window
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

int bg_menu;

int transpose;

static void menufllw(W *w)
{
	MENU *m = (MENU *)w->object;
	if (transpose) {
		if ((m->cursor % m->lines) < m->top)
			m->top = m->cursor % m->lines;
		else if (m->cursor % m->lines >= (m->top % m->lines) + m->h)
			m->top = (m->cursor % m->lines) - (m->h - 1);
	} else {
		if (m->cursor < m->top)
			m->top = m->cursor - m->cursor % m->perline;
		else if (m->cursor >= m->top+m->perline*m->h)
			m->top = m->cursor - m->cursor % m->perline - m->perline*(m->h-1);
	}
}

static void menudisp(W *w, int flg)
{
	MENU *m = (MENU *)w->object;
	ptrdiff_t col;
	int x;
	int y;
	int (*s)[COMPOSE] = m->t->t->scrn + m->x + m->y * m->t->t->co;
	int *a = m->t->t->attr + m->x + m->y * m->t->t->co;
	ptrdiff_t cut = m->nitems % m->lines;

	if (!cut) cut = m->lines;

	for (y = 0; y != m->h; ++y) {
		col = 0;
		if (transpose) {
			if (y < m->lines)
				for (x = 0; x < ((y + m->top) >= cut ? m->perline - 1 : m->perline) ; ++x) {
					int atr;
					ptrdiff_t index = x * m->lines + y + m->top;
			
					if (index == m->cursor && m->t->curwin == m->parent)
						atr = INVERSE|BG_COLOR(bg_menu);
					else
						atr = BG_COLOR(bg_menu);

					if (col == m->w)
						break;

					/* Generate field */
					genfield(m->t->t,
						 s + col,
						 a + col,
						 m->x + col,
						 m->y + y,
						 0,
						 m->list[index],
						 zlen(m->list[index]),
						 atr,
						 m->width,
						 0,NULL);

					col += m->width;

					/* Space between columns */
					if (col != m->w) {
						outatr(locale_map, m->t->t, s + col, a + col, m->x + col, m->y+y, ' ', BG_COLOR(bg_menu));
						++col;
					}
				}
		} else {
			for (x = 0; x != m->perline && y * m->perline + x + m->top < m->nitems; ++x) {
				int atr;
				ptrdiff_t index = x + y * m->perline + m->top;
		
				if (index == m->cursor && m->t->curwin==m->parent)
					atr = INVERSE|BG_COLOR(bg_menu);
				else
					atr = BG_COLOR(bg_menu);

				if (col == m->w)
					break;

				/* Generate field */
				genfield(m->t->t,
					 s + col,
					 a + col,
					 m->x + col,
					 m->y + y,
					 0,
					 m->list[index],
					 zlen(m->list[index]),
					 atr,
					 m->width,
					 0,NULL);

				col += m->width;

				/* Space between columns */
				if (col != m->w) {
					outatr(locale_map, m->t->t, s + col, a + col, m->x + col, m->y+y, ' ', BG_COLOR(bg_menu));
					++col;
				}
			}
		}
		outatr_complete(m->t->t);
		/* Clear to end of line */
		if (col != m->w)
			eraeol(m->t->t, m->x + col, m->y + y, BG_COLOR(bg_menu));
		s += m->t->t->co;
		a += m->t->t->co;
	}
	if (transpose) {
		m->parent->cury = (m->cursor % m->lines) - m->top;
		col = txtwidth(m->list[m->cursor],zlen(m->list[m->cursor]));
		if (col < m->width)
			m->parent->curx = (m->cursor / m->lines) * (m->width + 1) + col;
		else
			m->parent->curx = (m->cursor / m->lines) * (m->width + 1) + m->width;
	} else {
		m->parent->cury = (m->cursor - m->top) / m->perline;
		col = txtwidth(m->list[m->cursor],zlen(m->list[m->cursor]));
		if (col < m->width)
			m->parent->curx = ((m->cursor - m->top) % m->perline) * (m->width + 1) + col;
		else
			m->parent->curx = ((m->cursor - m->top) % m->perline) * (m->width + 1) + m->width;
	}
}

static void menumove(W *w, ptrdiff_t x, ptrdiff_t y)
{
	MENU *m = (MENU *)w->object;
	m->x = x;
	m->y = y;
}

static ptrdiff_t mlines(char **s, ptrdiff_t w)
{
	ptrdiff_t x;
	ptrdiff_t lines;
	ptrdiff_t width;
	ptrdiff_t nitems;
	ptrdiff_t perline;

	for (x = 0, width = 0; s[x]; ++x) {
		ptrdiff_t d = txtwidth(s[x],zlen(s[x]));
		if (d > width)
			width = d;
	}
	nitems = x;
	if (width > w - 1)
		width = w - 1;
	perline = w / (width + 1);

	lines = (nitems + perline - 1) / perline;

	return lines;
}

static void mconfig(MENU *m)
{
	/* Configure menu display parameters */
	if (m->list) {
		ptrdiff_t x;
		/* int lines; */

		m->top = 0;
		for (x = 0, m->width = 0; m->list[x]; ++x) {
			ptrdiff_t d = txtwidth(m->list[x],zlen(m->list[x]));
			if (d > m->width)
				m->width = d;
		}
		m->nitems = x;
		if (m->width > m->w - 1)
			m->width = m->w - 1;

		m->fitline = m->w / (m->width + 1);

		m->lines = (m->nitems + m->fitline - 1) / m->fitline;

		if (transpose)
			m->perline = (m->nitems + m->lines - 1) / m->lines;
		else
			m->perline = m->fitline;
	}
}

static void menuresz(W *w, ptrdiff_t wi, ptrdiff_t he)
{
	MENU *m = (MENU *)w->object;
	m->w = wi;
	m->h = he;
	mconfig(m);
}

int umbol(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	if (transpose)
		m->cursor %= m->lines;
	else
		m->cursor -= m->cursor % m->perline;
	return 0;
}

int umbof(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	m->cursor = 0;
	return 0;
}

int umeof(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	if (m->nitems) {
		if (transpose && (m->nitems % m->lines != 0)) {
			m->cursor = m->lines - 1 + (m->lines * (m->perline - 2));
		} else {
			m->cursor = m->nitems - 1;
		}
	}
	return 0;
}

int umeol(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	if (transpose) {
		ptrdiff_t cut = m->nitems % m->lines;
		if (!cut) cut = m->lines;
		m->cursor %= m->lines;
		if (m->cursor >= cut)
			m->cursor += m->lines * (m->perline - 2);
		else
			m->cursor += m->lines * (m->perline - 1);
	} else {
		m->cursor -= m->cursor % m->perline;

		if (m->cursor+m->perline-1 >= m->nitems)
			m->cursor = m->nitems - 1;
		else
			m->cursor += m->perline - 1;
	}

	return 0;
}

int umrtarw(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	if (transpose) {
		ptrdiff_t cut = m->nitems % m->lines;
		if (!cut) cut = m->lines;
		if (m->cursor % m->lines >= cut) {
			if (m->cursor / m->lines != m->perline - 2) {
				m->cursor += m->lines;
				return 0;
			}
		} else {
			if (m->cursor / m->lines != m->perline - 1) {
				m->cursor += m->lines;
				return 0;
			}
		}
		if ((m->cursor % m->lines) + 1 < m->lines) {
			m->cursor = m->cursor % m->lines + 1;
			return 0;
		}
		return -1;
	} else if (m->cursor + 1 < m->nitems) {
		++m->cursor;
		return 0;
	} else
		return -1;
}

int umtab(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	if (m->cursor + 1 >= m->nitems)
		m->cursor = 0;
	else
		++ m->cursor;
	return 0;
}

int umltarw(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	if (transpose && m->cursor >= m->lines) {
		m->cursor -= m->lines;
		return 0;
	} else if (transpose && m->cursor) {
		ptrdiff_t cut = m->nitems % m->lines;
		if (!cut) cut = m->lines;
		--m->cursor;
		if (m->cursor >= cut)
			m->cursor += (m->perline - 2) * m->lines;
		else
			m->cursor += (m->perline - 1) * m->lines;
		return 0;
	} else if (!transpose && m->cursor) {
		--m->cursor;
		return 0;
	} else
		return -1;
}

int umuparw(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	if (transpose && (m->cursor % m->lines)) {
		--m->cursor;
		return 0;
	} else if (!transpose && m->cursor >= m->perline) {
		m->cursor -= m->perline;
		return 0;
	} else
		return -1;
}

int umdnarw(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	if (transpose) {
		if (m->cursor != m->nitems - 1 && m->cursor % m->lines != m->lines - 1) {
			++m->cursor;
			return 0;
		} else {
			return -1;
		}
	} else {
		ptrdiff_t col = m->cursor % m->perline;

	        m->cursor -= col;

		if (m->cursor + m->perline < m->nitems) {
			m->cursor += m->perline;
			if (m->cursor + col >= m->nitems)
				if (m->nitems)
					m->cursor = m->nitems - 1;
				else
					m->cursor = 0;
			else
				m->cursor += col;
			return 0;
		} else {
			m->cursor += col;
			return -1;
		}
	}
}

void menujump(MENU *m,ptrdiff_t x,ptrdiff_t y)
{
	ptrdiff_t pos = m->top;
	if (transpose) {
		pos += y;
		pos += (x / (m->width + 1)) * m->lines;
	} else {
		pos += y * m->perline;
		pos += x / (m->width + 1);
	}
	if (pos >= m->nitems)
		pos = m->nitems - 1;
	if (pos < 0)
		pos = 0;
	m->cursor = pos;
}

static int mscrup(MENU *m,ptrdiff_t amnt)
{
	if (transpose) {
		if (m->top >= amnt) {
			m->top -= amnt;
			m->cursor -= amnt;
			return 0;
		} else if (m->top) {
			m->cursor -= m->top;
			m->top = 0;
			return 0;
		} else if (m->cursor % m->lines) {
			m->cursor -= (m->cursor % m->lines);
			return 0;
		} else
			return -1;
	} else {
		if (m->top >= amnt*m->perline) {
			m->top -= amnt*m->perline;
			m->cursor -= amnt*m->perline;
			return 0;
		} else if (m->top) {
			m->cursor -= m->top;
			m->top = 0;
			return 0;
		} else if (m->cursor >= m->perline) {
			m->cursor = m->cursor % m->perline;
			return 0;
		} else
			return -1;
	}
}

int umscrup(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	return mscrup(m, 1);
}

int umpgup(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	return mscrup(m, (m->h + 1) / 2);
}

static int mscrdn(MENU *m, ptrdiff_t amnt)
{
	if (transpose) {
		ptrdiff_t col = m->cursor / m->lines;
		ptrdiff_t y = m->cursor % m->lines;
		ptrdiff_t h = m->lines;
		ptrdiff_t t = m->top;
		ptrdiff_t cut = m->nitems % m->lines;
		if (!cut) cut = m->lines;
		m->cursor %= m->lines;

		if (t + m->h + amnt <= h) {
			m->top += amnt;
			m->cursor += amnt;
			if (m->cursor >= cut && col == m->perline - 1)
				--col;
			m->cursor += col * m->lines;
			return 0;
		} else if (t + m->h < h) {
			amnt = h - (t + m->h);
			m->top += amnt;
			m->cursor += amnt;
			if (m->cursor >= cut && col == m->perline - 1)
				--col;
			m->cursor += col * m->lines;
			return 0;
		} else if (y + 1 != h) {
			m->cursor = h - 1;
			if (m->cursor >= cut && col == m->perline - 1)
				--col;
			m->cursor += col * m->lines;
			return 0;
		} else {
			m->cursor += col * m->lines;
			return -1;
		}
	} else {
		ptrdiff_t col = m->cursor % m->perline;
		ptrdiff_t y = m->cursor / m->perline;
		ptrdiff_t h = (m->nitems + m->perline - 1) / m->perline;
		ptrdiff_t t = m->top / m->perline;
		m->cursor -= col;

		if (t + m->h + amnt <= h) {
			m->top += amnt*m->perline;
			m->cursor += amnt*m->perline;
			if (m->cursor + col >= m->nitems)
				if (m->nitems)
					m->cursor = m->nitems - 1;
				else
					m->cursor = 0;
			else
				m->cursor += col;
			return 0;
		} else if (t + m->h < h) {
			amnt = h - (t + m->h);
			m->top += amnt*m->perline;
			m->cursor += amnt*m->perline;
			if (m->cursor + col >= m->nitems)
				if (m->nitems)
					m->cursor = m->nitems - 1;
				else
					m->cursor = 0;
			else
				m->cursor += col;
			return 0;
		} else if (y+1!=h) {
			m->cursor = (h-1)*m->perline;
			if (m->cursor + col >= m->nitems)
				if (m->nitems)
					m->cursor = m->nitems - 1;
				else
					m->cursor = 0;
			else
				m->cursor += col;
			return 0;
		} else {
			m->cursor += col;
			return -1;
		}
	}
}

int umscrdn(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	return mscrdn(m, 1);
}

int umpgdn(W *w, int k)
{
	MENU *m;
	WIND_MENU(m, w);
	return mscrdn(m, (m->h + 1) / 2);
}

static int umrtn(W *w)
{
	MENU *m = (MENU *)w->object;
	dostaupd = 1;
	if (m->func)
		return m->func(m, m->cursor, m->object, 0);
	else
		return -1;
}

int umbacks(W *w, int c)
{
	MENU *m;
	WIND_MENU(m, w);
	if (m->backs)
		return m->backs(m, m->cursor, m->object);
	else
		return -1;
}

/* Fold upper, lower and Ctrl-letter to the same values */

static int menufold(int c)
{
	if (c >= 'a' && c <= 'z')
		return c & 0x1f;
	else if (c >= 'A' && c <= 'Z')
		return c & 0x1f;
	else
		return c;
}

static int umkey(W *w, int c)
{
	MENU *m = (MENU *)w->object;
	int x;
	int n = 0;

	if (c == '-' && m->func) {
		if (m->func)
			return m->func(m, m->cursor, m->object, -1);
		else
			return -1;
	}
	if (c == '+' && m->func) {
		if (m->func)
			return m->func(m, m->cursor, m->object, 1);
		else
			return -1;
	}

	c = menufold(c);
	for (x = 0; x != m->nitems; ++x)
		if (menufold(m->list[x][0]) == c)
			++n;
	if (!n)
		return -1;
	if (n == 1)
		for (x = 0; x != m->nitems; ++x)
			if (menufold(m->list[x][0]) == c) {
				m->cursor = x;
				return umrtn(m->parent);
			}
	do {
		++m->cursor;
		if (m->cursor == m->nitems)
			m->cursor = 0;
	} while (menufold(m->list[m->cursor][0]) != c);

	return -1;
}

static int menuabort(W *w)
{
	MENU *m = (MENU *)w->object;
	int (*abrt)(W *w, ptrdiff_t cursor, void *object) = m->abrt;
	void *object = m->object;
	ptrdiff_t x = m->cursor;
	W *win = w->win;

	joe_free(m);
	if (abrt)
		return abrt(win, x, object);
	else
		return -1;
}

WATOM watommenu = {
	"menu",
	menudisp,
	menufllw,
	menuabort,
	umrtn,
	umkey,
	menuresz,
	menumove,
	NULL,
	NULL,
	TYPEMENU
};

void ldmenu(MENU *m, char **s, ptrdiff_t cursor)
{
	m->list = s;
	m->cursor = cursor;
	mconfig(m);
}

int menu_above;

MENU *mkmenu(W *w, W *targ, char **s, int (*func)(MENU *m, ptrdiff_t cursor, void *object, int k),
             int (*abrt)(W *w, ptrdiff_t cursor, void *object), int (*backs)(MENU *m, ptrdiff_t cursor, void *object), ptrdiff_t cursor, void *object, int *notify)
{
	W *neww;
	MENU *m;
	ptrdiff_t lines;
	ptrdiff_t h = (w->main->h*40) / 100; /* 40% of window size */
	if (!h)
		h = 1;
	
	if (s) {
		lines = mlines(s, w->t->w);
		if (lines < h)
			h = lines;
	}

	neww = wcreate(w->t, &watommenu, w, targ, targ->main, h, NULL, notify);

	if (!neww) {
		if (notify)
			*notify = 1;
		return NULL;
	}
	wfit(neww->t);
	neww->object = (void *) (m = (MENU *) joe_malloc(SIZEOF(MENU)));
	m->parent = neww;
	m->func = func;
	m->abrt = abrt;
	m->backs = backs;
	m->object = object;
	m->t = w->t;
	m->h = neww->h;
	m->w = neww->w;
	m->x = neww->x;
	m->y = neww->y;
	m->top = 0;
	ldmenu(m, s, cursor);
	w->t->curwin = neww;
	return m;
}

static char *cull(char *a, char *b)
{
	int x;

	for (x = 0; a[x] && b[x] && a[x] == b[x]; ++x) ;
	return vstrunc(a, x);
}

char *find_longest(char **lst)
{
	char *com;
	int x;

	if (!lst || !aLEN(lst))
		return vstrunc(NULL, 0);
	com = vsncpy(NULL, 0, sv(lst[0]));
	for (x = 1; x != aLEN(lst); ++x)
		com = cull(com, lst[x]);
	return com;
}

char *mcomplete(MENU *m)
{
	char *com;
	int x;

	if (!m->nitems)
		return vstrunc(NULL, 0);
	if (m->nitems == 1 && !zcmp(m->list[0], "../"))
		return vstrunc(NULL, 0);
	com = vsncpy(NULL, 0, sz(m->list[0]));
	for (x = 1; x != m->nitems; ++x)
		com = cull(com, m->list[x]);
	return com;
}
