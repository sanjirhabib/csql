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
int vec_search(vector in,string s){
	for(int i=0; i<in.len; i++)
		if(eq(in.var[i],s)) return i;
	return Fail;
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
			if(i>from) vec_add(&ret,slice(in,from,i-from));
			from=i;
		}
	}
	if(from<in.len) vec_add(&ret,slice(in,from,in.len-from));
	return ret;
}

