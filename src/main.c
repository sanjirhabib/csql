#include "browse.h"
#include "database.h"
#include "main.h"

/*header
struct s_args {
	string binary;
	string db;
	int help;
};
extern struct s_args args;
end*/

struct s_args args={0};

void process_cmdline(int argc,char** argv){
	vector words=vec_new_ex(sizeof(var),argc);
	for(int i=0; i<argc; i++)
		words.var[i]=c_(argv[i]);

	each(words,i,string* s){
		if(!i){
			args.binary=s[i];
			continue;
		}
		int dashes=arg_dashes(s[i]);
		if(dashes==0) args.db=s[i];
		if(is_word(s[i],"--help -h -?")){
			args.help=1;
		}
	}
	_free(&words);
}
int arg_dashes(string s){
	if(s_start(s,"--")) return 2;	
	if(s_start(s,"-")) return 1;	
	return 0;
}
int usage(char* msg){
	if(msg)
		printf("%s\n",msg);
	printf(
		"csql v1 build: %s\n"
		"Terminal based sqlite3 database viewer and editor\n"
		"(c) 2023 by Sanjir Habib <habib@habibur.com>\n"
		"\n"
		"Usage: csql [options] <database>\n"
		"    -h, --help, -?\n"
		"        display this help\n"
		"\n",
		__DATE__
	);
	return 0;
}
int main(int argc,char** argv){
	process_cmdline(argc,argv);
	if(args.help)
		return usage(NULL);
	if(!args.db.len)
		return usage(NULL);

	var old=vis_init();

	var conn=lite_conn(args.db);
	if(lite_error(conn)){
		msg("Error: Can't open database file %.*s",ls(args.db));
		return -1;
	}
	table_list(conn);
	lite_close(conn);

	vis_restore(old);
	return 0;
}
