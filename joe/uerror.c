/*
 *	Compiler error handler
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

/* Error database */

typedef struct error ERROR;

struct error {
	LINK(ERROR) link;	/* Linked list of errors */
	off_t line;		/* Target line number */
	off_t org;		/* Original target line number */
	char *file;		/* Target file name */
	off_t src;		/* Error-file line number */
	char *msg;	/* The message */
} errors = { { &errors, &errors} };
ERROR *errptr = &errors;	/* Current error row */

B *errbuf = NULL;		/* Buffer with error messages */

/* Function which allows stepping through all error buffers,
   for multi-file search and replace.  Give it a buffer.  It finds next
   buffer in error list.  Look at 'berror' for error information. */

/* This is made to work like bafter: it does not increment refcount of buffer */

B *beafter(B *b)
{
	struct error *e;
	const char *name = b->name;
	if (!name) name = "";
	for (e = errors.link.next; e != &errors; e = e->link.next)
		if (!zcmp(name, e->file))
			break;
	if (e == &errors) {
		/* Given buffer is not in list?  Return first buffer in list. */
		e = errors.link.next;
	}
	while (e != &errors && !zcmp(name, e->file))
		e = e->link.next;
	berror = 0;
	if (e != &errors) {
		b = bfind(e->file);
		/* bfind bumps refcount, so we have to unbump it */
		if (b->count == 1)
			b->orphan = 1; /* Oops */
		else
			--b->count;
		return b;
	}
	return 0;
}

/* Insert and delete notices */

void inserr(const char *name, off_t where, off_t n, int bol)
{
	ERROR *e;

	if (!n)
		return;

	if (name) {
		for (e = errors.link.next; e != &errors; e = e->link.next) {
			if (!zcmp(e->file, name)) {
				if (e->line > where)
					e->line += n;
				else if (e->line == where && bol)
					e->line += n;
			}
		}
	}
}

void delerr(const char *name, off_t where, off_t n)
{
	ERROR *e;

	if (!n)
		return;

	if (name) {
		for (e = errors.link.next; e != &errors; e = e->link.next) {
			if (!zcmp(e->file, name)) {
				if (e->line > where + n)
					e->line -= n;
				else if (e->line > where)
					e->line = where;
			}
		}
	}
}

/* Abort notice */

void abrerr(const char *name)
{
	ERROR *e;

	if (name)
		for (e = errors.link.next; e != &errors; e = e->link.next)
			if (!zcmp(e->file, name))
				e->line = e->org;
}

/* Save notice */

void saverr(const char *name)
{
	ERROR *e;

	if (name)
		for (e = errors.link.next; e != &errors; e = e->link.next)
			if (!zcmp(e->file, name))
				e->org = e->line;
}

/* Pool of free error nodes */
ERROR errnodes = { {&errnodes, &errnodes} };

/* Free an error node */

static void freeerr(ERROR *n)
{
	vsrm(n->file);
	vsrm(n->msg);
	enquef(ERROR, link, &errnodes, n);
}

/* Free all errors */

static int freeall(void)
{
	int count = 0;
	while (!qempty(ERROR, link, &errors)) {
		freeerr(deque_f(ERROR, link, errors.link.next));
		++count;
	}
	errptr = &errors;
	return count;
}

/* Parse "make[1]: Entering directory '/home/..../foo'" message. */

static void parsedir(const char *s, char **rtn_dir)
{
	const char *u, *v;
	u = zstr(s, "Entering directory `");
	if (u) {
		u += zlen("Entering directory `");
		for (v = u; *v && *v != '\''; ++v);
		if (*v == '\'') {
			char *t = *rtn_dir;
			t = vstrunc(t, 0);
			t = vsncpy(sv(t), u, v - u);
			if (sLEN(t) && t[sLEN(t)-1] != '/')
				t = vsadd(t, '/');
			*rtn_dir = t;
		}
	}
}

/* Parse error messages into database */

/* From joe's joe 2.9 */

/* First word (allowing ., /, _ and -) with a . is the file name.  Next number
   is line number.  Then there should be a ':' */

static void parseone(struct charmap *map,const char *s,char **rtn_name,off_t *rtn_line)
{
	int flg;
	int c;
	char *name = NULL;
	off_t line = -1;
	const char *u, *v, *t;

	v = s;
	flg = 0;

	if (s[0] == 'J' && s[1] == 'O' && s[2] == 'E' && s[3] == ':')
		goto bye;

	do {
		
		/* Skip to first word */
		for (u = v; *u && !((t = u), (c = fwrd_c(map, &t, NULL)), ((c >= 0 && joe_isalnum_(map, c)) || c == '.' || c == '/')); u = t) ;

		/* Skip to end of first word */
		for (v = u; (t = v), (c = fwrd_c(map, &t, NULL)), ((c >= 0 && joe_isalnum_(map, c)) || c == '.' || c == '/' || c == '-'); v = t)
			if (c == '.')
				flg = 1;
	} while (!flg && u != v);

	/* Save file name */
	if (u != v)
		name = vsncpy(NULL, 0, u, v - u);

	/* Skip to first number */
	for (u = v; *u && (*u < '0' || *u > '9'); ++u) ;

	/* Skip to end of first number */
	for (v = u; *v >= '0' && *v <= '9'; ++v) ;

	/* Save line number */
	if (u != v) {
		line = ztoo(u);
	}
	if (line != -1)
		--line;

	/* Look for ':' */
	flg = 0;
	while (*v) {
	/* Allow : anywhere on line: works for MIPS C compiler */
/*
	for (y = 0; s[y];)
*/
		if (*v == ':') {
			flg = 1;
			break;
		}
		++v;
	}

	bye:

	if (!flg)
		line = -1;

	*rtn_name = name;
	*rtn_line = line;
}

/* Parser for file name lists from grep, find and ls.
 *
 * filename
 * filename:*
 * filename:line-number:*
 */

void parseone_grep(struct charmap *map,const char *s,char **rtn_name,off_t *rtn_line)
{
	int y;
	char *name = NULL;
	off_t line = -1;

	if (s[0] == 'J' && s[1] == 'O' && s[2] == 'E' && s[3] == ':')
		goto bye;

	/* Skip to first : or end of line */
	for (y = 0;s[y] && s[y] != ':';++y);
	if (y) {
		/* This should be the file name */
		name = vsncpy(NULL,0,s,y);
		line = 0;
		if (s[y] == ':') {
			/* Maybe there's a line number */
			++y;
			while (s[y] >= '0' && s[y] <= '9')
				line = line * 10 + (s[y++] - '0');
			--line;
			if (line < 0 || s[y] != ':') {
				/* Line number is only valid if there's a second : */
				line = 0;
			}
		}
	}

	bye:

	*rtn_name = name;
	*rtn_line = line;
}

static int parseit(struct charmap *map,const char *s, off_t row,
  void (*parseline)(struct charmap *map, const char *s, char **rtn_name, off_t *rtn_line), char *current_dir)
{
	char *name = NULL;
	off_t line = -1;
	ERROR *err;

	parseline(map,s,&name,&line);

	if (name) {
		if (line != -1) {
			/* We have an error */
			err = (ERROR *) alitem(&errnodes, SIZEOF(ERROR));
			err->file = name;
			if (current_dir) {
				err->file = vsncpy(NULL, 0, sv(current_dir));
				err->file = vsncpy(sv(err->file), sv(name));
				err->file = canonical(err->file);
				vsrm(name);
			} else {
				err->file = name;
			}
			err->org = err->line = line;
			err->src = row;
			err->msg = vsncpy(NULL, 0, sc("\\i"));
			err->msg = vsncpy(sv(err->msg), sv(s));
			enqueb(ERROR, link, &errors, err);
			return 1;
		} else
			vsrm(name);
	}
	return 0;
}

/* Parse the error output contained in a buffer */

void kill_ansi(char *s);

static off_t parserr(B *b)
{
	if (markv(1)) {
		char *curdir = 0;
		P *p = pdup(markb, "parserr1");
		P *q = pdup(markb, "parserr2");
		off_t nerrs = 0;
		errbuf = markb->b;

		freeall();

		p_goto_bol(p);

		do {
			char *s;

			pset(q, p);
			p_goto_eol(p);
			s = brvs(q, p->byte - q->byte);
			if (s) {
				kill_ansi(s);
				parsedir(s, &curdir);
				nerrs += parseit(q->b->o.charmap, s, q->line, (q->b->parseone ? q->b->parseone : parseone), (curdir ? curdir : q->b->current_dir));
				vsrm(s);
			}
			pgetc(p);
		} while (p->byte < markk->byte);
		vsrm(curdir);
		prm(p);
		prm(q);
		return nerrs;
	} else {
		char *curdir = 0;
		P *p = pdup(b->bof, "parserr3");
		P *q = pdup(p, "parserr4");
		off_t nerrs = 0;
		errbuf = b;

		freeall();
		do {
			char *s;

			pset(q, p);
			p_goto_eol(p);
			s = brvs(q, p->byte - q->byte);
			if (s) {
				kill_ansi(s);
				parsedir(s, &curdir);
				nerrs += parseit(q->b->o.charmap, s, q->line, (q->b->parseone ? q->b->parseone : parseone), (curdir ? curdir : q->b->current_dir));
				vsrm(s);
			}
		} while (pgetc(p) != NO_MORE_DATA);
		vsrm(curdir);
		prm(p);
		prm(q);
		return nerrs;
	}
}

static BW *find_a_good_bw(B *b)
{
	W *w;
	BW *bw = 0;
	/* Find lowest window with buffer */
	if ((w = maint->topwin) != NULL) {
		do {
			if ((w->watom->what&TYPETW) && ((BW *)w->object)->b==b && w->y>=0)
				bw = (BW *)w->object;
			w = w->link.next;
		} while (w != maint->topwin);
	}
	if (bw)
		return bw;
	/* Otherwise just find lowest window */
	if ((w = maint->topwin) != NULL) {
		do {
			if ((w->watom->what&TYPETW) && w->y>=0)
				bw = (BW *)w->object;
			w = w->link.next;
		} while (w != maint->topwin);
	}
	return bw;
}

int parserrb(B *b)
{
	BW *bw;
	off_t n;
	freeall();
	bw = find_a_good_bw(b);
	unmark(bw->parent, 0);
	n = parserr(b);
	if (n)
		joe_snprintf_1(msgbuf, JOE_MSGBUFSIZE, joe_gettext(_("%d messages found")), (int)n);
	else
		joe_snprintf_0(msgbuf, SIZEOF(msgbuf), joe_gettext(_("No messages found")));
	msgnw(bw->parent, msgbuf);
	return 0;
}

int urelease(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	bw->b->parseone = 0;
	if (qempty(ERROR, link, &errors) && !errbuf) {
		joe_snprintf_0(msgbuf, SIZEOF(msgbuf), joe_gettext(_("No messages")));
	} else {
		int count = freeall();
		errbuf = NULL;
		joe_snprintf_1(msgbuf, SIZEOF(msgbuf), joe_gettext(_("%d messages cleared")), count);
	}
	msgnw(bw->parent, msgbuf);
	updall();
	return 0;
}

int uparserr(W *w, int k)
{
	off_t n;
	BW *bw;
	WIND_BW(bw, w);
	freeall();
	bw->b->parseone = parseone;
	n = parserr(bw->b);
	if (n)
		joe_snprintf_1(msgbuf, JOE_MSGBUFSIZE, joe_gettext(_("%d messages found")), (int)n);
	else
		joe_snprintf_0(msgbuf, SIZEOF(msgbuf), joe_gettext(_("No messages found")));
	msgnw(bw->parent, msgbuf);
	return 0;
}

int ugparse(W *w, int k)
{
	off_t n;
	BW *bw;
	WIND_BW(bw, w);
	freeall();
	bw->b->parseone = parseone_grep;
	n = parserr(bw->b);
	if (n)
		joe_snprintf_1(msgbuf, JOE_MSGBUFSIZE, joe_gettext(_("%d messages found")), (int)n);
	else
		joe_snprintf_0(msgbuf, SIZEOF(msgbuf), joe_gettext(_("No messages found")));
	msgnw(bw->parent, msgbuf);
	return 0;
}

static int jump_to_file_line(BW *bw,char *file,off_t line,char *msg)
{
	int omid;
	if (!bw->b->name || zcmp(file, bw->b->name)) {
		if (doswitch(bw->parent, vsdup(file), NULL, NULL))
			return -1;
		bw = (BW *) maint->curwin->object;
	}
	omid = opt_mid;
	opt_mid = 1;
	pline(bw->cursor, line);
	dofollows();
	opt_mid = omid;
	bw->cursor->xcol = piscol(bw->cursor);
	msgnw(bw->parent, msg);
	return 0;
}

/* Show current message */

int ucurrent_msg(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (errptr != &errors) {
		msgnw(bw->parent, errptr->msg);
		return 0;
	} else {
		msgnw(bw->parent, joe_gettext(_("No messages")));
		return -1;
	}
}

/* Find line in error database: return pointer to message */

static ERROR *srcherr(BW *bw,char *file,off_t line)
{
	ERROR *p;
	for (p = errors.link.next; p != &errors; p=p->link.next)
		if (!zcmp(p->file,file) && p->org == line) {
			errptr = p;
			setline(errbuf, errptr->src);
			return errptr;
			}
	return 0;
}

/* Delete ansi formatting */

void kill_ansi(char *s)
{
	char *d = s;
	while (*s)
		if (*s == 27) {
			while (*s && (*s == 27 || *s == '[' || (*s >= '0' && *s <= '9') || *s == ';'))
				++s;
			if (*s)
				++s;
		} else
			*d++ = *s++;
	*d = 0;
}

int ujump(W *w, int k)
{
	char *curdir = 0;
	BW *bw;
	int rtn = -1;
	P *p;
	P *q;
	char *s;
	WIND_BW(bw, w);
	p = pdup(bw->b->bof, "ujump");
	q = pdup(p, "ujump");

	/* Parse entire file just to get current directory */
	while (p->line != bw->cursor->line) {
		pset(q, p);
		p_goto_eol(p);
		s = brvs(q, p->byte - q->byte);
		if (s) {
			kill_ansi(s);
			parsedir(s, &curdir);
			vsrm(s);
		}
		if (NO_MORE_DATA == pgetc(p))
			break;
	}

	/* Parse the line with the cursor, look for an error */
	pset(p, bw->cursor);
	pset(q, bw->cursor);
	p_goto_bol(p);
	p_goto_eol(q);
	s = brvs(p, q->byte - p->byte);
	prm(p);
	prm(q);
	if (s) {
		char *name = NULL;
		char *fullname = NULL;
		char *curd = get_cd(bw->parent);
		off_t line = -1;
		kill_ansi(s);
		if (bw->b->parseone)
			bw->b->parseone(bw->b->o.charmap,s,&name,&line);
		else
			parseone_grep(bw->b->o.charmap,s,&name,&line);
		/* Prepend current directory.. */
		if (curdir)
			fullname = vsncpy(NULL, 0, sv(curdir));
		else
			fullname = vsncpy(NULL, 0, sv(curd));
		fullname = vsncpy(sv(fullname), sv(name));
		vsrm(name);
		name = canonical(fullname);
		if (name && line != -1) {
			ERROR *er = srcherr(bw, name, line);
			uprevw(bw->parent, 0);
			/* Check that we made it to a tw */
			if (er)
				rtn = jump_to_file_line((BW *)maint->curwin->object,name,er->line,NULL /* p->msg */);
			else
				rtn = jump_to_file_line((BW *)maint->curwin->object,name,line,NULL);
			vsrm(name);
		}
		vsrm(s);
	}
	vsrm(curdir);
	return rtn;
}

int unxterr(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (errptr->link.next == &errors) {
		msgnw(bw->parent, joe_gettext(_("No more errors")));
		return -1;
	}
	errptr = errptr->link.next;
	setline(errbuf, errptr->src);
	return jump_to_file_line(bw,errptr->file,errptr->line,NULL /* errptr->msg */);
}

int uprverr(W *w, int k)
{
	BW *bw;
	WIND_BW(bw, w);
	if (errptr->link.prev == &errors) {
		msgnw(bw->parent, joe_gettext(_("No more errors")));
		return -1;
	}
	errptr = errptr->link.prev;
	setline(errbuf, errptr->src);
	return jump_to_file_line(bw,errptr->file,errptr->line,NULL /* errptr->msg */);
}
