#include "var.h"
#include "csql.h"

#define lib_error(x) _lib_error(x,__FILE__,__LINE__)
#define lib_warn(x) _lib_warn(x,__FILE__,__LINE__)
#define app_warn(x) _app_warn(x,__FILE__,__LINE__)
#define error(x) _error(x,__LINE__,__FILE__,__FUNCTION__)

int msg_timer;
vector msg_queue;
vector log_lines;

int vis_error(string in,window win){
	vis_log(in,win);
	getchar();
	return 0;
}
int vis_log(string in,window win){
	log_add(in);
	log_show(win);
	return 0;
}
int log_add(string in){
	log_lines=cat(log_lines,vec_own(s_vec(ro(in),"\n")));
	if(log_lines.len>200) vec_del_ex(&log_lines,0,log_lines.len-200,_free);
	return 0;
}
int log_show(window win){
	editor e=editor_new(win);
	e.lines=ro(log_lines);
	e.curr.y=log_lines.len;
	editor_view(&e);
	editor_show(&e,win);
	editor_free(&e);
	return 0;
}
int vis_show(){
	var s={0};
	if(msg_timer>0 && msg_timer<2){
		msg_timer++;
		return 0;
	}
	else if(msg_queue.len){
		msg_timer=1;
		s=msg_queue.var[0];
		msg_queue=splice(msg_queue,0,1,Null,NULL);
	}
	else if(msg_timer>4) msg_timer=0;
	else if(!msg_timer) return 0;
	else{
		msg_timer++;
		return 0;
	}
	vis_print(VisSavePos);
	vis_goto(1, 0);
	vis_print(VisClearLine);
	s_out(s);
	vis_print(VisRestorePos);
	fflush(stdout);
	return 0;
}
int vis_msg(char* format, ...){
	if(msg_queue.len>10) return 0;
	va_list args;
	va_start(args,  format);
	string msg=vargs_s(format, args);
	va_end(args);
	vec_add(&msg_queue,msg);
	vis_show();
	return 0;
}
var dump(var in){
	_dump(in,0);
	return in;
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