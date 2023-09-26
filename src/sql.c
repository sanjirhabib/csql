#include "field.h"
#include "code.h"
#include "cross.h"
#include "sql.h"

/*header

typedef struct s_sqlcls {
	string pre;
	string select;
	string from;
	string where;
	string group;
	string window;
	string order;
	string limit;
} sqlcls;

end*/

vector cols_pkeys(map cols){
	vector ret=vec_new();
	for(int i=0; i<cols.keys.len; i++){
		field f=*(field*)vec_p(cols.vals,i);
		if(f.pkey){
			vec_add(&ret,get(cols.keys,i));
		}
	}
	return ret;
}
map sql_cols(const string in, cross types){
	vector toks=code_split(in,";",0);
	if(!toks.len) return (map){0};
	vector toks2=code_split(toks.var[0]," \t\n\r",0);
	vector toks3=vec_trim(code_split(slice(vec_start(toks2,"("),1,-1),",",0));
	map ret=map_new_ex(sizeof(field));
	for(int i=0; i<toks3.len; i++){
		string line=get(toks3,i);
		if(!line.len) continue;
		pair temp=cut_any(line," \t\n\r");
		if(!is_word(s_lower(temp.head),"primary unique foreign constraint check")){
			temp.head=s_dequote(temp.head,"[\"'`");
			field fld={
				.name=temp.head,
				.title=name_title(temp.head),
				.create=temp.tail,
			};
			map_add_ex(&ret,temp.head,&fld,field_free);
			continue;
		}
		vector toks4=code_split(line," \t\n\r([{",0);
		vector cols=vec_trim(code_split(slice(vec_start(toks4,"("),1,-1),",",0));
		if(eq_c(toks4.var[0],"primary")){
			for(int i=0; i<cols.len; i++) ((field*)map_getp(ret,get(cols,i)))->pkey=1;
		}
		else if(eq_c(toks4.var[0],"unique")){
			for(int i=0; i<cols.len; i++) ((field*)map_getp(ret,get(cols,i)))->unique=1;
		}
		vec_free(&cols);
		vec_free(&toks4);
	}
	for(int i=0; i<ret.keys.len; i++){
		field* fld=getp(ret.vals,i);
		fld->type=field_type(fld,types);
	}
	vec_free(&toks);
	vec_free(&toks2);
	vec_free(&toks3);
	//dx(ret.keys);
	return ret;
}

sqlcls sql_cls(string sql){
	vector toks=code_split(sql," \t\n\r",0);
	vector cls=vec_split(toks,"select from where group window order limit");
	sqlcls ret={0};
	for(int i=0; i<cls.len; i++){
		vector v=cls.var[i];
		if(eq_c(v.var[0],"select")) ret.select=vec_s(slice(v,1,v.len)," ");
		else if(eq_c(v.var[0],"from")) ret.from=vec_s(slice(v,1,v.len)," ");
		else if(eq_c(v.var[0],"where")) ret.where=vec_s(slice(v,1,v.len)," ");
		else if(eq_c(v.var[0],"group")) ret.group=vec_s(slice(v,2,v.len)," ");
		else if(eq_c(v.var[0],"window")) ret.window=vec_s(slice(v,1,v.len)," ");
		else if(eq_c(v.var[0],"order")) ret.order=vec_s(slice(v,2,v.len)," ");
		else if(eq_c(v.var[0],"limit")) ret.limit=vec_s(slice(v,1,v.len)," ");
		else ret.pre=vec_s(v," ");
	}
	vec_free(&toks);
	vec_free(&cls);
	_free(&sql);
	return ret;
}
string cls_sql(sqlcls in){
	string ret={0};
	if(in.pre.len) ret=cat(ret,in.pre);
	if(in.select.len) ret=cat_all(ret,c_(" select "),in.select);
	if(in.from.len) ret=cat_all(ret,c_(" from "),in.from);
	if(in.where.len) ret=cat_all(ret,c_(" where "),in.where);
	if(in.group.len) ret=cat_all(ret,c_(" group by "),in.group);
	if(in.window.len) ret=cat_all(ret,c_(" window "),in.window);
	if(in.order.len) ret=cat_all(ret,c_(" order by "),in.order);
	if(in.limit.len) ret=cat_all(ret,c_(" limit "),in.limit);
	return ret;
}
map filter_params(const map where){
	map ret=map_new();
	for(int i=0; i<where.keys.len; i++){
		if(!where.vals.var[i].len) continue;
		map_add(&ret,ro(where.keys.var[i]),ro(where.vals.var[i]));
	}
	return ret;
}
string sql_filter(string sql,vector cols){
	sqlcls cls=sql_cls(sql);
	for(int i=0; i<cols.len; i++){
		cls.where=s_join(cls.where," and ",print_s("%.*s=:%.*s",ls(cols.var[i]),ls(cols.var[i])));
	}
	return cls_sql(cls);
}
string sql_limit(string sql,int from, int len){
	vector toks=code_split(sql," \t\n\r",0);
	int at=vec_words(toks,"limit");
	if(at>=0){
		string temp=vec_s(vec_del_ex(&toks,at,toks.len,_free)," ");
		_free(&sql);
		sql=temp;
	}
	else vec_free(&toks);
	return format("{} limit {},{}",sql,i_s(from),i_s(len));
}
vector sql_split(string in){
	vector toks=code_split(in,";",0);
	string full=Null;
	vector ret=NullVec;
	int level=0;
	each(toks,i,var* tok){
		vector toks2=code_split(trim(tok[i])," \t\n\r",0);
		level+=vec_count(toks2,"begin");
		level-=vec_count(toks2,"end");
		_free(&toks2);
		if(!full.len) full=tok[i];
		else full.len=tok[i].str+tok[i].len-full.str;
		if(!level){
			if(trim(full).len) vec_add(&ret,full);
			full=Null;
		}
		
	}
	_free(&toks);
	if(trim(full).len) vec_add(&ret,full);
	return ret;
}
