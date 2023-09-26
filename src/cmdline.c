#include "map.h"
#include "log.h"
#include "database.h"
#include "browse.h"
#include "cmdline.h"

#define COMPILE_DATE "2023-09-26"

/*header
struct s_args {
	string progfile;
	string db;
	string typesfile;
	int help;
	int vim;
};
extern struct s_args args;

end*/

void args_release(struct s_args* in){
}

struct s_args args={0};

void default_args(){
	args=(struct s_args){
		.typesfile=c_("~/.csql/types.tsv"),
		.vim=1,
	};
}
void inifile_args(){
	string lines=filec_s("/etc/csql.ini");
	vector ls=s_vec(lines,"\n");
	string section={0};
	each(ls,i,string* ln){
		string s=trim(ln[i]);
		if(!s.len) continue;
		if(s_start(s,"[")){
			section=trim(s_dequote(s,"["));
			continue;
		}
		vector parts=code_split(s,";#",2);
		pair nval=cut(parts.var[0],"=");
		_free(&parts);
		string name=trim(nval.head);
		string val=trim(nval.tail);

		if(eq_c(name,"vim")) args.vim=is_word(val,"yes 1") ? 1 : 0;
		if(eq_c(name,"typesfile") && val.len) args.typesfile=val;
	}
	_free(&lines);
	_free(&ls);
}
void cmdline_args(int argc,char** argv){
	vector words=vec_new_ex(sizeof(var),argc);
	for(int i=0; i<argc; i++)
		words.var[i]=c_(argv[i]);

	each(words,i,string* s){
		if(!i){
			args.progfile=s[i];
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
		"csql v1.0 build: %s\n"
		"Terminal based sqlite3 database viewer and editor\n"
		"(c) 2023 by Sanjir Habib <habib@habibur.com>\n"
		"\n"
		"Usage: csql [options] <database>\n"
		"    -h, --help, -?\n"
		"        display this help\n"
		"\n",
		COMPILE_DATE
	);
	return 0;
}

int csql_main(int argc,char** argv){
	default_args();
	inifile_args();

	cmdline_args(argc,argv);

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
	string types_s=file_s(args.typesfile);
	if(!types_s.len){
		msg("%.*s: %s", ls(args.typesfile), strerror(errno));
		return -1;
	}
	cross types=cross_new(s_rows(types_s));


	table_list(conn,types);


	_free(&types_s);
	lite_close(conn);
	cross_free(&types);
	vis_restore(old);

	return 0;
}
