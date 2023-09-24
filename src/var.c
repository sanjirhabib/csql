#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <sys/stat.h>

#include "var.h"


#define ls(x) (x).len>0 ? (x).len : 0,(x).str
#define cat_all(ret, ...) _cat_all(ret, ##__VA_ARGS__,end_new())
#define vec_all(ret, ...) _vec_all(ret, ##__VA_ARGS__,end_new())
#define vec_append(ret, ...) _vec_append(ret, ##__VA_ARGS__,end_new())
#define min(x,y) (x<y?x:y)
#define max(x,y) (x>y?x:y)
#define between(x,y,z) ((y)<(x) ? (x) : ((y)>(z) ? (z) : (y))) 
#define Null (var){0}
#define None (-1)
#define map_each(x,y,z) z=x.vals.ptr; for(int y=0; y<x.keys.len; y++)
#define each(x,y,z) z=x.ptr; for(int y=0; y<x.len; y++)


/*header
typedef struct {
	int from;
	int len;
	int next;
} part;
typedef enum {
	IsNull=0,
	IsPtr=-1,
	IsEnd=-3,
	IsInt=-4,
	IsError=-5,
	IsNone=-6,
	IsMap=-7,
	IsDouble=-8
} Type;
typedef struct s_int2 {
	int x;
	int y;
} int2;
typedef struct s_var {
	union {
		int i;
		int2 xy;
		double f;
		void* ptr;
		char* str;
		int* ints;
		long long ll;
		struct s_var* var;
	};
	long long len : 47;
	unsigned long long readonly : 1;
	unsigned short datasize;
} var;

typedef var string;
typedef var vector;

typedef struct s_pair {
	string head;
	string tail;
} pair;
end*/

int hash(var v){
	if(v.len>0) return cl_hash(v.ptr,v.len);
	int ret=0x1d4592f8+(int)v.ll+v.ll>>32;
	return ret ? ret : 0xc533c700;
}
int cl_hash(char *str, int len){
	int ret = 0x1d4592f8;
	int i=0;
	while(i<len) ret=((ret<<5)+ret)+tolower(str[i++]);
	return ret ? ret : 0xc533c700; //never return 0
};
void pair_free(pair* pair){
	_free(&pair->head);
	_free(&pair->tail);
}
int s_start(string in,char* sub){
	int len=strlen(sub);
	if(in.len<len) return 0;
	return c_eq(in.str,sub,len);
}
var p_(var* in){
	return in ? ro(*in) : Null;
}
var ptr_(void* in){
	return (var){
		.ptr=in,
		.len=IsPtr,
	};
}
void* getp(vector in, int index){
	return vec_p(in,index);
}
var get(vector in, int index){
	return vec_val(in,index);
}
var vec_val(vector in, int index){
	return p_(vec_p(in,index));
}
void* vec_p(vector in,int index){
	if(index<0||index>=in.len) return NULL;
	return in.str+max(in.datasize,1)*index;
}
int atoil(var in){
	if(in.len>32||in.len<=0) return 0;
	char temp[33]={0};
	memcpy(temp,in.str,in.len);
	_free(&in);
	return atoi(temp);
}
int _i(var in){
	return p_i(&in);
}
int p_i(var* in){
	if(!in||!in->len) return 0;	
	if(in->len>0) return atoil(*in);
	if(in->len==IsInt) return in->i;
	if(in->len==IsDouble) return (int)(in->f);
	return 0;
}
string i_s(long long in){
	char ret[21]={0};
	int at=sizeof(ret)-2;
	long long temp=abs(in);
	do{ ret[at--]='0'+temp%10; } while ((temp/=10));
	if(in<0) ret[at--]='-';
	return c_copy(ret+at+1);
}
void vec_free(vector* in){
	vec_free_ex(in,_free);
}
void vec_free_ex(vector* in,void* data_free){
	if(in->readonly) return;
	if(data_free){
		void(*callback)(void*)=data_free;
		for(int i=0; i<in->len; i++){
			callback(in->str+in->datasize*i);
		}
	}
	_free(in);
}
var vfree(var in){
	_free(&in);
	return (var){ .datasize=in.datasize };
}
void _free(var* in){
	if(in->readonly||in->len<=0) return;
	mem_resize(in->ptr,0,0);
	in->len=0;
	in->ptr=NULL;
}
var i_(int val){
	return (var){ .i=val, .len=IsInt };
}
string ptr_s(void* in){
	return in ? *(string*)in : Null;
}
void* vec_last(vector in){
	return vec_p(in,in.len-1);
}
void* vec_first(vector in){
	return vec_p(in,0);
}
int pow2(int i){
	if(!i) return 0;
	i--;
	int ret=2;
	while(i>>=1) ret<<=1;
	return ret;
};
vector vec_del(vector* in,int idx){
	return vec_del_ex(in,idx,1,_free);
}
part len_part(int full,int from,int len){
	if(from<0) from=0;
	if(from>full) from=full;
	//if(!len) len=full-from; //len 0=to end, now len 0 is 0 len string.
	if(len<0) len=full+len-from;
	if(len<0) len=0;
	if(from+len>full) len=full-from;
	return (part){
		.from=from,
		.len=len
	};
}
var ro(var in){
	in.readonly=1;
	return in;
}
var own(var* in){
	if(!in) return Null;
	var ret=*in;
	in->readonly=1;
	return ret;
}
var move(var* in){
	var ret=*in;
	*in=Null;
	return ret;
}
vector vec_disown(vector in){
	if(in.readonly) in=_dup(in);
	for(int i=0; i<in.len; i++){
		in.var[i].readonly=1;
	}
	return in;
}
string s_own(string in){
	if(in.len && in.readonly) return _dup(in);
	return in;
}
vector vec_own(vector in){
	for(int i=0; i<in.len; i++){
		if(in.var[i].readonly) in.var[i]=_dup(in.var[i]);
	}
	return in;
}
vector vec_del_ex(vector* in,int idx,int len,void* callback){
	part r=len_part(in->len,idx,len);
	if(!r.len) return *in;
	if(!r.from && r.len==in->len){
		vec_free_ex(in,callback);
		return *in;
	}
	int datasize=max(in->datasize,1);	
	if(callback){
		for(int i=r.from; i<r.from+r.len; i++){
			((void(*)(void*))callback)((void*)(in->str+datasize*i));
		}
	}
	if(r.from+r.len<in->len){
		memmove(
			in->str+datasize*r.from,
			in->str+datasize*(r.from+r.len),
			datasize*(in->len-(r.from+r.len))
		);
	}
	*in=resize(*in,in->len-r.len);
	return *in;
}
vector splice(vector in,int from,int len,vector by,void* callback){
	if(in.readonly) in=_dup(in);
	int datasize=in.datasize ? in.datasize : by.datasize;
	datasize=max(datasize,1);
	if(by.len && max(by.datasize,1)!=datasize)
		die("splice datasize doesn't match");
	if(callback){
		for(int i=from; i<from+len; i++){
			((void(*)(void*))callback)(vec_p(in,i));
		}
	}
	int add=by.len ? by.len-len : -len;
	if(add>0){
		in=resize(in,in.len+add);
		memmove(vec_p(in,from+len+add),vec_p(in,from+len),datasize*(in.len-from-len-add));
	}
	else if(add<0){
		memmove(vec_p(in,from+len+add),vec_p(in,from+len),datasize*(in.len-from-len));
		in=resize(in,in.len+add);
	}
	if(by.len){
		memcpy(vec_p(in,from),by.str,by.len*datasize);
		_free(&by);
	}
	return in;
}
vector vec_new(){
	return (vector){.datasize=sizeof(var)};
}
vector vec_new_ex(int datasize,int len){
	vector ret=(vector){.datasize=max(datasize,1)};
	if(len) ret=resize(ret,len);
	return ret;
}
int char_count(string in,char c){
	int ret=0;
	for(int i=0; i<in.len; i++) if(in.str[i]==c) ret++;
	return ret;
}
void* vec_insert(vector* in, int position, int count, void* data){
	int len=in->len-position;
	if(!len) return vec_add_ex(in,count,data);
	int datasize=max(in->datasize,1);
	*in=resize(*in,in->len+count);
	void* space=in->str+datasize*position;	
	memmove(space+datasize*count,space,datasize*len);
	memcpy(space,data,datasize*count);
	return space;
}
vector vec_set(vector in,int idx,var val){
	if(idx<0||idx>=in.len) return in;
	_free(&in.var[idx]);
	in.var[idx]=val;
	return in;
}
void* vec_add_ex(vector* in,int count,void* data){
	if(!in->datasize) in->datasize=sizeof(var);
	if(count<1) return NULL;
	void* ret=grow(in,count);
	if(data) memcpy(ret,data,in->datasize*count);
	return ret;
}
void* vec_addp(vector* in,void* data){
	return vec_add_ex(in,1,data);
}
vector a_vec(void* in,int datasize,int len){
	return (vector){
		.str=in,
		.datasize=datasize,
		.len=len,
		.readonly=1
	};
}
void* vec_add(vector* in,var data){
	//if(in->datasize!=sizeof(var)) die("data size mismatch");
	return vec_addp(in,&data);
}
string s_insert(string in,int offset,string by){
	offset=between(0,offset,in.len);
	if(!by.len) return in;
	int len=in.len;
	in=resize(in,in.len+by.len);
	memmove(in.str+offset+by.len,in.str+offset,len-offset);
	memcpy(in.str+offset,by.str,by.len);
	return in;
}
vector vec_dup(vector in){
	vector ret=_dup(in);
	for(int i=0; i<ret.len; i++){
		ret.var[i]=_dup(ret.var[i]);
	}
	return ret;
}
int s_ends(string big, string small){
	if(!small.len||!big.len||big.len<small.len) return 0;
	return c_eq(big.str+big.len-small.len,small.str,small.len);
}
int c_eq(char* in1,char* in2,int len){
	for(int i=0; i<len; i++)
		if(tolower(in1[i])!=tolower(in2[i])) return 0;
	return 1;
}
int eq(var in1, var in2){
	if(in1.len<=0) return in1.ptr==in2.ptr;
	if(in1.len!=in2.len) return 0;
	return c_eq(in1.str,in2.str,in1.len);
}
string c_dup(char* in){
	return s_new(in,in ? strlen(in) : 0);
}
string s_new(void* str,int len){
	string ret={0};
	ret=resize(ret,len);
	if(str && len) memcpy(ret.str,str,len);
	return ret;
}
void* mem_resize(void* in,int len,int oldlen){
	if(!len){
		if(in) free(in);
		return NULL;
	}
	if(!in) return memset(malloc(len),0,len);
	if(len<=oldlen) return in;
	void* ret=realloc(in,len);
	memset((char*)ret+oldlen,0,len-oldlen);
	return ret;
}
char* grow(string* in,int len){
	*in=resize(*in,in->len+len);
	return in->str+(in->len-len)*in->datasize;
}
string resize(string in,int len){
	int datasize=max(in.datasize,1);
	void* ret=in.readonly
		? memcpy(mem_resize(NULL,len*datasize,0),in.str,min(len,in.len)*datasize)
		: mem_resize(in.str,len*datasize,in.len*datasize);
	return (string){
		.ptr=ret,
		.len=len,
		.datasize=datasize,
	};
}
string cat(string str1,string str2){
	if(!str2.len) return str1;

	int datasize=max(str1.datasize,1);
	if(datasize!=max(str2.datasize,1)) die("cat called on unequal datasize");

	str2=_s(str2);
	if(str1.str+str1.len*datasize==str2.str){ //when string was previously split, ptrs are side by side
		str1.len+=str2.len;
		return str1;
	}
	void* ptr=grow(&str1,str2.len);
	memcpy(ptr,str2.str,str2.len*datasize);
	_free(&str2);
	return str1;
}
void* to_heap(void* in,size_t len){
	void* ret=mem_resize(NULL,len,0);
	memcpy(ret,in,len);
	return ret;
}
string trim_ex(string in, char* chars){
	char* ptr=in.ptr;
	char* end=in.ptr+in.len;
	while(ptr<end && strchr(chars,*ptr)) ptr++;
	while(end>ptr && strchr(chars,*(end-1))) end--;
	return cl_(ptr,end-ptr);
}
string trim(string in){
	return trim_ex(in," \t\n\r");
}
string s_rest(string main, string sub){
	if(!sub.str) return main;
	sub.str+=sub.len;
	sub.len=main.len-(sub.str-main.str);
	return sub;
}
void pair_sub(pair* in,int start,int len){
	if(!len) len=in->tail.len-start;
	in->tail.str+=start;
	in->tail.len-=len;
}
vector s_vec(string in,char* by){
	return s_vec_ex(in,by,0);
}
vector s_vec_ex(string in,char* by,int limit){
	int len=strlen(by);
	vector ret=vec_new();
	string temp;
	int from=0;
	for(int i=0; i<=in.len-len; i++){
		if(strncmp(in.str+i,by,len)==0){
			temp=i==from ? Null : slice(in,from,i-from);
			vec_add(&ret,temp);
			from=i+len;
			i+=len-1; //-1 as i++ will inc it in enclosing for-loop
			if(limit && ret.len>=limit-1) break;
		}
	}
	vec_add(&ret,slice(in,from,in.len));
	return ret;
}
string s_cs(string in){
	string ret=s_new(NULL,in.len+1);
	memcpy(ret.str,in.str,in.len);
	_free(&in); //added in 2023! without recursive fix
	return ret;
}
void s_catchar(string* in,char c){
	*grow(in,1)=c;
}
int s_int(string in){
	int ret=atoil(in);
	_free(&in);
	return ret;
}
string s_splice(string in, int offset, int len,string replace){
	return splice(in,offset,len,replace,NULL);
}
string s_del(string in, int offset, int len){
	return s_splice(in,offset,len,Null);
}
string sub(string in, int from, int len){
	if(!len){
		_free(&in);
		return Null;
	}
	part r=len_part(in.len,from,len);
	string ret=s_new(in.str+r.from,r.len);
	_free(&in);
	return ret;
}
string c_copy(char* in){
	return s_new(in,strlen(in));
}
string s_copy(string in){
	return s_new(in.str,in.len);
}
string head(string in, int len){
	return slice(in,0,len);
}
string tail(string in, int len){
	return slice(in,len,in.len-len);
}
string slice(string in, int from, int len){
	part r=len_part(in.len,from,len);
	return r.len ? (string){
		.str=in.str+r.from*max(in.datasize,1),
		.len=r.len,
		.datasize=in.datasize,
		.readonly=1,
	} : Null;
}
void msg(char* format,...){
	va_list args;
	va_start(args, format);
	vfprintf(stderr,format,args);
	fprintf(stderr,"\r\n");
	va_end(args);
}
string print_s(char* format,...){
	va_list args;
	va_start(args,format);
	string ret=vargs_s(format,args);
	va_end(args);
	return ret;
}
string vargs_s(char* format, va_list args){
	va_list copy;
	va_copy(copy,args);
	size_t len=vsnprintf(NULL,0,format,args);
	string ret=s_new(NULL,len+1);
	ret.len--;
	vsnprintf(ret.str,ret.len+1,format,copy);
	va_end(copy);
	return ret;
}
int s_caseeq(string str1,string str2){
	if(str1.len!=str2.len) return 0;	
	for(int i=0; i<str1.len; i++)
		if(str1.str[i]!=str2.str[i]) return 0;
	return 1;
}
int eq_c(string str1,char* str2){
	if(!str2) return 0;
	int len=strlen(str2);
	if(len!=str1.len) return 0;
	return c_eq(str2,str1.str,len);
}

string s_lower(string in){
	for(int i=0; i<in.len; i++){
		if(in.str[i]>='A' && in.str[i]<='Z') in.str[i]+=('a'-'A');
	}
	return in;
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
vector vec_cut(vector* in,int from, int len){
	vector ret=vec_new_ex(in->datasize,0);
	part r=len_part(in->len,from,len);
	if(!r.len) return ret;
	if(!r.from && r.len==in->len){
		ret=*in;
		*in=vec_new_ex(in->datasize,0);
		return ret;
	}
	vec_add_ex(&ret,r.len,vec_p(*in,r.from));
	vec_del_ex(in,r.from,r.len,NULL);
	return ret;
}
pair cut_any(string in,char* any){
	pair ret={.head=in};
	int i=0;
	while(i<in.len && !strchr(any,in.str[i])) i++;
	ret.head.len=i;
	while(i<in.len && strchr(any,in.str[i])) i++;
	ret.tail=slice(in,i,in.len);
	return ret;
}
pair cut(string in,char* by){
	int len=strlen(by);
	pair ret={0};
	for(int i=0; i<=in.len-len; i++){
		if(strncmp(in.str+i,by,len)!=0) continue;
		ret.head=slice(in,0,i);
		ret.tail=slice(in,i+len,in.len);
		return ret;
	}
	ret.head=in;
	return ret;
}
string cutp(string* in,char* by){
	pair ret=cut(*in,by);
	*in=ret.tail;
	return ret.head;
}
var end_new(){
	return (var){.len=IsEnd};
}
void buff_add(pair* in,var add){
	if(!in->tail.len) return;
	int len=_c(add,in->tail.str);
	if(len>in->tail.len) die("mem corrupted");
	in->tail.str+=len;
	in->tail.len-=len;
	_free(&add);
}
pair buff_new(int len){
	string s=s_new(NULL,len);
	return (pair){.head=s,.tail=s};
}
string _s(var in){
	if(in.len>=0) return in;
	string ret=s_new(NULL,_c(in,NULL));
	_c(in,ret.str);
	return ret;
}
string cl_(const void* in,int len){
	return in ? (string){
		.str=(void*)in,
		.len=len,
		.datasize=1,
		.readonly=1,
	} : Null;
}
string c_(const char* in){
	return cl_(in,in ? strlen(in) : 0);
}
int _c(var in,char* out){
	if(in.len>0){
		if(out) memcpy(out,in.str,in.len);
		return in.len;
	}
	switch(in.len){
		case IsPtr : if(out) memcpy(out,"<ptr>",6); return 5;
		case IsEnd : if(out) memcpy(out,"<ptr>",6); return 5;
		case IsError : if(out) memcpy(out,"<ptr>",6); return 5;
		case IsInt :
			int len=snprintf(0,0,"%d",in.i);
			if(!out) return len;
			char buff[32];
			snprintf(buff,sizeof(buff),"%d",in.i);
			memcpy(out,buff,len);
			return len;
		case IsDouble :
			len=snprintf(0,0,"%g",in.f);
			if(!out) return len;
			snprintf(buff,sizeof(buff),"%g",in.f);
			memcpy(out,buff,len);
			return len;
		default : return 0;
	}
}
string vec_s(vector in,char* sep){
	if(!in.len) return Null;
	string sep1=c_(sep);
	int len=((in.len-1)*sep1.len)+vec_strlen(in);
	pair ret=buff_new(len);
	for(int i=0; i<in.len ; i++){
		if(i) buff_add(&ret,sep1);
		buff_add(&ret,ro(in.var[i]));
	}
	vec_free(&in);
	return ret.head;
}
int vec_strlen(vector in){
	int ret=0;
	for(int i=0; i<in.len; i++) ret+=_c(in.var[i],NULL);
	return ret;
}
string cat_c(string in,char* add){
	return cat(in,c_(add));
}
int _len(var in){
	return _c(in,NULL);
}
part s_part(string in){
	return (part){
		.from=0,
		.len=in.len
	};
}
part s_next(string in,part r,char* sep){
	int len=strlen(sep);
	for(int i=r.next; i<in.len; i++){
		if(strncmp(in.str+i,sep,len)==0){
			r.len=i-r.next;
			r.from=r.next;
			r.next=i+len;
			return r;
		}
	}
	if(r.next<in.len){
		r.len=in.len-r.next;
		r.from=r.next;
		r.next=in.len;
		return r;
	}
	if(r.from+r.len<r.next){
		r.from=r.next;
		r.len=0;
		return r;
	}
	return (part){0};
}
void out(char* fmt,...){
	va_list args;
	va_start(args,fmt);
	string s=format_vargs(fmt,args);
	va_end(args);
	printf("%.*s\n",s.len,s.str);
	_free(&s);
}
string format(char* fmt,...){
	va_list args;
	va_start(args,fmt);
	string ret=format_vargs(fmt,args);
	va_end(args);
	return ret;
}
string c_repeat(char* in, int times){
	int len=strlen(in);
	string ret=s_new(NULL,len*times);
	for(int i=0; i<times; i++){
		memcpy(ret.str+i*times,in,len);
	}
	return ret;
}
string format_vargs(char* fmt,va_list args){
	part r={0};
	int total=0;
	var vfmt=c_(fmt);
	while((r=s_next(vfmt,r,"{}")).next) total++;
	total-=1;

	va_list copy;
	va_copy(copy,args);

	int len=0;
	for(int i=0; i<total; i++){
		var sub=va_arg(args,var);
		len+=_len(sub);
	}
	pair ret=buff_new(strlen(fmt)-total*2+len);

	r=(part){0};
	for(int i=0; i<total; i++){
		r=s_next(vfmt,r,"{}");
		if(r.len) buff_add(&ret,slice(vfmt,r.from,r.len));
		var sub=va_arg(copy,var);
		buff_add(&ret,sub);
	}

	r=s_next(vfmt,r,"{}");
	if(r.len) buff_add(&ret,slice(vfmt,r.from,r.len));

	va_end(copy);
	return ret.head;
}
vector _vec_append(string ret, ...){
	va_list args;
	va_start(args,ret);
	while(1){
		var val=va_arg(args,var);
		if(val.len==IsEnd) break;
		vec_add(&ret,val);
	}
	va_end(args);
	return ret;
}
vector _vec_all(string in,...){
	vector ret=vec_new();
	va_list args;
	va_start(args,in);
	var val=in;
	while(1){
		if(val.len==IsEnd) break;
		vec_add(&ret,val);
		val=va_arg(args,var);
	}
	va_end(args);
	return ret;
}
string _cat_all(string in,...){
	va_list args;
	va_list args2;
	va_start(args,in);
	va_copy(args2,args);
	int len=_len(in);
	while(1){
		var sub=va_arg(args,var);
		if(sub.len==IsEnd) break;
		len+=_len(sub);
	}
	pair ret=buff_new(len);
	buff_add(&ret,in);
	while(1){
		var sub=va_arg(args2,var);
		if(sub.len==IsEnd) break;
		buff_add(&ret,sub);
	}
	va_end(args2);
	va_end(args);
	return ret.head;
}
string s_dequote(string in,char* qchars){
	if(in.len<2) return in;
	if(strchr(qchars,in.str[0])) return slice(in,1,-1);
	return in;
}
string s_unescape_ex(string in,char* find, char* replace){
	int extra=0;
	for(int i=0; i<in.len-1; i++){
		if(in.str[i]!='\\') continue;
		extra++;
		i++;
	}
	if(!extra) return in;
	string ret=s_new(NULL,in.len-extra);
	int offset=0;
	for(int i=0; i<in.len; i++){
		char c=in.str[i];
		if(i>=in.len-1 || c!='\\'){
			ret.str[offset++]=c;
			continue;
		}
		i++;
		char* found=strchr(replace,c);
		ret.str[offset++]==*found ? find[found-replace] : c;
	}
	_free(&in);
	return ret;
}
vector vec_escape(const vector in){
	vector ret=vec_new();
	ret=resize(ret,in.len);
	for(int i=0; i<in.len; i++){
		ret.var[i]=s_escape(ro(in.var[i]));
	}
	return ret;
}
string s_unescape(string in){
	return s_unescape_ex(in,"\0\n\r\t\\\033","0nrt\\e");
}
string s_escape(string in){
	return s_escape_ex(in,"\0\n\r\t\\\033","0nrt\\e");
}
string s_escape_ex(string in,char* find,char* replace){
	int extra=0;
	for(int i=0; i<in.len ; i++)
		if(strchr(find,in.str[i]))
			extra++;
	if(!extra) return in;
	string ret=s_new(NULL,in.len+extra);
	int offset=0;
	for(int i=0; i<in.len ; i++){
		char c=in.str[i];
		char* found=strchr(find,c);
		if(found){
			ret.str[offset++]='\\';
			ret.str[offset++]=replace[found-find];
		}
		else
			ret.str[offset++]=c;
	}
	_free(&in);
	return ret;
}
int s_chr(string in,char has){
	for(int i=0; i<in.len; i++)
		if(in.str[i]==has) return i+1;
	return 0;
}
char* s_has(string in,char* has){
	int sublen=strlen(has);
	for(int i=0; i<=in.len-sublen; i++)
		if(c_eq(in.str+i,has,sublen))
			return in.str+i;
	return NULL;
}
string replace(string in,char* find,char* replace){
	string ret={0};
	pair sub=cut(in,find);
	while(sub.head.len!=in.len){
		ret=cat_all(ret,sub.head,c_(replace));
		in=sub.tail;
		sub=cut(in,find);
	}
	ret=cat(ret,sub.head);
	return ret;
}
void trace(char* format,...){
#ifdef TRACE
	va_list args;
	va_start(args, format);
	vfprintf(stderr,format,args);
	va_end(args);
	fprintf(stderr,"//\n");
#endif
}
var _dup(var in){
	var ret=in;
	ret.readonly=0;
	ret.datasize=max(ret.datasize,1);
	ret.ptr=memcpy(mem_resize(NULL,ret.len*ret.datasize,0),in.str,ret.len*ret.datasize);
	return ret;
}
int is_error(var in){
	return in.len==IsError;
}
int is_char(char c,int alpha,int num,char* extra){
	if(alpha && (c>='a' && c<='z' || c>='A' && c<='Z')) return 1;
	if(num && c>='0' && c<='9') return 1;
	if(extra && strchr(extra,c)) return 1;
	return 0;
}
int is_alpha_char(int c){
	return is_char(c,1,0,NULL);
}
int is_word_char(int c){
	return is_char(c,1,1,"_");
}
int is_numeric_char(int c){
	return is_char(c,0,1,"-+.");
}
int is_word(string word,char* list){
	char* str=list;
	char* end=list+strlen(list)-word.len;
	while(str<=end){
		if(memcmp(str,word.str,word.len)==0 && (str==end || *(str+word.len)==' ')) return 1;
		while(str<end && *str!=' ') str++;
		str++;
	}
	return 0;
}
void die(char* msg){
	fprintf(stderr,"%s",msg);
	exit(-1);
}
