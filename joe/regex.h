/*
 *	Regular expression subroutines
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#ifndef _JOE_REGEX_H
#define _JOE_REGEX_H 1

int pmatch PARAMS((unsigned char **pieces, unsigned char *regex, int len, P *p, int n, int icase));

#endif
