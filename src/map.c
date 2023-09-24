#include "var.h"
#include "map.h"

#define map_all(ret, ...) _m_all(ret, ##__VA_ARGS__,end_new())

/*header
typedef struct s_map {
	vector keys;
	vector vals;
	vector index;
} map;
typedef struct {
	int head;
	int tail;
} mapindex;
end*/

map vec_map(vector keys,vector vals){
	return (map){
		.keys=keys,
		.vals=vals,
		.index=keys_index(keys),
	};
}
map map_new(){
	return map_new_ex(sizeof(var));
}
map map_new_ex(int datasize){
	map ret={0};
	ret.keys=vec_new();
	ret.vals=vec_new_ex(datasize,0);
	ret.index=vec_new_ex(sizeof(mapindex),0);
	return ret;
}
int keys_idx(vector keys, vector index, string key){
	if(!keys.len) return None;
	int slot=hash(key) & index.len-1;
	slot=((mapindex*)index.str)[slot].head-1;
	while(1){
		if(slot<0) return None;
		if(eq(key,((var*)keys.str)[slot])) return slot;
		slot=((mapindex*)index.str)[slot].tail-1;
	}
	//never.
}
int map_c_i(map in,char* key){
	return p_i(map_p_c(in,key));
}
var map_c(map in,char* key){
	return p_(map_p_c(in,key));
}
string map_iget(map in,int key){
	return p_(map_igetp(in,key));
}
void* map_p_c(map in,const char* key){
	return vec_p(in.vals,keys_idx(in.keys,in.index,c_(key)));
}
void* map_igetp(map in,int key){
	return vec_p(in.vals,keys_idx(in.keys,in.index,i_(key)));
}
string map_val(map in,int idx){
	return vec_val(in.vals,idx);
}
string map_key(map in,int idx){
	return vec_val(in.keys,idx);
}
void* map_getp(map in,var key){
	return vec_p(in.vals,keys_idx(in.keys,in.index,key));
}
string map_get(map in,var key){
	return p_(map_getp(in,key));
}
map* map_delc(map* in, char* key){
	return map_del_ex(in,c_(key),_free);
}
map* map_del(map* in, var key){
	return map_del_ex(in,key,_free);
}
map* map_del_idx(map* in, int slot,void* callback){
	if(slot<0||slot>=in->keys.len) return in;
	vec_del_ex(&in->vals,slot,1,callback);
	vec_del_ex(&in->keys,slot,1,_free);
	int newlen=pow2(in->keys.len);
	if(newlen!=in->index.len){
		in->index=resize(in->index,newlen);
		return map_reindex(in,0);
	}
	mapindex* idx=(void*)in->index.str;
	int slotnext=idx[slot].tail;
	for(int i=0; i<in->index.len; i++){
		if(idx[i].head==slot+1) idx[i].head=slotnext;
		if(i>=slot && i<in->keys.len) idx[i].tail=idx[i+1].tail;
		if(idx[i].tail==slot+1) idx[i].tail=slotnext;
		if(idx[i].head>slot+1) idx[i].head--;
		if(idx[i].tail>slot+1) idx[i].tail--;
	}
	idx[in->keys.len].tail=0;
	return in;
}
map* map_del_ex(map* in, var key,void* callback){
	return map_del_idx(in,keys_idx(in->keys,in->index,key),callback);
}
void* map_add_ex(map* in, var key, void* data,void* callback){
	int slot=keys_idx(in->keys,in->index,key);
	if(slot!=None){ //key exists
		if(callback) ((void(*)(void*))callback)(vec_p(in->vals,slot));
		if(data) memcpy(in->vals.str+in->vals.datasize*slot,data,in->vals.datasize);
		_free(&key);
		return in->vals.str+in->vals.datasize*slot;
	}
	if(in->index.len<in->keys.len+1){
		in->index=resize(in->index,pow2(in->keys.len+1));
		map_reindex(in,0);
	}
	vec_add(&in->keys,key);
	void* ret=vec_addp(&in->vals,data);
	map_reindex(in,in->keys.len-1);
	return ret;
}
vector keys_index(vector keys){
	vector ret=vec_new_ex(sizeof(mapindex),pow2(keys.len));
	mapindex* idx=(mapindex*)ret.str;
	for(int i=0; i<keys.len; i++){
		int slot=hash(keys.var[i]) & ret.len-1;
		if(idx[slot].head) idx[i].tail=idx[slot].head;
		idx[slot].head=i+1;
	}
	return ret;
}
map* map_reindex(map* in,int from){
	mapindex* idx=(mapindex*)in->index.str;
	if(!from) memset(idx,0,in->index.datasize*in->index.len);
	for(int i=from; i<in->keys.len; i++){
		var key=((var*)in->keys.str)[i];
		int slot=hash(key) & in->index.len-1;
		if(idx[slot].head) idx[i].tail=idx[slot].head;
		idx[slot].head=i+1;
	}
	return in;
}
void* map_caddi(map* in,char* key,int val){
	return map_add(in,c_(key),i_(val));
}
void* map_caddc(map* in,char* key,char* val){
	return map_add(in,c_(key),c_(val));
}
void* map_cadd(map* in,char* key,var val){
	return map_add(in,c_(key),val);
}
void* map_add(map* in,var key, var val){
	return map_add_ex(in,key,&val,_free);
}
void* map_last(map in){
	return vec_last(in.vals);
}
void* map_first(map in){
	return vec_first(in.keys);
}
void* map_lastkey(map in){
	return vec_last(in.keys);
}
void* map_firstkey(map in){
	return vec_first(in.keys);
}
map map_ro(map in){
	in.vals.readonly=1;
	in.keys.readonly=1;
	in.index.readonly=1;
	return in;
}
void map_free(map* in){
	map_free_ex(in,_free);
}
void map_free_ex(map* in,void* callback){
	vec_free_ex(&in->vals,callback);
	vec_free(&in->keys);
	_free(&in->index);
}
void* map_p_i(map m,int i){
	return m.vals.str+m.vals.datasize*i;
}
map _m_all(string in,...){
	map ret=map_new();
	va_list args;
	va_start(args,in);
	var name=in;
	while(1){
		if(name.len==IsEnd) break;
		var val=va_arg(args,var);
		map_add(&ret,name,val);
		name=va_arg(args,var);
	}
	va_end(args);
	return ret;
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
var map_var(map in){
	return (var){.ptr=to_heap(&in,sizeof(in)),.len=IsMap};
}
int rows_count(map in){
	return in.vals.len/in.keys.len;
}
map rows_row(map in, int idx){
	if(idx<0||idx>=rows_count(in)) return (map){0};
	in.vals.str=in.vals.str+in.vals.datasize*in.keys.len*idx;
	in.vals.len=in.keys.len;
	in.vals.readonly=1;
	in.keys.readonly=1;
	in.index.readonly=1;
	return in;
}
string rows_s(const map in){
	vector lines=vec_new();
	lines=resize(lines,in.vals.len/in.keys.len+1);
	lines.var[0]=vec_s(vec_escape(in.keys),"\t");
	for(int i=0; i<in.vals.len/in.keys.len; i++){
		lines.var[i+1]=vec_s(vec_escape(rows_row(in,i).vals),"\t");
	}
	return vec_s(lines,"\n");
}
string map_s(map in,char* sepkey,char* sepval){
	if(!in.keys.len) return (string){0};
	string sep1=c_(sepkey);
	string sep2=c_(sepval);
	int len=in.keys.len*(sep1.len+sep2.len)+vec_strlen(in.keys)+vec_strlen(in.vals);
	pair ret=buff_new(len);
	for(int i=0; i<in.keys.len; i++){
		buff_add(&ret,ro(in.keys.var[i]));
		buff_add(&ret,sep1);
		buff_add(&ret,ro(in.vals.var[i]));
		buff_add(&ret,sep2);
	}
	map_free(&in);
	return ret.head;
}
map s_rows(string in){
	vector lines=s_vec(trim(in),"\n");
	if(!lines.len) return (map){0};
	vector keys=s_vec(lines.var[0],"\t");
	if(!keys.len) return (map){0};
	vector index=keys_index(keys);
	vector vals=vec_new();
	for(int i=1; i<lines.len; i++){
		vector temp=s_vec(lines.var[i],"\t");
		temp=resize(temp,keys.len);
		vec_add_ex(&vals,temp.len,temp.ptr);
		_free(&temp);
	}
	_free(&lines);
	return (map){
		.keys=keys,
		.vals=vals,
		.index=index,
	};
}
string _json(var in){
	string ret={0};
	if(!in.ptr && !in.len) return c_("NULL");
	if(in.len>0 && !in.datasize) //string
		return s_escape(in); // todo: only for '"'

	if(in.len==IsMap){
		map m=*(map*)in.ptr;
		ret=cat(ret,c_("{"));
		for(int i=0; i<m.keys.len; i++){
			if(i) ret=cat(ret,c_(", "));
			ret=cat(ret, _json(map_key(m,i)));
			ret=cat(ret,c_(": "));
			ret=cat(ret, _json(vec_val(m.vals,i)));
		}
		ret=cat(ret,c_("}"));
		return ret;
	}
	if(in.len>0 && in.datasize){
		ret=cat(ret,c_("["));
		for(int i=0; i<in.len; i++){
			if(i) ret=cat(ret,c_(", "));
			ret=cat(ret, _json(vec_val(in,i)));
		}
		ret=cat(ret,c_("]"));
		return ret;
	}
	return _s(in);
}
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
