/*
 * Amanda, The Advanced Maryland Automatic Network Disk Archiver
 * Copyright (c) 1991-1998 University of Maryland at College Park
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of U.M. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  U.M. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * U.M. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL U.M.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: James da Silva, Systems Design and Analysis Group
 *			   Computer Science Department
 *			   University of Maryland at College Park
 */
/*
 * $Id: amlogroll.c,v 1.14 2006/07/25 18:27:57 martinea Exp $
 *
 * rename a live log file to the datestamped name.
 */

#include "amanda.h"
#include "conffile.h"
#include "logfile.h"
#include "version.h"

char *datestamp;

void handle_start(void);
int main(int argc, char **argv);

int
main(
    int		argc,
    char **	argv)
{
    char *conffile;
    char *logfname;
    char *conf_logdir;
    FILE *logfile;
    char * cwd;
    int    new_argc,   my_argc;
    char **new_argv, **my_argv;

    /*
     * Configure program for internationalization:
     *   1) Only set the message locale for now.
     *   2) Set textdomain for all amanda related programs to "amanda"
     *      We don't want to be forced to support dozens of message catalogs.
     */  
    setlocale(LC_MESSAGES, "C");
    textdomain("amanda"); 

    safe_fd(-1, 0);

    set_pname("amlogroll");

    dbopen(DBG_SUBDIR_SERVER);

    /* Process options */
    
    erroutput_type = ERR_INTERACTIVE;

    cwd = g_get_current_dir();
    if (cwd == NULL) {
	error(_("Cannot determine current working directory: %s"),
	        strerror(errno));
        g_assert_not_reached();
    }

    parse_conf(argc, argv, &new_argc, &new_argv);
    my_argc = new_argc;
    my_argv = new_argv;

    if (my_argc < 2) {
	config_dir = stralloc2(cwd, "/");
	if ((config_name = strrchr(cwd, '/')) != NULL) {
	    config_name = stralloc(config_name + 1);
	}
    } else {
	config_name = stralloc(my_argv[1]);
	config_dir = vstralloc(CONFIG_DIR, "/", config_name, "/", NULL);
    }

    amfree(cwd);
    safe_cd();

    /* read configuration files */

    conffile = stralloc2(config_dir, CONFFILE_NAME);
    if(read_conffile(conffile)) {
        error(_("errors processing config file \"%s\""), conffile);
	/*NOTREACHED*/
    }
    amfree(conffile);

    check_running_as(RUNNING_AS_DUMPUSER);

    dbrename(config_name, DBG_SUBDIR_SERVER);

    report_bad_conf_arg();

    conf_logdir = getconf_str(CNF_LOGDIR);
    if (*conf_logdir == '/') {
        conf_logdir = stralloc(conf_logdir);
    } else {
        conf_logdir = stralloc2(config_dir, conf_logdir);
    }
    logfname = vstralloc(conf_logdir, "/", "log", NULL);
    amfree(conf_logdir);

    if((logfile = fopen(logfname, "r")) == NULL) {
	error(_("could not open log %s: %s"), logfname, strerror(errno));
	/*NOTREACHED*/
    }
    amfree(logfname);

    erroutput_type |= ERR_AMANDALOG;
    set_logerror(logerror);

    while(get_logline(logfile)) {
	if(curlog == L_START) {
	    handle_start();
	    if(datestamp != NULL) {
		break;
	    }
	}
    }
    afclose(logfile);
 
    log_rename(datestamp);

    amfree(datestamp);
    amfree(config_dir);
    amfree(config_name);
    free_new_argv(new_argc, new_argv);
    free_server_config();

    dbclose();

    return 0;
}

void handle_start(void)
{
    static int started = 0;
    char *s, *fp;
    int ch;

    if(!started) {
	s = curstr;
	ch = *s++;

	skip_whitespace(s, ch);
	if(ch == '\0' || strncmp_const_skip(s - 1, "date", s, ch) != 0) {
	    return;				/* ignore bogus line */
	}

	skip_whitespace(s, ch);
	if(ch == '\0') {
	    return;
	}
	fp = s - 1;
	skip_non_whitespace(s, ch);
	s[-1] = '\0';
	datestamp = newstralloc(datestamp, fp);
	s[-1] = (char)ch;

	started = 1;
    }
}
