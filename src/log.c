#include <assert.h>
#include <errno.h>

#include "map.h"
#include "log.h"

#define lib_error(x) _lib_error(x,__FILE__,__LINE__)
#define lib_warn(x) _lib_warn(x,__FILE__,__LINE__)
#define app_warn(x) _app_warn(x,__FILE__,__LINE__)
#define error(x) _error(x,__LINE__,__FILE__,__FUNCTION__)

int msg_timer;
vector msg_queue;

/*header
struct s_log {
	vector lines;
	vector types;
	int has_new;
};
extern struct s_log logs;

typedef enum {LogInfo, LogWarning, LogError} LogLevel;

end*/

struct s_log logs={0};

vector ring_add(vector in,vector add,int size){
	if(in.len+add.len<size) return cat(in,add);
	if(add.len>=size) return sub(add,add.len-size,size);
	if(in.len<size) in=resize(in,size);
	assert(in.datasize==add.datasize);
	memmove(in.str+add.len*add.datasize,in.str,(in.len-add.len)*in.datasize);
	memcpy(in.str,add.str,add.len*add.datasize);
	_free(&add);
	return in;
}

int log_error(string in){
	log_add(in,LogError);
}
int log_add(string in,LogLevel type){
	vector add=vec_own(s_vec(ro(in),"\n"));
	logs.lines=ring_add(logs.lines,add,200);
	vector types=vec_new(sizeof(int),add.len);
	each(types,i,int* v) v[i]=type;
	logs.types=ring_add(logs.types,types,200);
	_free(&in);
	if(type==LogError) logs.has_new=1;
	return 0;
}
var dump(var in){
	_dump(in,0);
	return in;
}
void os_log(string in){
	log_error(print_s("%.*s: %s", ls(in), strerror(errno)));
	_free(&in);
}
void _dump(var in,int level){
	if(in.datasize>1){
		if(in.datasize!=sizeof(var)){
			msg("%*s[ len: %d, datasize: %d, readonly: %d ]",level*4,"",in.len,in.datasize,in.readonly);
			return;
		}
		msg("%*s[",level*4,"");
		for(int i=0; i<in.len; i++){
			_dump(get(in,i),level+1);
		}
		msg("%*s]",level*4,"");
	}
	else if(in.len>0){
		string temp=s_escape(ro(in));
		msg("%*s\"%.*s\"",level*4,"",ls(temp));
		_free(&temp);
	}
	else if(!in.len){
		msg("%*s<blank>",level*4,"");
	}
}
void dx(var in){
	dump(in);
	fflush(stdout);
	raise(SIGINT);
}
int _error(char* msg,int line, char* file,const char* func){
	printf("error: %s in %s():%d in %s\n",msg,func,line,file);
	exit(-1);
	return 0;
}
int app_error(char* msg,...){
	fprintf(stderr,"ERROR: ");
	va_list args;
	va_start(args, msg);
	vfprintf(stderr,msg,args);
	va_end(args);
	fprintf(stderr,"\n");
	return -1;
}
int _app_warn(char* msg,char* file,int line){
	printf("WARNING: %s in %s line %d\n",msg,file,line);
	exit(-1);
}
int _lib_error(char* msg,char* file,int line){
	printf("ERROR: %s in %s line %d\n",msg,file,line);
	raise(SIGINT);
	exit(-1);
}
int _lib_warn(char* msg,char* file,int line){
	printf("WARNING: %s in %s line %d\n",msg,file,line);
}
void vec_print(vector in){
	vec_print_ex(in,p_print);
}
void vec_print_ex(vector in,void* callback){
	printf("[\n");
	for(int i=0; i<in.len; i++){
		printf("\t");
		((void(*)(void* data))callback)(in.str+in.datasize*i);
		if(i<in.len-1) printf(",\n");
	}
	printf("\n]\n");
}
void s_print(string in,int own){
	if(!in.len) return;
	printf("%.*s",in.len,in.str);
	if(own) _free(&in);
}
void map_dump(map in){
	if(!in.keys.len){
		msg("{NullMap}");
		return;
	}
	for(int j=0; j<in.vals.len/in.keys.len; j++){
		msg("{");
		for(int i=0; i<in.keys.len; i++){
			fprintf(stderr,"\t%.*s: ",ls(in.keys.var[i]));
			dump(in.vals.var[j*in.keys.len+i]);
		}
		msg("}");
	}
}
void println(var in){
	print(in);
	printf("\n");
}
void pi_print(int* in){
	printf("%d",*in);
}
void p_print(var* in){
	if(in) print(*in);
}
void print(var in){
	if(in.len>0){
		printf("%.*s",ls(in));
	}
	else if(in.datasize>1){
		vec_print(*(vector*)in.ptr);
	}
	else{
		switch(in.len){
			case IsInt : printf("%d",in.i); break;
			case IsDouble : printf("%f",in.f); break;
			case IsPtr : printf("%p",in.ptr); break;
			case IsNull : printf("NULL"); break;
			//case IsMap : map_print(*(map*)in.ptr); break;
		}
	}
}
void map_print(map in){
	map_print_ex(in,p_print);
}
void map_print_ex(map in,void* callback){
	printf("Map:{\n",in.keys.len);
	for(int i=0; i<in.keys.len; i++){
		p_print(vec_p(in.keys,i));
		printf(": ");
		((void(*)(void*))callback)(vec_p(in.vals,i));
		if(i<in.keys.len-1) printf(",\n");
	}
	printf("\n}\n");
}
void vec_check(vector in){
	each(in,i,var* v){
		assert(v[i].datasize);
	}
}
void log_print(){
	if(!logs.has_new) return;
	each(logs.lines,i,string* s){
		msg("%.*s",ls(s[i]));
	}
	logs.has_new=0;
}
void msg(char* format,...){
	va_list args;
	va_start(args, format);
	vfprintf(stderr,format,args);
	fprintf(stderr,"\n");
	va_end(args);
}
