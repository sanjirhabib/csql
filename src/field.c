#include "var.h"
#include "cross.h"
#include "code.h"
#include "log.h"
#include "field.h"

/*header
typedef struct s_field {
	string name;
	string type;
	string title;
	string create;
	string meta;
	void* mem;
	int size;
	int pkey;
	int unique;
	int index;
	int align;
} field;
typedef enum {
	WinTop, WinBottom, WinLeft, WinRight, WinCenter
} Side;
end*/


int is_date(string in){
	if(in.len!=10) return 0;
	if(in.str[4]!='-'||in.str[7]!='-') return 0;
	if(!is_numeric(sub(in,0,4))) return 0;
	return 1;
}
string val_type(string in){
	if(is_date(in)) return c_("date");
	if(is_numeric(in)) return c_("int");
	if(is_word(in,"debit credit")) return c_("currency");
	return c_("text");
}
string vals_type(vector in,string name){
	string curr=Null;
	int no=0;
	each(in,i,var* val){
		if(!val[i].len) continue;

		string type=val_type(val[i]);

		if(!curr.len) curr=type;
		else if(eq(curr,val[i])) return type;	
		else if(no>=2) return type;
		no++;
	}
	return curr.len ? curr : c_("text");
}
map cross_fields(cross in,cross types){
	vector vals=vec_new(sizeof(field), in.rows.keys.len);
	field* p=vals.ptr;
	each(in.rows.keys,i,var* name){
		if(i==in.rows.keys.len-2) break;
		field fld={
			.name=name[i],
			.type=vals_type(cross_col(in,i,Null),name[i]),
			.pkey=i ? 0 : 1,
			.title=name_title(name[i])
		};
		p[i]=fld;
	}
	vector keys=sub(in.rows.keys,0,-1);
	return (map){
		.keys=keys,
		.vals=vals,
		.index=keys_index(keys),
	};
}
map s_fields(string in){
	vector lines=s_vec(in,"\n");
	vector vals=vec_new(sizeof(field),0);
	vector keys=NullVec;
	each(lines,i,var* ln){
		field f=s_field(ln[i]);

		if(!f.title.len) f.title=name_title(f.name);
		if(!f.type.len) f.type=c_("code");

		vec_addp(&vals,&f);
		vec_add(&keys,f.name);
	}
	_free(&lines);
	_free(&in);
	return (map){
		.keys=keys,
		.vals=vals,
		.index=keys_index(keys),
	};
}
field s_field(string in){
	vector vals=code_split(in," \t\n\r",0);
	field ret={
		.type=c_("code"),
	};
	each(vals,i,var* v){
		string s=v[i];
		if(!i){
			ret.name=s;
			continue;
		}
		string val=get(vals,i+1);
		if(eq_c(s,"type"))
			ret.type=val;
		else if(eq_c(s,"pkey"))
			ret.pkey=_i(val);
		else if(eq_c(s,"unique"))
			ret.unique=_i(val);
		else if(eq_c(s,"index"))
			ret.index=_i(val);
		else if(eq_c(s,"align"))
			ret.index=eq_c(val,"right") ? WinRight : WinLeft;
		else if(eq_c(s,"title"))
			ret.title=val;
		else{
			ret.meta=s_join(ret.meta," ",val);
		}
	}
	_free(&vals);
	_free(&in);
	return ret;
}


field vec_field(vector in){
	return (field){
		.name=in.var[0],
		.type=in.var[1],
		.create=in.var[2],
		.pkey=_i(in.var[3]),
		.unique=_i(in.var[4]),
		.index=_i(in.var[5]),
		.meta=in.var[6],
	};
}
vector field_vec(field* f){
	return vec_all(
		f->name,
		f->type,
		f->create,
		i_s(f->pkey),
		i_s(f->unique),
		i_s(f->index),
		NullStr
	);
}
void field_dump(field* f){
	msg("{\n\tname: %.*s", ls(f->name));
	msg("\ttype: %.*s", ls(f->type));
	msg("\tcreate: %.*s", ls(f->create));
	msg("\tpkey: %d", f->pkey);
	msg("\tunique: %d", f->unique);
	msg("\tindex: %d", f->index);
	msg("}");
}
map structure_fields(){
	field colmeta[]={
		{c_("name"), c_("code"),c_("Name"),{0},{0},NULL,50,1,0,0},
		{c_("type"), c_("int"),c_("Type"),{0},{0},NULL,0,0,0,1},
		{c_("create"), c_("int"),c_("Create"),{0},{0},NULL,0,0,0,0},
		{c_("pkey"), c_("bool"),c_("PKey"),{0},{0},NULL,0,0,0,1},
		{c_("unique"), c_("bool"),c_("Unique"),{0},{0},NULL,0,0,0,1},
		{c_("index"), c_("bool"),c_("Index"),{0},{0},NULL,0,0,0,1},
		{c_("meta"), c_("int"),c_("Meta"),{0},{0},NULL,0,0,0,0},
	};
	vector vals=vec_new(sizeof(field), 0);
	vec_add_ex(&vals, sizeof(colmeta)/sizeof(field),colmeta);
	return vec_map(s_vec(c_("name type create pkey unique index meta"), " "),vals);
}
map fields_rows(map cols){
	vector vals=NullVec;
	map_each(cols,i,field* f){
		vals=vec_append(vals,
		_dup(f[i].name),
		_dup(f[i].type),
		_dup(f[i].create),
		i_s(f[i].pkey),
		i_s(f[i].unique),
		i_s(f[i].index),
		NullStr
		);
	}
	return vec_map(s_vec(c_("name type create pkey unique index meta"), " "),vals);
}
string name_title(string in){
	string ret=s_new(in.str,in.len);
	int capitalize=1;
	for(int i=0; i<ret.len; i++){
		if(capitalize) ret.str[i]=toupper(ret.str[i]);
		capitalize=0;
		if(ret.str[i]=='_'){
			ret.str[i]=' ';
			capitalize=1;
		}
	}
	return ret;
}

void field_free(field* in){
	_free(&in->name);
	_free(&in->type);
	_free(&in->title);
	_free(&in->create);
	if(in->mem) free(in->mem);
	in->mem=NULL;
}
string meta_type(string type,int size,string name){
	if(size==36) return c_("guid");
	if(size==7 && eq_c(name,"month")) return c_("month");
	if(eq_c(type,"date")) return c_("date");
	if(eq_c(name,"year")) return c_("year");
	if(eq_c(type,"tinyint")) return c_("bool");
	if(size==1 && eq_c(type,"int")) return c_("bool");
	if(eq_c(type,"clob")) return c_("para");
	if(size>255 && eq_c(type,"text")) return c_("para");
	if(is_word(type,"mediumblob longblob bytea")) return c_("blob");
	if(is_word(type,"string varchar2 varchar char")){
		if(size>2048) return c_("page");
		if(size>255) return c_("para");
		if(size>50) return c_("text");
	}
	if(is_word(name,"debit credit")) return c_("currency");
	if(is_word(type,"varchar2 varchar char")) return c_("code");
	if(eq_c(type,"numeric")) return c_("currency");
	if(is_word(type,"number int int4 integer")) return c_("int");
	if(is_word(type,"real double float float4")) return c_("float");
	if(eq_c(type,"timestamp")) return c_("datetime");
	return c_("text");
}
string field_type(field* fld,cross types){
	vector creates=cross_col(types,0,c_("create"));
	for(int i=0; i<creates.len; i++){
		assert(types.vals.keys.var[i].datasize);
		if(eq(fld->create,creates.var[i])) return types.vals.keys.var[i];
	}
	return NullStr;
}
int is_numeric(string in){
	if(!in.len) return 0;
	for(int i=0; i<in.len; i++){
		if(!is_numeric_char(in.str[i])) return 0;
	}
	return 1;
}
