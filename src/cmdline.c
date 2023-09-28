#include "log.h"
#include "file.h"
#include "database.h"
#include "cmdline.h"

#define COMPILE_DATE "2023-09-28"

/*header
struct s_args {
	string progfile;
	string file1;
	string typesfile;
	int help;
	int vim;
};
enum FileType {
	FileError=-1,
	FileText,
	FileSQLite,
	FileCSV,
	FileBinary,
};
struct s_filetype {
	string filename;	
	string osfilename;	
	int type;
	char* rec_sep;
	char* line_sep;
	int is_quoted;
	int has_title;
	int is_readonly;
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
	}
	_free(&ls);
	return lines;
}
void cmdline_args(int argc,char** argv){
	vector words=vec_new(sizeof(var),argc);
	for(int i=0; i<argc; i++)
		words.var[i]=c_(argv[i]);

	each(words,i,string* s){
		if(!i){
			args.progfile=s[i];
			continue;
		}
		int dashes=arg_dashes(s[i]);
		if(dashes==0) args.file1=s[i];
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
		"csql v1.0.1 build: %s\n"
		"Terminal based sqlite3 editor\n"
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
struct s_filetype file_type(string filename){
	struct s_filetype ret={
		.filename=filename,
		.osfilename=filename_os(filename),
		.line_sep="\n",
		.type=FileError,
	};
	string header=file_read(filename,0,512);
	if(!header.len) return ret;
	if(s_start(header,"SQLite format 3")){
		ret.type=FileSQLite;
		vfree(header);
		return ret;
	}
	vector lines=code_split(header,"\n",0);
	string line1=lines.var[0];
	char* end=line1.ptr+line1.len;
	char* ptr=line1.ptr;
	ret.is_quoted=*ptr=='"' ? 1 : 0;
	while(ptr<end){
		unsigned char c=*ptr;
		if(c>=0xF8 && c<=0xFF){
			ret.type=FileBinary;
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
		ret.type=FileText;
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
		ret.type=FileText;
		vfree(header);
		vfree(lines);
		return ret;
	}
	ret.type=FileCSV;
	vector names=code_split(line1,ret.rec_sep,0);
	int has_title=1;
	each(names,i,string* n){
		string nn=s_dequote(n[i],"\"");
		if(!nn.len || is_numeric(nn)){
			has_title=0;
			break;
		}
	}
	ret.has_title=has_title;
	vfree(header);
	vfree(lines);
	vfree(names);
	return ret;
}
int filetype_sqlite(struct s_filetype file, window win, cross types){
	var conn=lite_conn(args.file1);
	if(lite_error(conn)) return End;
	table_list(conn, win, types);
	lite_close(conn);
	return 0;
}
int filetype_csv(struct s_filetype file, window win, cross types){
	return tsv_browse(ro(file.osfilename),win,types);
}
int csql_main(int argc,char** argv){
	default_args();
	string inimem=inifile_args();

	cmdline_args(argc,argv);
	int2 dim=vis_size();
	window win={.x=4, .y=3,.width=dim.x-8,.height=dim.y-6};

	if(args.help)
		return usage(NULL);
	if(!args.file1.len)
		return usage(NULL);

	var old=vis_init();
	string types_s=file_s(args.typesfile);
	if(!types_s.len){
		log_print();
		vis_restore(old);
		_free(&inimem);
		return -1;
	}
	cross types=cross_new(s_rows(types_s));

	struct s_filetype info=file_type(args.file1);
	if(info.type==FileSQLite){
		filetype_sqlite(info,win,types);
	}
	else if(info.type==FileCSV){
		filetype_csv(info,win,types);
	}
	else if(info.type==FileError){
		log_error(c_("Error analyzing file"));
	}
	else{
		log_error(c_("This type of file isn't handled yet"));
	}
	vfree(info.filename);
	vfree(info.osfilename);
	cross_free(&types);
	vfree(types_s);
	log_print();
	vis_restore(old);
	_free(&inimem);

	return 0;
}
