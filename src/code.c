#include "map.h"
#include "code.h"

char* jump_quote(char* ptr, char* end){
	if(end-ptr<2) return ptr;
	if(!strchr("\"'`",*ptr)) return ptr;
	char c=*ptr;
	ptr++;
	while(ptr<end){
		if(*ptr=='\\') ptr+=2;
		else if(*ptr==c) return ptr;
		else ptr++;
	}
	return ptr;
}
char* jump_paren(char* ptr, char* end){
	if(end-ptr<2) return ptr;
	if(!strchr("([{",*ptr)) return ptr;
	int level=1;
	char c=*ptr;
	char o=c+1+c/80; //the closing paren for each of 3 kinds of paren
	ptr++;
	while(ptr<end){
		ptr=jump_quote(ptr,end);
		ptr=jump_comment(ptr,end);
		if(*ptr==o) level--;
		else if(*ptr==c) level++;
		if(!level) return ptr;
		ptr++;
	}
	return ptr;
}
char* jump_comment(char* ptr, char* end){
	if(end-ptr<3) return ptr;
	if(memcmp("\n--",ptr,3)!=0) return ptr;
	ptr+=3;
	while(ptr<end){
		if(*ptr=='\n') return ptr;
		else ptr++;
	}
	return ptr;
}
char* jump_separator(char* ptr,char* end,char* seps){
	while(ptr<end){
		char* from=ptr;
		ptr=jump_comment(ptr,end);
		ptr=jump_char(ptr,end,seps);
		if(ptr==from) return ptr;
	}
	return ptr;
}
/*
   old
   seps " \t\n\r;("
   terms " \t\n\r[{('\"`;"
   nonterms " \t\n\r;"

   new
   seps " \t\n\r"
   terms ";("
   stops ""

	//new stops = old param:seps
	//new terms = old param:seps
	//seps = old var:nonterms
	
	string stops=cat_all(c_(seps),c_(terms),cl_("\0",1));
	*/
vector code_split(string in,char* seps,int total){
	char* end=in.str+in.len;
	char* ptr=in.str;
	vector ret=vec_new();
	char* from=ptr;
	char* terms=NULL;
	int slen=strlen(seps);
	char quotes[]="([{\"'`";
	terms=memcpy(malloc(slen+sizeof(quotes)),seps,slen);
	strcpy(terms+slen,quotes);
	char* nonterms=memset(malloc(slen+1),0,slen+1);
	int off=0;
	for(int i=0; i<slen; i++){
		if(strchr(quotes,seps[i])) continue;
		nonterms[off++]=seps[i];
	}
	while(ptr<end){
		while(ptr<end){
			char* from2=ptr;
			ptr=jump_paren(ptr,end);
			char* temp=ptr;
			ptr=jump_quote(ptr,end);
			if(temp!=ptr) ptr++;
			ptr=jump_nonchar(ptr,end,terms);
			if(ptr==from2) break;
			if(ptr>=end) break;
			if(strchr(terms,*ptr)) break;
		}
		if(ptr==from||ptr>=end) break;
		if(strchr(seps,*ptr)){
			vec_add(&ret,cl_(from,ptr-from));
			ptr=jump_separator(ptr,end,nonterms);
			from=ptr;
			if(total && ret.len>=total-1) break;
		}
	}
	if(from<end) vec_add(&ret,cl_(from,end-from));
	free(terms);
	free(nonterms);
	return ret;
}
int vec_count(vector in,char* search){
	int ret=0;
	var s=c_(search);
	for(int i=0; i<in.len; i++)
		if(eq(in.var[i],s)) ret++;
	return ret;
}
int vec_search(vector in,string s){
	for(int i=0; i<in.len; i++)
		if(eq(in.var[i],s)) return i;
	return End;
}
string vec_start(vector toks,char* start){
	for(int i=0; i<toks.len; i++){
		if(s_start(get(toks,i),start)) return get(toks,i);
	}
	return Null;
}
vector vec_trim(vector in){
	for(int i=0; i<in.len; i++){
		in.var[i]=trim(in.var[i]);
	}
	return in;
}
int vec_words(vector toks,char* terms){
	for(int i=0; i<toks.len; i++){
		if(is_word(get(toks,i),terms)) return i;
	}
	return -1;
}
vector vec_split(vector in,char* words){
	vector ret=vec_new();
	int from=0;
	for(int i=0; i<in.len; i++){
		if(is_word(in.var[i],words)){
			if(i>from) vec_add(&ret,sub(in,from,i-from));
			from=i;
		}
	}
	if(from<in.len) vec_add(&ret,sub(in,from,in.len-from));
	return ret;
}
char* jump_nonchar(char* ptr, char* end,char* chars){
	while(ptr<end){
		if(strchr(chars,*ptr)) return ptr;
		else ptr++;
	}
	return ptr;
}
char* jump_char(char* ptr, char* end,char* chars){
	while(ptr<end){
		if(!strchr(chars,*ptr)) return ptr;
		else ptr++;
	}
	return ptr;
}
map s_map(string in){
	vector toks=code_split(in," ",0);
	int len=toks.len;
	vector keys=vec_new_ex(sizeof(var),toks.len/2);
	vector vals=vec_new_ex(sizeof(var),toks.len/2);
	for(int i=0; i<toks.len/2; i++){
		keys.var[i]=toks.var[i*2];
		vals.var[i]=toks.var[i*2+1];
	}
	_free(&toks);
	return (map){
		.keys=keys,
		.vals=vals,
		.index=keys_index(keys),
	};
}
