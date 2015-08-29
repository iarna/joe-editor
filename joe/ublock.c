/*
 * 	Highlighted block functions
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

int nowmarking;

/* Global options */

int square = 0;			/* Set for rectangle mode */
int lightoff = 0;		/* Set if highlighting should turn off

				   after block operations */
/* Global variables */

P *markb = NULL;		/* Beginning and end of block */
P *markk = NULL;

/* Push markb & markk */

typedef struct marksav MARKSAV;
struct marksav {
	LINK(MARKSAV) link;
	P *markb, *markk;
} markstack = { { &markstack, &markstack} };
MARKSAV markfree = { {&markfree, &markfree} };
int nstack = 0;

int upsh(W *w, int k)
{
	MARKSAV *m = (MARKSAV *)alitem(&markfree, SIZEOF(MARKSAV));

	m->markb = 0;
	m->markk = 0;
	if (markk)
		pdupown(markk, &m->markk, "upsh");
	if (markb)
		pdupown(markb, &m->markb, "upsh");
	enqueb(MARKSAV, link, &markstack, m);
	++nstack;
	return 0;
}

int upop(W *w, int k)
{
	MARKSAV *m = markstack.link.prev;

	if (m != &markstack) {
		--nstack;
		prm(markk);
		prm(markb);
		markk = m->markk;
		if (markk)
			markk->owner = &markk;
		markb = m->markb;
		if (markb)
			markb->owner = &markb;
		demote(MARKSAV, link, &markfree, m);
		if (lightoff)
			unmark(w, 0);
		updall();
		return 0;
	} else
		return -1;
}

/* Return true if markb/markk are valid */
/* If r is set, swap markb with markk if necessary */

int autoswap;

int markv(int r)
{
	if (markb && markk && markb->b == markk->b && (r == 2 ? markk->byte >= markb->byte : markk->byte > markb->byte) &&
	    (!square || (r == 2 ? markk->xcol >= markb->xcol : markk->xcol > markb->xcol))) {
		return 1;
	} else if(autoswap && r && markb && markk && markb->b == markk->b && markb->byte > markk->byte && (!square || markk->xcol < markb->xcol)) {
		P *p = pdup(markb, "markv");
		prm(markb); markb=0; pdupown(markk, &markb, "markv");
		prm(markk); markk=0; pdupown(p, &markk, "markv");
		prm(p);
		return 1;
	} else
		return 0;
}

/* Rectangle-mode subroutines */

/* B *pextrect(P *org,off_t height,off_t left,off_t right);
 * Copy a rectangle into a new buffer
 *
 * org points to top-left corner of rectangle.
 * height is number of lines in rectangle.
 * right is rightmost column of rectangle + 1
 */

B *pextrect(P *org, off_t height, off_t right)
{
	P *p = pdup(org, "pextrect");	/* Left part of text to extract */
	P *q = pdup(p, "pextrect");		/* After right part of text to extract */
	B *tmp = bmk(NULL);	/* Buffer to extract to */
	P *z = pdup(tmp->eof, "pextrect");	/* Buffer pointer */

	while (height--) {
		pcol(p, org->xcol);
		pset(q, p);
		pcolwse(q, right);
		p_goto_eof(z);
		binsb(z, bcpy(p, q));
		p_goto_eof(z);
		binsc(z, '\n');
		pnextl(p);
	}
	prm(p);
	prm(q);
	prm(z);
	return tmp;
}

/* void pdelrect(P *org,off_t height,off_t right);
 * Delete a rectangle.
 */

void pdelrect(P *org, off_t height, off_t right)
{
	P *p = pdup(org, "pdelrect");
	P *q = pdup(p, "pdelrect");

	while (height--) {
		pcol(p, org->xcol);
		pset(q, p);
		pcol(q, right);
		bdel(p, q);
		pnextl(p);
	}
	prm(p);
	prm(q);
}

/* void pclrrect(P *org,off_t height,off_t right,int usetabs);
 * Blank-out a rectangle.
 */

void pclrrect(P *org, off_t height, off_t right, int usetabs)
{
	P *p = pdup(org, "pclrrect");
	P *q = pdup(p, "pclrrect");

	while (height--) {
		off_t pos;

		pcol(p, org->xcol);
		pset(q, p);
		pcoli(q, right);
		pos = q->col;
		bdel(p, q);
		pfill(p, pos, usetabs);
		pnextl(p);
	}
	prm(p);
	prm(q);
}

/* int ptabrect(P *org,off_t height,off_t right)
 * Check if there are any TABs in a rectangle
 */

int ptabrect(P *org, off_t height, off_t right)
{
	P *p = pdup(org, "ptabrect");

	while (height--) {
		int c;

		pcol(p, org->xcol);
		while ((c = pgetc(p)) != NO_MORE_DATA && c != '\n') {
			if (c == '\t') {
				prm(p);
				return '\t';
			} else if (piscol(p) > right)
				break;
		}
		if (c != '\n')
			pnextl(p);
	}
	prm(p);
	return ' ';
}

/* Insert rectangle */

void pinsrect(P *cur, B *tmp, off_t width, int usetabs)
{
	P *p = pdup(cur, "pinsrect");	/* We insert at & move this pointer */
	P *q = pdup(tmp->bof, "pinsrect");	/* These are for scanning through 'tmp' */
	P *r = pdup(q, "pinsrect");

/*	if (width) */
		while (pset(r, q), p_goto_eol(q), (q->line != tmp->eof->line || piscol(q))) {
			pcol(p, cur->xcol);
			if (piscol(p) < cur->xcol)
				pfill(p, cur->xcol, usetabs);
			binsb(p, bcpy(r, q));
			pfwrd(p, q->byte - r->byte);
			if (piscol(p) < cur->xcol + width)
				pfill(p, cur->xcol + width, usetabs);
			if (piseol(p))
				pbackws(p);
			if (!pnextl(p)) {
				binsc(p, '\n');
				pgetc(p);
			}
			if (pgetc(q) == NO_MORE_DATA)
				break;
		}
	prm(p);
	prm(q);
	prm(r);
}

/* Block functions */

/* Set beginning */

int umarkb(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	pdupown(bw->cursor, &markb, "umarkb");
	markb->xcol = bw->cursor->xcol;
	updall();
	return 0;
}

int udrop(W *w, int k)
{
	prm(markk);
	if (marking && markb)
		prm(markb);
	else
		umarkb(w, 0);
	return 0;
}

int ubegin_marking(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (nowmarking) {
		/* We're marking now... don't stop */
		return 0;
	} else if (markv(0) && bw->cursor->b==markb->b) {
		/* Try to extend current block */
		if (bw->cursor->byte==markb->byte) {
			pset(markb,markk);
			prm(markk); markk=0;
			nowmarking = 1;
			return 0;
		} else if(bw->cursor->byte==markk->byte) {
			prm(markk); markk=0;
			nowmarking = 1;
			return 0;
		}
	}
	/* Start marking - no message */
	prm(markb); markb=0;
	prm(markk); markk=0;
	updall();
	nowmarking = 1;
	return umarkb(bw->parent, 0);
}

int utoggle_marking(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markv(0) && bw->cursor->b==markb->b && bw->cursor->byte>=markb->byte && bw->cursor->byte<=markk->byte) {
		/* Just clear selection */
		prm(markb); markb=0;
		prm(markk); markk=0;
		updall();
		nowmarking = 0;
		msgnw(bw->parent, joe_gettext(_("Selection cleared.")));
		return 0;
	} else if (markk) {
		/* Clear selection and start new one */
		prm(markb); markb=0;
		prm(markk); markk=0;
		updall();
		nowmarking = 1;
		msgnw(bw->parent, joe_gettext(_("Selection started.")));
		return umarkb(bw->parent, 0);
	} else if (markb && markb->b==bw->cursor->b) {
		nowmarking = 0;
		if (bw->cursor->byte<markb->byte) {
			pdupown(markb, &markk, "utoggle_marking");
			prm(markb); markb=0;
			pdupown(bw->cursor, &markb, "utoggle_marking");
			markb->xcol = bw->cursor->xcol;
		} else {
			pdupown(bw->cursor, &markk, "utoggle_marking");
			markk->xcol = bw->cursor->xcol;
		}
		updall(); /* Because other windows could be changed */
		return 0;
	} else {
		nowmarking = 1;
		msgnw(bw->parent, joe_gettext(_("Selection started.")));
		return umarkb(bw->parent, 0);
	}
}

int uselect(W *w, int k)
{
	if (!markb)
		umarkb(w, 0);
	return 0;
}

/* Set end */

int umarkk(W *w, int k)
{
	BW *bw;
	WIND_BW (bw, w);
	pdupown(bw->cursor, &markk, "umarkk");
	markk->xcol = bw->cursor->xcol;
	updall();
	return 0;
}

/* Unset marks */

int unmark(W *w, int k)
{
	prm(markb);
	prm(markk);
	nowmarking = 0;
	updall();
	return 0;
}

/* Mark line */

int umarkl(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	p_goto_bol(bw->cursor);
	umarkb(w, 0);
	pnextl(bw->cursor);
	umarkk(w, 0);
	utomarkb(w, 0);
	pcol(bw->cursor, bw->cursor->xcol);
	return 0;
}

int utomarkb(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markb && markb->b == bw->b) {
		pset(bw->cursor, markb);
		return 0;
	} else
		return -1;
}

int utomarkk(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markk && markk->b == bw->b) {
		pset(bw->cursor, markk);
		return 0;
	} else
		return -1;
}

int uswap(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markb && markb->b == bw->b) {
		P *q = pdup(markb, "uswap");

		umarkb(w, 0);
		pset(bw->cursor, q);
		prm(q);
		return 0;
	} else
		return -1;
}

int utomarkbk(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markb && markb->b == bw->b && bw->cursor->byte != markb->byte) {
		pset(bw->cursor, markb);
		return 0;
	} else if (markk && markk->b == bw->b && bw->cursor->byte != markk->byte) {
		pset(bw->cursor, markk);
		return 0;
	} else
		return -1;
}

/* Delete block */

int ublkdel(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markv(1)) {
		if (markb->b!=bw->b && !modify_logic(bw,markb->b))
			return -1;
		if (square)
			if (bw->o.overtype) {
				off_t ocol = markk->xcol;

				pclrrect(markb, markk->line - markb->line + 1, markk->xcol, ptabrect(markb, markk->line - markb->line + 1, markk->xcol));
				pcol(markk, ocol);
				markk->xcol = ocol;
			} else
				pdelrect(markb, markk->line - markb->line + 1, markk->xcol);
		else
			bdel(markb, markk);
		if (lightoff)
			unmark(bw->parent, 0);
	} else {
		msgnw(bw->parent, joe_gettext(_("No block")));
		return -1;
	}
	return 0;
}

/* Special delete block function for PICO */

int upicokill(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	upsh(w, 0);
	umarkk(w, 0);
	if (markv(1)) {
		if (square)
			if (bw->o.overtype) {
				off_t ocol = markk->xcol;

				pclrrect(markb, markk->line - markb->line + 1, markk->xcol, ptabrect(markb, markk->line - markb->line + 1, markk->xcol));
				pcol(markk, ocol);
				markk->xcol = ocol;
			} else
				pdelrect(markb, markk->line - markb->line + 1, markk->xcol);
		else
			bdel(markb, markk);
		if (lightoff)
			unmark(w, 0);
	} else
		udelln(w, 0);
	return 0;
}

/* Move highlighted block */

int ublkmove(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markv(1)) {
		if (markb->b!=bw->b && !modify_logic(bw,markb->b))
			return -1;
		if (square) {
			off_t height = markk->line - markb->line + 1;
			off_t width = markk->xcol - markb->xcol;
			int usetabs = ptabrect(markb, height, markk->xcol);
			off_t ocol = piscol(bw->cursor);
			B *tmp = pextrect(markb, height, markk->xcol);
			int update_xcol = (bw->cursor->xcol >= markk->xcol && bw->cursor->line >= markb->line && bw->cursor->line <= markk->line);

			ublkdel(w, 0);
			/* now we can't use markb and markk until we set them again */
			/* ublkdel() frees them */
			if (bw->o.overtype) {
				/* If cursor was in block, blkdel moves it to left edge of block, so fix it
				 * back to its original place here */
				pcol(bw->cursor, ocol);
				pfill(bw->cursor, ocol, ' ');
				pdelrect(bw->cursor, height, piscol(bw->cursor) + width);
			} else if (update_xcol)
				/* If cursor was to right of block, xcol was not properly updated */
				bw->cursor->xcol -= width;
			pinsrect(bw->cursor, tmp, width, usetabs);
			brm(tmp);
			if (lightoff)
				unmark(bw->parent, 0);
			else {
				umarkb(bw->parent, 0);
				umarkk(bw->parent, 0);
				pline(markk, markk->line + height - 1);
				pcol(markk, markb->xcol + width);
				markk->xcol = markb->xcol + width;
			}
			return 0;
		} else if (bw->cursor->b != markk->b || bw->cursor->byte > markk->byte || bw->cursor->byte < markb->byte) {
			off_t size = markk->byte - markb->byte;

			binsb(bw->cursor, bcpy(markb, markk));
			bdel(markb, markk);
			if (lightoff)
				unmark(bw->parent, 0);
			else {
				umarkb(bw->parent, 0);
				umarkk(bw->parent, 0);
				pfwrd(markk, size);
			}
			updall();
			return 0;
		}
	}
	msgnw(bw->parent, joe_gettext(_("No block")));
	return -1;
}

/* Duplicate highlighted block */

int ublkcpy(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markv(1)) {
		if (square) {
			off_t height = markk->line - markb->line + 1;
			off_t width = markk->xcol - markb->xcol;
			int usetabs = ptabrect(markb, height, markk->xcol);
			B *tmp = pextrect(markb, height, markk->xcol);

			if (bw->o.overtype)
				pdelrect(bw->cursor, height, piscol(bw->cursor) + width);
			pinsrect(bw->cursor, tmp, width, usetabs);
			brm(tmp);
			if (lightoff)
				unmark(bw->parent, 0);
			else {
				umarkb(bw->parent, 0);
				umarkk(bw->parent, 0);
				pline(markk, markk->line + height - 1);
				pcol(markk, markb->xcol + width);
				markk->xcol = markb->xcol + width;
			}
			return 0;
		} else {
			off_t size = markk->byte - markb->byte;
			B *tmp = bcpy(markb, markk);

			/* Simple overtype for hex mode */
			if (bw->o.hex && bw->o.overtype) {
				P *q = pdup(bw->cursor, "ublkcpy");
				if (q->byte + size >= q->b->eof->byte)
					pset(q, q->b->eof);
				else
					pfwrd(q, size);
				bdel(bw->cursor, q);
				prm(q);
			}

			binsb(bw->cursor, tmp);
			if (lightoff)
				unmark(bw->parent, 0);
			else {
				umarkb(bw->parent, 0);
				umarkk(bw->parent, 0);
				pfwrd(markk, size);
			}
			updall();
			return 0;
		}
	} else {
		msgnw(bw->parent, joe_gettext(_("No block")));
		return -1;
	}
}

/* Write highlighted block to a file */
/* This is called by ublksave in ufile.c */

/*int dowrite(BW *bw, char *s, void *object, int *notify)
{
	if (notify)
		*notify = 1;
	if (markv(1)) {
		if (square) {
			int fl;
			int ret = 0;
			B *tmp = pextrect(markb,
					  markk->line - markb->line + 1,
					  markk->xcol);

			if ((fl = bsave(tmp->bof, s, tmp->eof->byte, 0)) != 0) {
				msgnw(bw->parent, joe_gettext(msgs[-fl]));
				ret = -1;
			}
			brm(tmp);
			if (lightoff)
				unmark(bw);
			vsrm(s);
			return ret;
		} else {
			int fl;
			int ret = 0;

			if ((fl = bsave(markb, s, markk->byte - markb->byte, 0)) != 0) {
				msgnw(bw->parent, joe_gettext(msgs[-fl]));
				ret = -1;
			}
			if (lightoff)
				unmark(bw);
			vsrm(s);
			return ret;
		}
	} else {
		vsrm(s);
		msgnw(bw->parent, _(_("No block")));
		return -1;
	}
}*/

/* Set highlighted block on a program block */

void setindent(BW *bw)
{
	P *p, *q;
	off_t indent;

	if (pisblank(bw->cursor))
		return;

	p = pdup(bw->cursor, "setindent");
	q = pdup(p, "setindent");
	indent = pisindent(p);

	do {
		if (!pprevl(p))
			goto done;
		else
			p_goto_bol(p);
	} while (pisindent(p) >= indent || pisblank(p));
	pnextl(p);
	/* Maybe skip blank lines at beginning */
      done:
	p_goto_bol(p);
	p->xcol = piscol(p);
	if (markb)
		prm(markb);
	markb = p;
	p->owner = &markb;

	do {
		if (!pnextl(q))
			break;
	} while (pisindent(q) >= indent || pisblank(q));
	/* Maybe skip blank lines at end */
	if (markk)
		prm(markk);
	q->xcol = piscol(q);
	markk = q;
	q->owner = &markk;

	updall();
}

/* Purity check */
/* Verifies that at least n indentation characters (for non-blank lines) match c */
/* If n is 0 (for urindent), this fails if c is space but indentation begins with tab */

static int purity_check(int c, off_t n)
{
	P *p = pdup(markb, "purity_check");
	while (p->byte < markk->byte) {
		off_t x;
		p_goto_bol(p);
		if (!n && c==' ' && brc(p)=='\t') {
			prm(p);
			return 0;
		} else if (!piseol(p))
			for (x=0; x!=n; ++x)
				if (pgetc(p)!=c) {
					prm(p);
					return 0;
				}
		pnextl(p);
	}
	prm(p);
	return 1;
}

/* Left indent check */
/* Verify that there is enough whitespace to do the left indent */

static int lindent_check(int c, off_t n)
{
	P *p = pdup(markb, "lindent_check");
	off_t indwid;
	if (c=='\t')
		indwid = n * p->b->o.tab;
	else
		indwid = n;
	while (p->byte < markk->byte) {
		p_goto_bol(p);
		if (!piseol(p) && pisindent(p)<indwid) {
			prm(p);
			return 0;
		}
		pnextl(p);
	}
	prm(p);
	return 1;
}

/* Indent more */

int urindent(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (square) {
		if (markb && markk && markb->b == markk->b && markb->byte <= markk->byte && markb->xcol <= markk->xcol) {
			P *p = pdup(markb, "urindent");

			do {
				pcol(p, markb->xcol);
				pfill(p, markb->xcol + bw->o.istep, bw->o.indentc);
			} while (pnextl(p) && p->line <= markk->line);
			prm(p);
		}
	} else {
		if (!markb || !markk || markb->b != markk->b || bw->cursor->byte < markb->byte || bw->cursor->byte > markk->byte || markb->byte == markk->byte) {
			setindent(bw);
		} else if ( 1 /* bw->o.purify */) {
			P *p = pdup(markb, "urindent");
			P *q = pdup(markb, "urindent");
			off_t indwid;

			if (bw->o.indentc=='\t')
				indwid = bw->o.tab * bw->o.istep;
			else
				indwid = bw->o.istep;

			while (p->byte < markk->byte) {
				p_goto_bol(p);
				if (!piseol(p)) {
					off_t col;
					pset(q, p);
					p_goto_indent(q, bw->o.indentc);
					col = piscol(q);
					bdel(p,q);
					pfill(p,col+indwid,bw->o.indentc);
				}
				pnextl(p);
			}
			prm(p);
			prm(q);
		} else if (purity_check(bw->o.indentc,0)) {
			P *p = pdup(markb, "urindent");

			while (p->byte < markk->byte) {
				p_goto_bol(p);
				if (!piseol(p))
					while (piscol(p) < bw->o.istep) {
						binsc(p, bw->o.indentc);
						pgetc(p);
					}
				pnextl(p);
			}
			prm(p);
		} else {
			/* Purity failure */
			msgnw(bw->parent,joe_gettext(_("Selected lines not properly indented")));
			return 1;
		}
	}
	return 0;
}

/* Indent less */

int ulindent(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (square) {
		if (markb && markk && markb->b == markk->b && markb->byte <= markk->byte && markb->xcol <= markk->xcol) {
			P *p = pdup(markb, "ulindent");
			P *q = pdup(p, "ulindent");

			do {
				pcol(p, markb->xcol);
				while (piscol(p) < markb->xcol + bw->o.istep) {
					int c = pgetc(p);

					if (c != ' ' && c != '\t' && c != bw->o.indentc) {
						prm(p);
						prm(q);
						return -1;
					}
				}
			} while (pnextl(p) && p->line <= markk->line);
			pset(p, markb);
			do {
				pcol(p, markb->xcol);
				pset(q, p);
				pcol(q, markb->xcol + bw->o.istep);
				bdel(p, q);
			} while (pnextl(p) && p->line <= markk->line);
			prm(p);
			prm(q);
		}
	} else {
		if (!markb || !markk || markb->b != markk->b || bw->cursor->byte < markb->byte || bw->cursor->byte > markk->byte || markb->byte == markk->byte) {
			setindent(bw);
		} else if (1 /* bw->o.purify */ && lindent_check(bw->o.indentc,bw->o.istep)) {
			P *p = pdup(markb, "ulindent");
			P *q = pdup(markb, "ulindent");
			off_t indwid;

			if (bw->o.indentc=='\t')
				indwid = bw->o.tab * bw->o.istep;
			else
				indwid = bw->o.istep;

			while (p->byte < markk->byte) {
				p_goto_bol(p);
				if (!piseol(p)) {
					off_t col;
					pset(q, p);
					p_goto_indent(q, bw->o.indentc);
					col = piscol(q);
					bdel(p,q);
					pfill(p,col-indwid,bw->o.indentc);
				}
				pnextl(p);
			}
			prm(p);
			prm(q);
		} else if (purity_check(bw->o.indentc,bw->o.istep)) {
			P *p = pdup(markb, "ulindent");
			P *q = pdup(p, "ulindent");

			p_goto_bol(p);
			while (p->byte < markk->byte) {
				if (!piseol(p)) {
					pset(q, p);
					while (piscol(q) < bw->o.istep)
						pgetc(q);
					bdel(p, q);
				}
				pnextl(p);
			}
			prm(p);
			prm(q);
		} else {
			/* Purity failure */
			msgnw(bw->parent,joe_gettext(_("Selected lines not properly indented")));
			return 1;
		}
	}
	return 0;
}

/* Insert a file */

int doinsf(W *w, char *s, void *object, int *notify)
{
	BW *bw;
	WIND_BW(bw, w);
	if (notify)
		*notify = 1;
	if (square)
		if (markv(2)) {
			B *tmp;
			off_t width = markk->xcol - markb->xcol;
			off_t height;
			int usetabs = ptabrect(markb,
					       markk->line - markb->line + 1,
					       markk->xcol);

			tmp = bload(s);
			if (berror) {
				msgnw(bw->parent, joe_gettext(msgs[-berror]));
				brm(tmp);
				return -1;
			}
			if (piscol(tmp->eof))
				height = tmp->eof->line + 1;
			else
				height = tmp->eof->line;
			if (bw->o.overtype) {
				pclrrect(markb, off_max(markk->line - markb->line + 1, height), markk->xcol, usetabs);
				pdelrect(markb, height, width + markb->xcol);
			}
			pinsrect(markb, tmp, width, usetabs);
			pdupown(markb, &markk, "doinsf");
			markk->xcol = markb->xcol;
			if (height) {
				pline(markk, markk->line + height - 1);
				pcol(markk, markb->xcol + width);
				markk->xcol = markb->xcol + width;
			}
			brm(tmp);
			updall();
			return 0;
		} else {
			msgnw(bw->parent, joe_gettext(_("No block")));
			return -1;
	} else {
		int ret = 0;
		B *tmp = bload(s);

		if (berror) {
			msgnw(bw->parent, joe_gettext(msgs[-berror])), brm(tmp);
			ret = -1;
		} else
			binsb(bw->cursor, tmp);
		vsrm(s);
		bw->cursor->xcol = piscol(bw->cursor);
		return ret;
	}
}


/* Filter highlighted block through a UNIX command */

static int filtflg = 0;

static int dofilt(W *w, char *s, void *object, int *notify)
{
	int fr[2];
	int fw[2];
	int flg = 0;
	BW *bw;
	WIND_BW(bw, w);

	if (notify)
		*notify = 1;
	if (markb && markk && !square && markb->b == bw->b && markk->b == bw->b && markb->byte == markk->byte) {
		flg = 1;
		goto ok;
	} if (!markv(1)) {
		msgnw(bw->parent, joe_gettext(_("No block")));
		return -1;
	}
      ok:
	if (markb->b!=bw->b && !modify_logic(bw,markb->b))
		return -1;

	if (-1 == pipe(fr)) {
		msgnw(bw->parent, joe_gettext(_("Couldn't create pipe")));
		return -1;
	}
	if (-1 == pipe(fw)) {
		msgnw(bw->parent, joe_gettext(_("Couldn't create pipe")));
		return -1;
	}
	npartial(bw->parent->t->t);
	ttclsn();
#ifdef HAVE_FORK
	if (!fork()) {
#else
	if (!vfork()) { /* For AMIGA only */
#endif
#ifdef HAVE_PUTENV
		char *fname;
		const char *name;
		ptrdiff_t len;
#endif
		signrm();
		close(0);
		close(1);
		close(2);
		if (-1 == dup(fw[0])) _exit(1);
		if (-1 == dup(fr[1])) _exit(1);
		if (-1 == dup(fr[1])) _exit(1);
		close(fw[0]);
		close(fr[1]);
		close(fw[1]);
		close(fr[0]);
#ifdef HAVE_PUTENV
		fname = vsncpy(NULL, 0, sc("JOE_FILENAME="));
		name = bw->b->name ? bw->b->name : "Unnamed";
		if((len = slen(name)) >= 512)	/* limit filename length */
			len = 512;
		fname = vsncpy(sv(fname), name, len);
		putenv(fname);
		vsrm(fname);
#endif
		execl("/bin/sh", "/bin/sh", "-c", s, NULL);
		_exit(0);
	}
	close(fr[1]);
	close(fw[0]);
#ifdef HAVE_FORK
	if (fork()) {
#else
	if (vfork()) { /* For AMIGA only */
#endif
		close(fw[1]);
		if (square) {
			B *tmp;
			off_t width = markk->xcol - markb->xcol;
			off_t height;
			int usetabs = ptabrect(markb,
					       markk->line - markb->line + 1,
					       markk->xcol);

			tmp = bread(fr[0], MAXOFF);
			if (piscol(tmp->eof))
				height = tmp->eof->line + 1;
			else
				height = tmp->eof->line;
			if (bw->o.overtype) {
				pclrrect(markb, markk->line - markb->line + 1, markk->xcol, usetabs);
				pdelrect(markb, off_max(height, markk->line - markb->line + 1), width + markb->xcol);
			} else
				pdelrect(markb, markk->line - markb->line + 1, markk->xcol);
			pinsrect(markb, tmp, width, usetabs);
			pdupown(markb, &markk, "dofilt");
			markk->xcol = markb->xcol;
			if (height) {
				pline(markk, markk->line + height - 1);
				pcol(markk, markb->xcol + width);
				markk->xcol = markb->xcol + width;
			}
			if (lightoff)
				unmark(bw->parent, 0);
			brm(tmp);
			updall();
		} else {
			P *p = pdup(markk, "dofilt");
			if (!flg)
				prgetc(p);
			bdel(markb, p);
			binsb(p, bread(fr[0], MAXOFF));
			if (!flg) {
				pset(p,markk);
				prgetc(p);
				bdel(p,markk);
			}
			prm(p);
			if (lightoff)
				unmark(bw->parent, 0);
		}
		close(fr[0]);
		wait(NULL);
		wait(NULL);
	} else {
		if (square) {
			B *tmp = pextrect(markb,
					  markk->line - markb->line + 1,
					  markk->xcol);

			bsavefd(tmp->bof, fw[1], tmp->eof->byte);
		} else
			bsavefd(markb, fw[1], markk->byte - markb->byte);
		close(fw[1]);
		_exit(0);
	}
	vsrm(s);
	ttopnn();
	if (filtflg)
		unmark(bw->parent, 0);
	bw->cursor->xcol = piscol(bw->cursor);
	return 0;
}

static B *filthist = NULL;

static void markall(BW *bw)
{
	pdupown(bw->cursor->b->bof, &markb, "markall");
	markb->xcol = 0;
	pdupown(bw->cursor->b->eof, &markk, "markall");
	markk->xcol = piscol(markk);
	updall();
}

static int checkmark(BW *bw)
{
	if (!markv(1))
		if (square)
			return 2;
		else {
			markall(bw);
			filtflg = 1;
			return 1;
	} else {
		filtflg = 0;
		return 0;
	}
}

int ufilt(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
#ifdef __MSDOS__
	msgnw(bw->parent, joe_gettext(_("Sorry, no sub-processes in DOS (yet)")));
	return -1;
#else
	switch (checkmark(bw)) {
	case 0:
		if (wmkpw(bw->parent, joe_gettext(_("Command to filter block through (^C to abort): ")), &filthist, dofilt, NULL, NULL, utypebw, NULL, NULL, locale_map, 1))
			return 0;
		else
			return -1;
	case 1:
		if (wmkpw(bw->parent, joe_gettext(_("Command to filter file through (^C to abort): ")), &filthist, dofilt, NULL, NULL, utypebw, NULL, NULL, locale_map, 1))
			return 0;
		else
			return -1;
	case 2:
	default:
		msgnw(bw->parent, joe_gettext(_("No block")));
		return -1;
	}
#endif
}

/* Force region to lower case */

int ulower(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markv(1)) {
		P *q;
	        P *p;
	        int c;
		B *b = bcpy(markb,markk);
		/* Leave one character in buffer to keep pointers set properly... */
		q = pdup(markk, "ulower");
		prgetc(q);
		bdel(markb,q);
		b->o.charmap = markb->b->o.charmap;
		p=pdup(b->bof, "ulower");
		while ((c=pgetc(p))!=NO_MORE_DATA) {
			c = joe_tolower(b->o.charmap,c);
			binsc(q,c);
			pgetc(q);
		}
		prm(p);
		bdel(q,markk);
		prm(q);
		brm(b);
		bw->cursor->xcol = piscol(bw->cursor);
		return 0;
	} else
		return -1;
}

/* Force region to upper case */

int uupper(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (markv(1)) {
		P *q;
	        P *p;
	        int c;
		B *b = bcpy(markb,markk);
		q = pdup(markk, "uupper");
		prgetc(q);
		bdel(markb,q);
		b->o.charmap = markb->b->o.charmap;
		p=pdup(b->bof, "uupper");
		while ((c=pgetc(p))!=NO_MORE_DATA) {
			c = joe_toupper(b->o.charmap,c);
			binsc(q,c);
			pgetc(q);
		}
		prm(p);
		bdel(q,markk);
		prm(q);
		brm(b);
		bw->cursor->xcol = piscol(bw->cursor);
		return 0;
	} else
		return -1;
}

/* Get sum, sum of squares, and return count of
 * a block of numbers.
 *
 * avg = sum/count
 *
 * stddev = sqrt(  (a-avg)^2 + (b-avg)^2 + (c-avg)^2 )
 *        = sqrt(  a^2-2*a*avg+avg^2  + b^2-2*b*avg+avg^2 +�c^2-2*c*avg+avg^2 )
 *        = sqrt(  a^2+b^2+c^2 + 3*avg^2 - 2*avg*(a+b+c) )
 *        = sqrt(  sumsq + count*avg^2 - 2*avg*sum  )
 *
 */

int blksum(double *sum, double *sumsq)
{
	char buf[80];
	if (markv(1)) {
		P *q = pdup(markb, "blksum");
		int x;
		int c;
		double accu = 0.0;
		double accusq = 0.0;
		double v;
		int count = 0;
		off_t left = markb->xcol;
		off_t right = markk->xcol;
		while (q->byte < markk->byte) {
			/* Skip until we're within columns */
			while (q->byte < markk->byte && square && (piscol(q) < left || piscol(q) >= right))
				pgetc(q);

			/* Skip to first number */
			while (q->byte < markk->byte && (!square || (piscol(q) >= left && piscol(q) < right))) {
				c=pgetc(q);
				if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
					/* Copy number into buffer */
					buf[0] = TO_CHAR_OK(c); x=1;
					while (q->byte < markk->byte && (!square || (piscol(q) >= left && piscol(q) < right))) {
						c=pgetc(q);
						if ((c >= '0' && c <= '9') || c == 'e' || c == 'E' ||
						    c == 'p' || c == 'P' || c == 'x' || c == 'X' ||
						    c == '.' || c == '-' || c == '+' ||
						    (c >= 'a' && c <= 'f') || (c >= 'A' && c<='F')) {
							if(x != 79)
								buf[x++]= TO_CHAR_OK(c);
						} else
							break;
					}
					/* Convert number to floating point, add it to total */
					buf[x] = 0;
					v = strtod(buf,NULL);
					++count;
					accu += v;
					accusq += v*v;
					break;
				}
			}
		}
		prm(q);
		*sum = accu;
		*sumsq = accusq;
		return count;
	} else
		return -1;
}

/* Get a (possibly square) block into a buffer
 * Block is converted to UTF-8
 */

char *blkget()
{
	if (markv(1)) {
		P *q;
		ptrdiff_t buf_size = markk->byte - markb->byte + 1; /* Risky... */
		ptrdiff_t buf_x = 0;
		char *buf = (char *)joe_malloc(buf_size);
		off_t left = markb->xcol;
		off_t right = markk->xcol;
		q = pdup(markb, "blkget");
		while (q->byte < markk->byte) {
			/* Skip until we're within columns */
			while (q->byte < markk->byte && square && (piscol(q) < left || piscol(q) >= right))
				pgetc(q);

			/* Copy text into buffer */
			while (q->byte < markk->byte && (!square || (piscol(q) >= left && piscol(q) < right))) {
				int ch = pgetc(q);
				char bf[8];
				ptrdiff_t len, x;
				if (!q->b->o.charmap->type)
					ch = to_uni(q->b->o.charmap, ch);
				len = utf8_encode(bf, ch);
				for (x = 0; x != len; ++x) {
					if (buf_x == buf_size - 1) {
						buf_size *= 2;
						buf = (char *)joe_realloc(buf, buf_size);
					}
					buf[buf_x++] = bf[x];
				}
			}
			/* Add a new line if we went past right edge of column */
			if (square && q->byte<markk->byte && piscol(q) >= right) {
				if (buf_x == buf_size - 1) {
					buf_size *= 2;
					buf = (char *)joe_realloc(buf, buf_size);
				}
				buf[buf_x++] = '\n';
			}
		}
		prm(q);
		buf[buf_x] = 0;
		return buf;
	} else
		return 0;
}
