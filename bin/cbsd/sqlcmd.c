#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "shell.h"
#include "main.h"
#include "nodes.h"      /* for other headers */
#include "eval.h"
#include "jobs.h"
#include "show.h"
#include "options.h"
#include "syntax.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "trap.h"
#include "mystring.h"
#include "builtins.h"
#include "myhistedit.h"
#include "var.h"
#include "sqlite3.h"

#include "sqlcmd.h"

char *delim;

char *
nm(void)
{
	return "cbsdsql";
}

int
sqlCB(sqlite3_stmt *stmt)
{
    int icol;
    const char      *colname;
    int allcol;
    int printheader=0;
    char *sqlcolnames = NULL;

    if (stmt == NULL) return 1;

    sqlcolnames=getenv("sqlcolnames");
    allcol = sqlite3_column_count(stmt);

    if ((printheader)&&(sqlcolnames==NULL)) {
        for ( icol = 0; icol < allcol; icol++ ) {
            colname = sqlite3_column_name(stmt, icol);
            if ( icol != ( allcol - 1 ))
                out1fmt("%s%s",colname,delim);
            else
                out1fmt("%s\n",colname);
        }
    }

    for (icol = 0; icol < allcol; icol++) {
            if (sqlcolnames)
                out1fmt("%s=\"%s\"\n",sqlite3_column_name(stmt,icol),sqlite3_column_text(stmt,icol));
            else {
                if ( icol == ( allcol - 1) )
                    out1fmt("%s\n",sqlite3_column_text(stmt,icol));
                else
                    out1fmt("%s%s",sqlite3_column_text(stmt,icol),delim);
                }
    }

    return 0;
}

int
sqlitecmd(int argc, char **argv)
{
	sqlite3	*db;
	int	 res;
	int	 i;
	char	*query;
	char	*tmp;
	char *dbdir;
	char *dbfile;
	int     maxretry=10;
	int     retry=0;
	int	ret;
	sqlite3_stmt *stmt;
	char *cp;

	if ((cp = lookupvar("sqldelimer")) == NULL)
	    delim=DEFSQLDELIMER;
	else
	    delim=cp;

	if (argc<3) { 
	    out1fmt("%s: format: %s <dbfile> <query>\n",nm(),nm());
	    return 0;
	}

	dbdir = lookupvar("dbdir");
	i=strlen(dbdir)+strlen(argv[1]);
	dbfile = calloc(strlen(dbdir)+strlen(argv[1])+strlen(DBPOSTFIX)+1, sizeof(char *));

	if (dbfile == NULL) {
	    error("Out of memory!\n");
            return (1);
	}

	sprintf(dbfile,"%s/%s%s",dbdir,argv[1],DBPOSTFIX);

	if (SQLITE_OK != (res = sqlite3_open(dbfile, &db))) {
		out1fmt("%s: Can't open database file: %s\n", nm(), dbfile);
		free(dbfile);
		return 1;
	}

	free(dbfile);

	res = 0;
        for (i = 2; i < argc; i++)
                res += strlen(argv[i]) + 1;
        if (res) {
                query = (char *)sqlite3_malloc(res);
                tmp = query;
                for (i = 2; i < argc; i++) {
                        strcpy(tmp, argv[i]);
                        tmp += strlen(tmp);
                        *tmp = ' ';
                        tmp++;
                }
                tmp[-1] = 0;
        }

        ret = sqlite3_prepare_v2(db, query, strlen(query) + 1, &stmt, NULL);

	retry = 0;
	if (ret == SQLITE_OK)
	{
	    while ( (ret!=SQLITE_DONE)&&(retry<maxretry))
	    {
		ret = sqlite3_step(stmt);
		if (ret == SQLITE_ROW) {
		    sqlCB(stmt);
		    continue;
		}
		else if (ret == SQLITE_BUSY) {
		    sqlite3_sleep(5);
		    retry++;
		}
	    }
	}

        sqlite3_finalize(stmt);

        sqlite3_free(query);
        sqlite3_close(db);

//      printf("ERR: %d(Q: %s)\n",ret,query);

        return 0;
}
