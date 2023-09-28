#include "log.h"
#include "file.h"

#include <wordexp.h>



int is_file(string filename){
	struct stat st={0};
	string temp=s_c(filename);
	stat(temp.str, &st);
	_free(&temp);
	return S_ISREG(st.st_mode) || S_ISLNK(st.st_mode);
}
int is_dir(string filename){
	struct stat st={0};
	string temp=s_c(filename);
	stat(temp.str, &st);
	_free(&temp);
	return S_ISDIR(st.st_mode);
}
int file_size(string filename){
	struct stat st={0};
	string temp=s_c(filename);
	stat(temp.str, &st);
	_free(&temp);
	return st.st_size;
}
string path_cat(string path1, string path2){
	if(!path1.len) return path2;
	if(!path2.len) return path1;
	if(path2.str[0]=='.') path2=cut(path2,".").tail;
	if(path2.str[0]=='/') path2=cut(path2,"/").tail;
	return cat(path1,path2);
}
int s_save(string in,string filename){
	string name=filename_os(filename);
	FILE* fp=fopen(name.str,"w");
	if(!fp){
		os_log(name);
		_free(&in);
		return 0;
	}
	string parts=in;
	while(parts.len>0){
		int ret=fwrite(parts.str,1,parts.len,fp);
		if(!ret) return 0;
		parts=sub(parts,ret,parts.len);
	}
	_free(&name);
	_free(&in);
	fclose(fp);
	return 1;
}
string file_read(string filename,int from, int len){
	string name=filename_os(filename);
	string ret=NullStr;
	if(!name.len) return ret;
	FILE* fp=fopen(name.str,"r");
	if(!fp){
		os_log(name);
		return ret;
	}
	fseek(fp,0,SEEK_END);
	size_t size=ftell(fp);
	fseek(fp,0,SEEK_SET);
	range r=len_range(size,from,len==End ? size : len);
	ret=s_new(NULL,r.len);
	size_t read=fread(ret.str,1,r.len,fp);
	fclose(fp);
	if(read!=r.len) _free(&ret);
	vfree(name);
	return ret;
}
string file_s(string filename){
	return file_read(filename,0,End);
}
string filename_os(string filename){
	filename=s_nullterm(filename);
    wordexp_t exp_result;
    wordexp(filename.str, &exp_result, 0);
	string ret=_dup(cl_(exp_result.we_wordv[0],strlen(exp_result.we_wordv[0])+1));
	wordfree(&exp_result);
	_free(&filename);
	return ret;
}
