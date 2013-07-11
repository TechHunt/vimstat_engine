#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef WIN32
#include "win.h"
#else
#include "unix.h"
#endif /* WIN32 */

struct vimstat_s {
    /* Common link format is http://vimeo.com/XXXXXXXX */
    char link[25+1];
    long nviews;
    long nlikes;
    long ncomments;
};

static void warn(char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

static void die(char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(-1);
}

char *program_name;

void usage(int status)
{
    if (status){
        printf("Try `--help' for more information.\n");
    } else {
        printf("Usage: %s [OPTIONS]... < url-file\n"
               "\n"
               "Possible options:\n"
               "  --html    output as html <table> row\n"
               "  --help    output this message and exit\n", program_name);
    }
    exit(status);
}

static char *vimstat_tools_installed()
{
    int ncmd;
    char *cmds[] = {
        "wget --version 1>" DEV_NULL " 2>" DEV_NULL,
        "grep --version 1>" DEV_NULL " 2>" DEV_NULL,
        NULL
    };
    char *errmsg[] = {
        "Wget not installed.\n",
        "Grep not installed.\n",
        NULL
    };

    for (ncmd=0; cmds[ncmd] != NULL; ncmd++ ){
        if ( system(cmds[ncmd]) ){
            return errmsg[ncmd];
        }
    }

    return NULL;
}

static FILE *vimstat_open_pipe(char *url)
{
    char *cmd_fmt = "wget -qO- %s 2>" DEV_NULL "|"
    "grep -i -e userplays -e userlikes -e usercomments";
    const size_t cmd_len = strlen(cmd_fmt) - 2 /*%s*/ + 25 /*URL len.*/;
    char cmd[cmd_len+1];
    static FILE *p;

    snprintf(cmd, cmd_len, cmd_fmt, url);
    p = popen(cmd, "r");

    return p;
}

static int vimstat_is_url_valid(char *url)
{
    return ( (strlen(url)==25) &&
             (strncmp(url, "http://vimeo.com/", 17)==0) &&
             (atol(url+17)>0) );
}

static int vimstat_is_obj_valid(struct vimstat_s *stat)
{
    /* Require text fields to contain something and
     * numeric fields to contain positive values.*/
    return stat->nviews >=0 &&
           stat->nlikes >=0 &&
           stat->ncomments >=0;
}

static void vimstat_print_text(struct vimstat_s *stat)
{
    printf("Link: %s\n", stat->link);
    printf("Views: %lu\n", stat->nviews);
    printf("Likes: %lu\n", stat->nlikes);
    printf("Comments: %lu\n", stat->ncomments);
    putc('\n', stdout);
}

static void vimstat_print_html(struct vimstat_s *stat)
{
    printf("<tr>");
    printf("<td><a href=\"%s\">%s</a></td>", stat->link, stat->link);
    printf("<td>%lu</td>", stat->nviews);
    printf("<td>%lu</td>", stat->nlikes);
    printf("<td>%lu</td>", stat->ncomments);
    printf("</tr>");
    putc('\n', stdout);
}

static struct vimstat_s *vimstat_stat_url(char *url)
{
    static struct vimstat_s tmp_stat;
    char buf[1024];
    FILE *pipe_in;
    char *substr;

    /* Remove newline from url. */
    if ((substr = strrchr(url, '\n'))){
        *substr = '\0';
    }

    if (!vimstat_is_url_valid(url)){
        warn("Invalid URL: %s\n", url);
        return NULL;
    }

    if ((pipe_in = vimstat_open_pipe(url)) == NULL){
        warn("Unable to stat URL: %s\n", url);
        return NULL;
    }

    strncpy(tmp_stat.link, url, 25);

    while (fgets(buf, sizeof(buf), pipe_in) != NULL) {
        /* Assume that each line contains one of
         * UserPlays, UserlLikes or UserComments keywords.
         */
         if ((substr = strstr(buf, "UserPlays:"))){
            tmp_stat.nviews = atol(substr+10);
        } else if ((substr = strstr(buf, "UserLikes:"))){
            tmp_stat.nlikes = atol(substr+10);
        } else if ((substr = strstr(buf, "UserComments:"))){
            tmp_stat.ncomments = atol(substr+13);
        }
    }
    pclose(pipe_in);

    /* If I get invalid values from URL, then
     * die or what?
     */
    if (!vimstat_is_obj_valid(&tmp_stat)){
        warn("Bad values parsed from URL: %s\n", url);
        return NULL;
    }

    return &tmp_stat;
}

int main(int argc, char *argv[])
{
    struct vimstat_s *stat;
    char buf[1024];
    char *error_msg;
    int narg;
    void (*vimstat_print)(struct vimstat_s *stat) = vimstat_print_text;

    program_name = argv[0];
    /* Check if wget, grep are installed. */
    if ( (error_msg = vimstat_tools_installed()) != NULL ){
        die(error_msg);
    }

    for (narg = 1; narg < argc; narg++){
        if (strcmp(argv[narg], "--help") == 0){
            usage(0);
        } else if (strcmp(argv[narg], "--html") == 0){
            vimstat_print = vimstat_print_html;
        } else {
            warn("Invalid agrument: %s\n", argv[narg]);
            usage(-1);
        }
    }

    /* Stdin for source of URLs */
    while ( fgets(buf, sizeof(buf), stdin) != NULL ){
        if ( buf[0] == '#' ||
             buf[0] == '\n' ){
            continue;
        } else {
            if ((stat = vimstat_stat_url(buf)) != NULL){
                vimstat_print(stat);
            }
        }
    }

    return 0;
}
