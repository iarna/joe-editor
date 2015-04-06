/*
 *	Shell-window functions
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

/* Executed when shell process terminates */

static void cdone(B *b)
{
	b->pid = 0;
	close(b->out);
	b->out = -1;
	if (b->vt) {
		vtrm(b->vt);
		b->vt = 0;
	}
}

static void cdone_parse(B *b)
{
	b->pid = 0;
	close(b->out);
	b->out = -1;
	if (b->vt) {
		vtrm(b->vt);
		b->vt = 0;
	}
	parserrb(b);
}

/* Set up for shell mode in each window with the buffer */

static void ansiall(B *b)
{
	W *w;
	if ((w = maint->topwin) != NULL) {
		do {
	 		if (w->watom->what & TYPETW) {
	 			BW *bw = (BW *)w->object;
		 		if (bw->b == b) {
		 			bw->o.ansi = bw->b->o.ansi;
		 			bw->o.syntax = bw->b->o.syntax;
		 		}
			}
		w = w->link.next;
	 	} while (w != maint->topwin);
	 }
}

/* Executed for each chunk of data we get from the shell */

/* Mark each window which needs to follow the shell output */

static void cready(B *b, off_t byte)
{
	W *w;
	if (b->oldcur && b->oldcur->byte == byte)
		b->shell_flag = 1;
	else
		b->shell_flag = 0;
	if ((w = maint->topwin) != NULL) {
	 	do {
	 		if (w->watom->what & TYPETW) {
	 			BW *bw = (BW *)w->object;
		 		if (bw->b == b && bw->cursor->byte == byte) {
		 			bw->shell_flag = 1;
		 		} else {
		 			bw->shell_flag = 0;
		 		}
			}
		w = w->link.next;
	 	} while (w != maint->topwin);
	}
}

static void cfollow(B *b, VT *vt, off_t byte)
{
	W *w;
	int xn;
	if (vt && piscol(vt->vtcur) >= vt->width)
		xn = 1; /* xn glitch: cursor is past end of line */
	else
		xn = 0;
	if (b->oldcur && b->shell_flag) {
		b->shell_flag = 0;
		pgoto(b->oldcur, byte);
		b->oldcur->xcol = piscol(b->oldcur);
		if (xn)
			b->oldcur->xcol = vt->width - 1;
	}
	if ((w = maint->topwin) != NULL) {
	 	do {
	 		if (w->watom->what & TYPETW) {
	 			BW *bw = (BW *)w->object;
	 			if (bw->shell_flag) {
	 				bw->shell_flag = 0;
	 				pgoto(bw->cursor, byte);
	 				bw->cursor->xcol = piscol(bw->cursor);
					if (xn)
						bw->cursor->xcol = vt->width - 1;
	 				if (vt && bw->top->line != vt->top->line && (1 + vt->vtcur->line - vt->top->line) <= bw->h)
	 					pline(bw->top, vt->top->line);
	 				dofollows();
	 			}
			}
		w = w->link.next;
	 	} while (w != maint->topwin);
	}
}

void vt_scrdn()
{
	W *w;
	 if ((w = maint->topwin) != NULL) {
	 	do {
	 		if (w->watom->what & TYPETW) {
	 			BW *bw = (BW *)w->object;
	 			if (bw->shell_flag) {
	 				pnextl(bw->top);
	 				if (bw->parent->y != -1)
						nscrlup(bw->parent->t->t, bw->y, bw->y + bw->h, 1);
	 			}
			}
		w = w->link.next;
	 	} while (w != maint->topwin);
	 }
}

static void cdata(B *b, unsigned char *dat, int siz)
{
	if (b->vt) { /* ANSI terminal emulator */
		MACRO *m;
		do {
			cready(b, b->vt->vtcur->byte);

			m = vt_data(b->vt, &dat, &siz);

			cfollow(b, b->vt, b->vt->vtcur->byte);
			undomark();
			if (m) {
				/* only do this if cursor is on window */
				if ((maint->curwin->watom->what & TYPETW) && ((BW *)maint->curwin->object)->b == b) {
					exmacro(m, 1);
					edupd(1);
				}
				rmmacro(m);
			}
		} while (m);
	} else { /* Dumb terminal */
		P *q = pdup(b->eof, USTR "cdata");
		P *r = pdup(b->eof, USTR "cdata");
		off_t byte = q->byte;
		unsigned char bf[1024];
		int x, y;
		cready(b, byte);
		for (x = y = 0; x != siz; ++x) {
			if (dat[x] == 13 || dat[x] == 0) {
				;
			} else if (dat[x] == 8 || dat[x] == 127) {
				if (y) {
					--y;
				} else {
					pset(q, r);
					prgetc(q);
					bdel(q, r);
					--byte;
				}
			} else if (dat[x] == 7) {
				ttputc(7);
			} else {
				bf[y++] = dat[x];
			}
		}
		if (y) {
			binsm(r, bf, y);
		}
		prm(r);
		prm(q);
		cfollow(b, NULL, b->eof->byte);
		undomark();
	}
}

int cstart(BW *bw, unsigned char *name, unsigned char **s, void *obj, int build, int out_only, unsigned char *first_command, int vt)
{
#ifdef __MSDOS__
	varm(s);
	msgnw(bw->parent, joe_gettext(_("Sorry, no sub-processes in DOS (yet)")));
	return -1;
#else
	MPX *m;
	int shell_w = -1, shell_h = -1;


	if (bw->b->pid) {
		if (!vt) { /* Don't complain if shell already running.. makes F-key switching nicer */
			/* Keep old behavior for dumb terminal */
			msgnw(bw->parent, joe_gettext(_("Program already running in this window")));
		}
		varm(s);
		return -1;
	}

	if (vt) {
		BW *master = vtmaster(bw->parent->t, bw->b); /* In case of multiple BWs on one B, pick one to be the master */
		if (!master) master = bw; /* Should never happen */
		shell_w = master->w;
		shell_h = master->h;
		bw->b->vt = mkvt(bw->b, master->top, master->h, master->w);

		bw->b->o.ansi = 1;
		bw->b->o.syntax = load_syntax(USTR "ansi");

		/* Turn on shell mode for each window */
		ansiall(bw->b);
	}

	/* p_goto_eof(bw->cursor); */

	if (!(m = mpxmk(&bw->b->out, name, s, cdata, bw->b, build ? cdone_parse : cdone, bw->b, out_only, shell_w, shell_h))) {
		varm(s);
		msgnw(bw->parent, joe_gettext(_("No ptys available")));
		return -1;
	} else {
		bw->b->pid = m->pid;
		if (first_command)
			if (-1 == write(bw->b->out, first_command, zlen(first_command)))
				msgnw(bw->parent, joe_gettext(_("Write failed when writing first command to shell")));
	}
	return 0;
#endif
}

static int dobknd(BW *bw, int vt)
{
	unsigned char **a;
	unsigned char *s;
        unsigned char *sh;
	unsigned char start_sh[] = ". " JOERC "shell.sh\n";
	unsigned char start_csh[] = "source " JOERC "shell.csh\n";

        if (!modify_logic(bw,bw->b))
        	return -1;

        sh=(unsigned char *)getenv("SHELL");

        if (file_exists(sh) && zcmp(sh,USTR "/bin/sh")) goto ok;
        if (file_exists(sh=USTR "/bin/bash")) goto ok;
        if (file_exists(sh=USTR "/usr/bin/bash")) goto ok;
        if (file_exists(sh=USTR "/bin/sh")) goto ok;

        msgnw(bw->parent, joe_gettext(_("\"SHELL\" environment variable not defined or exported")));
        return -1;

        ok:
	a = vamk(3);
	vaperm(a);
	s = vsncpy(NULL, 0, sz(sh));
	a = vaadd(a, s);
	s = vsncpy(NULL, 0, sc("-i"));
	a = vaadd(a, s);
	return cstart(bw, sh, a, NULL, 0, 0, (vt ? (zstr(sh, USTR "csh") ? start_csh : start_sh) : NULL), vt);
}

/* Start ANSI shell */

int uvtbknd(BW *bw)
{
	if (kmap_empty(kmap_getcontext(USTR "vtshell"))) {
		msgnw(bw->parent, joe_gettext(_(":vtshell keymap is missing")));
		return -1;
	}
	return dobknd(bw, 1);
}

/* Start dumb shell */

int ubknd(BW *bw)
{
	if (kmap_empty(shell_kbd->topmap)) {
		msgnw(bw->parent, joe_gettext(_(":shell keymap is missing")));
		return -1;
	}
	return dobknd(bw, 0);
}

/* Run a program in a window */

B *runhist = NULL;

int urun(BW *bw)
{
	unsigned char *s = ask(bw->parent, joe_gettext(_("Program to run: ")), &runhist, USTR "Run",
	                       utypebw, locale_map, 0, 0, NULL);

	if (s) {
		unsigned char **a;
		unsigned char *cmd;

		if (!modify_logic(bw,bw->b))
			return -1;

		a = vamk(10);
		cmd = vsncpy(NULL, 0, sc("/bin/sh"));

		a = vaadd(a, cmd);
		cmd = vsncpy(NULL, 0, sc("-c"));
		a = vaadd(a, cmd);
		a = vaadd(a, s);
		return cstart(bw, USTR "/bin/sh", a, NULL, 0, 0, NULL, 0);
	} else {
		return -1;
	}
}

B *buildhist = NULL;

int ubuild(BW *bw)
{
	unsigned char *s;
	int prev = 0;
	if (buildhist) {
		s = joe_gettext(_("Build command: "));
		prev = 1;
	} else {
		s = joe_gettext(_("Enter build command (for example, 'make'): "));
	}
	/* "file prompt" was set for this... */
	s = ask(bw->parent, s, &buildhist, USTR "Run", utypebw, locale_map, 0, prev, NULL);
	if (s) {
		unsigned char **a = vamk(10);
		unsigned char *cmd = vsncpy(NULL, 0, sc("/bin/sh"));
		unsigned char *t = NULL;


		bw->b->o.ansi = 1;
		bw->b->o.syntax = load_syntax(USTR "ansi");
		/* Turn on shell mode for each window */
		ansiall(bw->b);

		a = vaadd(a, cmd);
		cmd = vsncpy(NULL, 0, sc("-c"));
		a = vaadd(a, cmd);
		if (bw->b->current_dir && bw->b->current_dir[0]) {
			// Change directory before we run
			t = vsncpy(sv(t), sc("cd '"));
			t = vsncpy(sv(t), sv(bw->b->current_dir));
			t = vsncpy(sv(t), sc("' && "));
		}
		t = vsncpy(sv(t), sc("echo \"\nJOE: cd `pwd`\n\" && if ("));
		t = vsncpy(sv(t), sv(s));
		t = vsncpy(sv(t), sc("); then echo \"\nJOE: [32mPASS[0m (exit status = $?)\n\"; else echo \"\nJOE: [31mFAIL[0m (exit status = $?)\n\"; fi"));
		s = t;
		a = vaadd(a, s);
		return cstart(bw, USTR "/bin/sh", a, NULL, 1, 0, NULL, 0);
	} else {
		return -1;
	}
}

B *grephist = NULL;

int ugrep(BW *bw)
{
	unsigned char *s;
	int prev = 0;
	if (grephist) {
		s = joe_gettext(_("Grep command: "));
		prev = 1;
	} else {
		s = joe_gettext(_("Enter grep command (for example, 'grep -n foo *.c'): "));
	}
	/* "file prompt" was set for this... */
	s = ask(bw->parent, s, &grephist, USTR "Run", utypebw, locale_map, 0, prev, NULL);
	if (s) {
		unsigned char **a = vamk(10);
		unsigned char *cmd = vsncpy(NULL, 0, sc("/bin/sh"));

		a = vaadd(a, cmd);
		cmd = vsncpy(NULL, 0, sc("-c"));
		a = vaadd(a, cmd);
		a = vaadd(a, s);
		return cstart(bw, USTR "/bin/sh", a, NULL, 1, 0, NULL, 0);
	} else {
		return -1;
	}
}

/* Kill program */

int ukillpid(BW *bw)
{
	if (bw->b->pid) {
		int c = query(bw->parent, sz(joe_gettext(_("Kill program (y,n,^C)?"))), 0);
		if (bw->b->pid && (c == YES_CODE || yncheck(yes_key, c)))
			kill(bw->b->pid, 1);
		return -1;
	} else {
		return 0;
	}
}
