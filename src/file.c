#include "var.h"
#include "file.h"
#include <errno.h>
#include <wordexp.h>



int is_file(string filename){
	struct stat st={0};
	string temp=s_cs(filename);
	stat(temp.str, &st);
	_free(&temp);
	return S_ISREG(st.st_mode) || S_ISLNK(st.st_mode);
}
int is_dir(string filename){
	struct stat st={0};
	string temp=s_cs(filename);
	stat(temp.str, &st);
	_free(&temp);
	return S_ISDIR(st.st_mode);
}
int file_size(string filename){
	struct stat st={0};
	string temp=s_cs(filename);
	stat(temp.str, &st);
	_free(&temp);
	return st.st_size;
}
vector file_lines(string file){
	return s_vec(file_s(file),"\n");
}
string path_cat(string path1, string path2){
	if(!path1.len) return path2;
	if(!path2.len) return path1;
	if(path2.str[0]=='.') path2=cut(path2,".").tail;
	if(path2.str[0]=='/') path2=cut(path2,"/").tail;
	return cat(path1,path2);
}
int s_save(string in,string filename){
	string temp=s_cs(filename);
	FILE* fp=fopen(temp.str,"w");
	_free(&temp);
	if(!fp) return 0;
	string parts=in;
	while(parts.len>0){
		int ret=fwrite(parts.str,1,parts.len,fp);
		if(!ret) return 0;
		parts=slice(parts,ret,parts.len);
	}
	_free(&filename);
	_free(&in);
	fclose(fp);
	return 1;
}
string filec_s(char* filename){
	string ret={0};
	if(!filename) return ret;
	FILE* fp=fopen(filename,"r");
	if(!fp) return Null;
	fseek(fp,0,SEEK_END);
	size_t size=ftell(fp);
	fseek(fp,0,SEEK_SET);
	ret=s_new(NULL,size);
	size_t read=fread(ret.str,1,size,fp);
	fclose(fp);
	if(read!=size) _free(&ret);
	return ret;
}
string file_s(string filename){
	string temp=s_cs(filename);

    wordexp_t exp_result;
    wordexp(temp.str, &exp_result, 0);

	string ret=filec_s(exp_result.we_wordv[0]);

	_free(&temp);
	wordfree(&exp_result);
	return ret;
}
