/*
 * pcre.c - interface to the PCRE library
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 2001, 2002, 2003, 2004, 2007 Clint Adams
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Clint Adams or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Andrew Main and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Clint Adams and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Andrew Main and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */


#include "pcre.mdh"
#include "pcre.pro"

#define CPCRE_PLAIN 0

/**/
#if defined(HAVE_PCRE_COMPILE) && defined(HAVE_PCRE_EXEC)
#include <pcre.h>

static pcre *pcre_pattern;
static pcre_extra *pcre_hints;

/**/
static int
zpcre_utf8_enabled(void)
{
#if defined(MULTIBYTE_SUPPORT) && defined(HAVE_NL_LANGINFO) && defined(CODESET)
    static int have_utf8_pcre = -1;

    /* value can toggle based on MULTIBYTE, so don't
     * be too eager with caching */
    if (have_utf8_pcre < -1)
	return 0;

    if (!isset(MULTIBYTE))
	return 0;

    if ((have_utf8_pcre == -1) &&
        (!strcmp(nl_langinfo(CODESET), "UTF-8"))) {

	if (pcre_config(PCRE_CONFIG_UTF8, &have_utf8_pcre))
	    have_utf8_pcre = -2; /* erk, failed to ask */
    }

    if (have_utf8_pcre < 0)
	return 0;
    return have_utf8_pcre;

#else
    return 0;
#endif
}

/**/
static int
bin_pcre_compile(char *nam, char **args, Options ops, UNUSED(int func))
{
    int pcre_opts = 0, pcre_errptr;
    const char *pcre_error;
    
    if(OPT_ISSET(ops,'a')) pcre_opts |= PCRE_ANCHORED;
    if(OPT_ISSET(ops,'i')) pcre_opts |= PCRE_CASELESS;
    if(OPT_ISSET(ops,'m')) pcre_opts |= PCRE_MULTILINE;
    if(OPT_ISSET(ops,'x')) pcre_opts |= PCRE_EXTENDED;
    
    if (zpcre_utf8_enabled())
	pcre_opts |= PCRE_UTF8;

    pcre_hints = NULL;  /* Is this necessary? */
    
    if (pcre_pattern)
	pcre_free(pcre_pattern);

    pcre_pattern = pcre_compile(*args, pcre_opts, &pcre_error, &pcre_errptr, NULL);
    
    if (pcre_pattern == NULL)
    {
	zwarnnam(nam, "error in regex: %s", pcre_error);
	return 1;
    }
    
    return 0;
}

/**/
#ifdef HAVE_PCRE_STUDY

/**/
static int
bin_pcre_study(char *nam, UNUSED(char **args), UNUSED(Options ops), UNUSED(int func))
{
    const char *pcre_error;

    if (pcre_pattern == NULL)
    {
	zwarnnam(nam, "no pattern has been compiled for study");
	return 1;
    }
    
    pcre_hints = pcre_study(pcre_pattern, 0, &pcre_error);
    if (pcre_error != NULL)
    {
	zwarnnam(nam, "error while studying regex: %s", pcre_error);
	return 1;
    }
    
    return 0;
}

/**/
#else /* !HAVE_PCRE_STUDY */

# define bin_pcre_study bin_notavail

/**/
#endif /* !HAVE_PCRE_STUDY */

/**/
static int
zpcre_get_substrings(char *arg, int *ovec, int ret, char *matchvar, char *substravar, int matchedinarr)
{
    char **captures, **match_all, **matches;
    int capture_start = 1;

    if (matchedinarr)
	capture_start = 0;
    if (matchvar == NULL)
	matchvar = "MATCH";
    if (substravar == NULL)
	substravar = "match";

    /* captures[0] will be entire matched string, [1] first substring */
    if(!pcre_get_substring_list(arg, ovec, ret, (const char ***)&captures)) {
	match_all = ztrdup(captures[0]);
	setsparam(matchvar, match_all);
	matches = zarrdup(&captures[capture_start]);
	setaparam(substravar, matches);
	pcre_free_substring_list((const char **)captures);
    }

    return 0;
}

/**/
static int
bin_pcre_match(char *nam, char **args, Options ops, UNUSED(int func))
{
    int ret, capcount, *ovec, ovecsize, c;
    char *matched_portion = NULL;
    char *receptacle = NULL;
    int return_value = 1;

    if (pcre_pattern == NULL) {
	zwarnnam(nam, "no pattern has been compiled");
	return 1;
    }
    
    if(OPT_HASARG(ops,c='a')) {
	receptacle = OPT_ARG(ops,c);
    }
    if(OPT_HASARG(ops,c='v')) {
	matched_portion = OPT_ARG(ops,c);
    }
    if(!*args) {
	zwarnnam(nam, "not enough arguments");
    }
    
    if ((ret = pcre_fullinfo(pcre_pattern, pcre_hints, PCRE_INFO_CAPTURECOUNT, &capcount)))
    {
	zwarnnam(nam, "error %d in fullinfo", ret);
	return 1;
    }
    
    ovecsize = (capcount+1)*3;
    ovec = zalloc(ovecsize*sizeof(int));
    
    ret = pcre_exec(pcre_pattern, pcre_hints, *args, strlen(*args), 0, 0, ovec, ovecsize);
    
    if (ret==0) return_value = 0;
    else if (ret==PCRE_ERROR_NOMATCH) /* no match */;
    else if (ret>0) {
	zpcre_get_substrings(*args, ovec, ret, matched_portion, receptacle, 0);
	return_value = 0;
    }
    else {
	zwarnnam(nam, "error in pcre_exec");
    }
    
    if (ovec)
	zfree(ovec, ovecsize*sizeof(int));

    return return_value;
}

/**/
static int
cond_pcre_match(char **a, int id)
{
    pcre *pcre_pat;
    const char *pcre_err;
    char *lhstr, *rhre, *avar=NULL;
    int r = 0, pcre_opts = 0, pcre_errptr, capcnt, *ov, ovsize;
    int return_value = 0;

    if (zpcre_utf8_enabled())
	pcre_opts |= PCRE_UTF8;

    lhstr = cond_str(a,0,0);
    rhre = cond_str(a,1,0);
    pcre_pat = ov = NULL;

    if (isset(BASHREMATCH))
	avar="BASH_REMATCH";

    switch(id) {
	 case CPCRE_PLAIN:
		pcre_pat = pcre_compile(rhre, pcre_opts, &pcre_err, &pcre_errptr, NULL);
		if (pcre_pat == NULL) {
		    zwarn("failed to compile regexp /%s/: %s", rhre, pcre_err);
		    break;
		}
                pcre_fullinfo(pcre_pat, NULL, PCRE_INFO_CAPTURECOUNT, &capcnt);
    		ovsize = (capcnt+1)*3;
		ov = zalloc(ovsize*sizeof(int));
    		r = pcre_exec(pcre_pat, NULL, lhstr, strlen(lhstr), 0, 0, ov, ovsize);
		/* r < 0 => error; r==0 match but not enough size in ov
		 * r > 0 => (r-1) substrings found; r==1 => no substrings
		 */
    		if (r==0) {
		    zwarn("reportable zsh problem: pcre_exec() returned 0");
		    return_value = 1;
		    break;
		}
	        else if (r==PCRE_ERROR_NOMATCH) return 0; /* no match */
		else if (r<0) {
		    zwarn("pcre_exec() error: %d", r);
		    break;
		}
                else if (r>0) {
		    zpcre_get_substrings(lhstr, ov, r, NULL, avar, isset(BASHREMATCH));
		    return_value = 1;
		    break;
		}
		break;
    }

    if (pcre_pat)
	pcre_free(pcre_pat);
    if (ov)
	zfree(ov, ovsize*sizeof(int));

    return return_value;
}

static struct conddef cotab[] = {
    CONDDEF("pcre-match", CONDF_INFIX, cond_pcre_match, 0, 0, CPCRE_PLAIN)
    /* CONDDEF can register =~ but it won't be found */
};

/**/
#else /* !(HAVE_PCRE_COMPILE && HAVE_PCRE_EXEC) */

# define bin_pcre_compile bin_notavail
# define bin_pcre_study bin_notavail
# define bin_pcre_match bin_notavail

/**/
#endif /* !(HAVE_PCRE_COMPILE && HAVE_PCRE_EXEC) */

static struct builtin bintab[] = {
    BUILTIN("pcre_compile", 0, bin_pcre_compile, 1, 1, 0, "aimx",  NULL),
    BUILTIN("pcre_study",   0, bin_pcre_study,   0, 0, 0, NULL,    NULL),
    BUILTIN("pcre_match",   0, bin_pcre_match,   1, 1, 0, "a:v:",    NULL)
};


/**/
int
setup_(UNUSED(Module m))
{
    return 0;
}

/**/
int
boot_(Module m)
{
#if defined(HAVE_PCRE_COMPILE) && defined(HAVE_PCRE_EXEC)
    return !addbuiltins(m->nam, bintab, sizeof(bintab)/sizeof(*bintab)) ||
	   !addconddefs(m->nam, cotab, sizeof(cotab)/sizeof(*cotab));
#else /* !(HAVE_PCRE_COMPILE && HAVE_PCRE_EXEC) */
    return !addbuiltins(m->nam, bintab, sizeof(bintab)/sizeof(*bintab));
#endif /* !(HAVE_PCRE_COMPILE && HAVE_PCRE_EXEC) */
}

/**/
int
cleanup_(Module m)
{
    deletebuiltins(m->nam, bintab, sizeof(bintab)/sizeof(*bintab));
#if defined(HAVE_PCRE_COMPILE) && defined(HAVE_PCRE_EXEC)
    deleteconddefs(m->nam, cotab, sizeof(cotab)/sizeof(*cotab));
#endif /* !(HAVE_PCRE_COMPILE && HAVE_PCRE_EXEC) */
    return 0;
}

/**/
int
finish_(UNUSED(Module m))
{
    return 0;
}
