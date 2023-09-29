#include "log.h"
#include "file.h"
#include "database.h"
#include "cmdline.h"


/*header

#define COMPILE_DATE "2023-09-30"

enum FileType {
	FileError=-1,
	FileText,
	FileSQLite,
	FileCSV,
	FileTSV,
	FileMeta,
	FileBinary,
};
enum Command {
	CmdNone=0,
	CmdSync,
	CmdDump,
};
struct s_filetype {
	string filename;	
	string cmd1;
	string cmd2;
	string cmd3;
	int type;
	char* rec_sep;
	char* line_sep;
	int is_quoted;
	int has_title;
	int is_readonly;
	int file_exists; //file exists.
};
struct s_args {
	string progfile;
	struct s_filetype file1;
	struct s_filetype file2;
	int command;
	string typesfile;
	int help;
	int vim;
	int border;
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
		.border=4,
	};
}
string inifile_args(){
	string lines=file_s(c_("/etc/csql.ini"));
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
		if(eq_c(name,"border") && val.len) args.border=atoil(val);
	}
	_free(&ls);
	return lines;
}
int cmdline_args(int argc,char** argv){
	vector words=vec_new(sizeof(var),argc);
	for(int i=0; i<argc; i++)
		words.var[i]=c_(argv[i]);

	int ret=0;
	struct s_filetype* file=&args.file1;
	struct s_filetype* file2=&args.file2;
	each(words,i,string* s){
		string v=s[i];
		if(!v.len) continue;
		if(!i){
			args.progfile=v;
			continue;
		}
		int bareword=v.str[0]!='-';
		if(bareword){
			if(!file){
				ret=log_error("extra argument %.*s",ls(v));
				break;
			}
			if(!file->filename.len) file->filename=v;
			else if(!file->cmd1.len) file->cmd1=v;
			else if(!file->cmd2.len) file->cmd2=v;
			else if(!file->cmd3.len) file->cmd3=v;
			else{
				ret=log_error("extra argument %.*s",ls(v));
				break;
			}
			continue;
		}
		else if(file && file->filename.len){
			file=file2;
			file2=NULL;
		}



		if(is_word(v,"--help -h -?")){
			args.help=1;
		}
		else if(is_word(v,"-sync -dump")){
			if(args.command){
				ret=log_error("extra argument %.*s",ls(v));
				break;
			}
			if(eq_c(v,"-sync")) args.command=CmdSync;
			else if(eq_c(v,"-dump")) args.command=CmdDump;
		}
		else if(is_word(v,"-tsv -csv -db -sql -meta -ro")){
			if(!file){
				ret=log_error("extra argument %.*s",ls(v));
				break;
			}
			if(eq_c(v,"-tsv")) file->type=FileTSV;
			else if(eq_c(v,"-csv")) file->type=FileCSV;
			else if(eq_c(v,"-db")) file->type=FileSQLite;
			else if(eq_c(v,"-txt")) file->type=FileText;
			else if(eq_c(v,"-meta")) file->type=FileMeta;
			else if(eq_c(v,"-ro")) file->is_readonly=1;
		}
		else ret=log_error("unknown switch %.*s",ls(v));
	}
	_free(&words);
	return ret;
}
int arg_dashes(string s){
	if(s_start(s,"--")) return 2;	
	if(s_start(s,"-")) return 1;	
	return 0;
}
int usage(char* msg){
	if(msg)
		printf("%s\n\n",msg);
	printf(
		"csql v1.0.1 build: %s\n"
		"Terminal based sqlite3 editor\n"
		"(c) 2023 by Sanjir Habib <habib@habibur.com>\n"
		"\n"
		"Usage: csql [<options ...>] <source ...> [<options ...> <destination ...>]\n"
		"where:\n"
		"  <options ...>\n"
		"\n"
		"    commands\n"
		"    -dump\n"
		"        dump rows\n"
		"    -sync\n"
		"        sync between two by reading from source and writing to destination\n"
		"\n"
		"    file type\n"
		"    -tsv\n"
		"        tab seperated file\n"
		"    -csv\n"
		"        comma seperated file\n"
		"    -db\n"
		"        sqlite3 database\n"
		"    -txt\n"
		"        text file\n"
		"    -sql\n"
		"        sql dump file\n"
		"    -meta\n"
		"        meta file describing database schema\n"
		"\n"
		"  <source ...> and <destination ...>\n"
		"        <filename> [<table>] [<id>]\n"
		"\n"
		"  -h, --help, -?\n"
		"      display this help\n"
		"\n",
		COMPILE_DATE
	);
	return 0;
}
struct s_filetype file_type(struct s_filetype ret){
	string filename=ret.filename;
	if(!filename.len) return ret;

	string header=file_read(filename,0,512);
	if(!header.len)
		return ret;
	ret.file_exists=0;

	if(s_start(header,"SQLite format 3")){
		if(!ret.type) ret.type=FileSQLite;
		vfree(header);
		return ret;
	}
	else if(ret.type==FileSQLite){
		log_error("File is not a SQLite database");
		vfree(header);
		return ret;
	}
	ret.line_sep="\n";
	vector lines=code_split(header,"\n",0);
	string line1=lines.var[0];
	char* end=line1.ptr+line1.len;
	char* ptr=line1.ptr;
	ret.is_quoted=*ptr=='"' ? 1 : 0;
	while(ptr<end){
		unsigned char c=*ptr;
		if(c>=0xF8 && c<=0xFF){
			if(!ret.type)
				ret.type=FileBinary;
			else if(ret.type!=FileBinary)
				log_error("File is binary");
			vfree(header);
			vfree(lines);
			return ret;
		}
		else if(c=='"'){
			ptr=jump_quote(ptr,end);
		}
		else if(c==','){
			ret.rec_sep=",";
			break;
		}
		else if(c=='\t'){
			ret.rec_sep="\t";
			break;
		}
		ptr++;
	}
	if(!ret.rec_sep){
		if(!ret.type)
			ret.type=FileText;
		else if(ret.type!=FileText)
			log_error("File seems like a a text file");
		vfree(header);
		vfree(lines);
		return ret;
	}
	int len=char_count(line1,ret.rec_sep[0]);
	int hits=0;
	int miss=0;
	for(int i=1; i<lines.len-1; i++){
		int len2=char_count(lines.var[i],ret.rec_sep[0]);
		if(len==len2) hits++;
		else miss++;
		if(len2>len){
			ret.type=FileText;
			vfree(header);
			vfree(lines);
			return ret;
		}
	}
	if(!ret.is_quoted && lines.len>1 && lines.var[1].len && lines.var[1].str[0]=='"') ret.is_quoted=1;
	if(hits<=miss){
		if(!ret.type)
			ret.type=FileText;
		else if(ret.type!=FileText)
			log_error("CSV file is malformed");
		vfree(header);
		vfree(lines);
		return ret;
	}
	if(!ret.type)
		ret.type=FileCSV;
	if(!ret.has_title){
		vector names=code_split(line1,ret.rec_sep,0);
		int has_title=1;
		each(names,i,string* n){
			string v=s_dequote(n[i],"\"");
			if(!v.len || is_numeric(v)){
				has_title=0;
				break;
			}
		}
		vfree(names);
		ret.has_title=has_title;
	}
	vfree(header);
	vfree(lines);
	return ret;
}
int is_alpha(string in,char* extra){
	if(!in.len) return 0;
	char* str=in.str;
	if(!is_char(*str,1,0,"_")) return 0;
	char* end=in.str+in.len;
	while(str<end){
		if(!is_char(*str,1,1,extra)) return 0;
		str++;
	}
	return 1;
}
int sqlite_dump(string in, string table, struct s_filetype out){
	var conn=lite_conn(in);
	if(lite_error(conn)) return End;
	if(!is_alpha(table,"_")){
		return log_error("invalid table name");
	}
	var rs=lite_rs(conn,print_s("select * from %.*s order by 1",ls(table)),NullMap);
	if(lite_error(conn)) return -1;
	vector keys=rs_cols(rs);
	if(!keys.len) return -1;
	var fp=file_fp(out.filename,"w");
	fp_write(fp,vec_s(keys,"\t"));
	fp_write(fp,c_("\n"));
	vector ret=Null;
	while((ret=rs_row(rs)).len){
		fp_write(fp,vec_s(ret,"\t"));
		fp_write(fp,c_("\n"));
	}
	lite_close(conn);
	return 0;
}
int sqlite_browse(struct s_filetype file, window win, cross types){
	var conn=lite_conn(args.file1.filename);
	if(lite_error(conn)) return End;
	table_list(conn, win, types);
	lite_close(conn);
	return 0;
}
int csvfile_browse(struct s_filetype file, window win, cross types){
	return tsv_browse(ro(file.filename),win,types);
}
int csql_main(int argc,char** argv){
	default_args();
	string inimem=inifile_args();

	int ret=cmdline_args(argc,argv);
	if(ret==End){
		log_print();
		vfree(inimem);
		return End;
	}

	int2 dim=vis_size();
	window win={.x=args.border, .y=args.border/2};
	win.width=dim.x-win.x*2;
	win.height=dim.y-win.y*2;

	if(args.help)
		return usage(NULL);
	if(!args.file1.filename.len)
		return usage(NULL);

	string types_s=file_s(args.typesfile);
	if(!types_s.len){
		log_print();
		_free(&inimem);
		return -1;
	}
	cross types=cross_new(s_rows(types_s));

	args.file1=file_type(args.file1);
	args.file2=file_type(args.file2);

	if(logs.has_new){
		// do nothing.
	}
	else if(args.command){
		if(args.command==CmdDump){
			if(args.file1.type!=FileSQLite)
				log_error("source is not a sqlite database for dumping");
			else if(!args.file1.cmd1.len)
				log_error("provide a table name for dumping");
			else
				sqlite_dump(args.file1.filename, args.file1.cmd1, args.file2);
		}
	}
	else if(args.file1.type==FileSQLite){
		var old=vis_init();
		sqlite_browse(args.file1,win,types);
		vis_restore(old);
	}
	else if(args.file1.type==FileCSV){
		var old=vis_init();
		csvfile_browse(args.file1,win,types);
		vis_restore(old);
	}
	else{
		log_error("This type of file isn't handled yet");
	}
	cross_free(&types);
	vfree(types_s);
	log_print();
	_free(&inimem);

	return 0;
}
