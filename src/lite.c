#include <sqlite3.h>
#include "var.h"
#include "sql.h"
#include "log.h"
#include "lite.h"


var lite_conn(string db){
	var ret={.len=IsPtr};
	string name=s_c(db);
	if(sqlite3_open_v2(name.ptr,(sqlite3**)&ret.ptr,SQLITE_OPEN_READWRITE|SQLITE_OPEN_URI,NULL)!=SQLITE_OK){
		_free(&name);
		lite_print_error(ret,(string){0},(map){0});
		return ret;
	}
	_free(&name);
	return ret;
}
int lite_error(var conn){
	sqlite3_errcode(conn.ptr)!=SQLITE_DONE;
}
string lite_msg(var conn){
	return lite_error(conn) ? c_((char*)sqlite3_errmsg(conn.ptr)) : (string){0};
}
map lite_print_error(var conn,string sql,map params){
	log_error(c_(sqlite3_errmsg(conn.ptr)));
	_free(&sql);
	map_free(&params);
	return (map){0};
}
map lite_exec(var conn,var sql,map params){
	sqlite3_stmt* stm=NULL;
	if(sqlite3_prepare_v2(conn.ptr,sql.ptr,sql.len,&stm,NULL)!=SQLITE_OK)
		return lite_print_error(conn,sql,params);
	// bind parameters
	for(int i=0; i<params.keys.len; i++){
		string temp=s_c(cat(c_(":"),get(params.keys,i)));
		int idx=sqlite3_bind_parameter_index(stm,temp.str);
		_free(&temp);
		if(!idx) continue;
		sqlite3_bind_text(stm,idx,params.vals.var[i].ptr ? params.vals.var[i].ptr : "",params.vals.var[i].len,NULL);
	}
	vector keys=NullVec;
	int ncols=sqlite3_column_count(stm);
	for(int i=0; i<ncols; i++){
		char* temp=(char*)sqlite3_column_name(stm,i);
		vec_add(&keys,c_dup(temp));
	}
	vector vals=NullVec;
	int step=0;
	while((step=sqlite3_step(stm))){
		if(step==SQLITE_ROW){
			for(int i=0;i<ncols;i++){
				char* temp=(char*)sqlite3_column_text(stm,i);
				vec_add(&vals,c_dup(temp));
			}
		}
		else if(step==SQLITE_BUSY) continue;
		else if(step==SQLITE_DONE) break;
		else{
			sqlite3_finalize(stm);
			return lite_print_error(conn,sql,params);
		}
	}
	if(sqlite3_finalize(stm)!=SQLITE_OK) return lite_print_error(conn,sql,params);
	_free(&sql);
	map_free(&params);
	return (map){
		.keys=keys,
		.vals=vals,
		.index=keys_index(keys),
	};
}	
int lite_close(var conn){
	sqlite3_close_v2(conn.ptr);
	return 0;
}
var lite_val(var conn,string sql,map params){
	map rows=lite_exec(conn,sql,params);
	var ret=Null;
	if(rows.vals.len) ret=own(getp(rows.vals,0));
	map_free(&rows);
	return ret;
}
var lite_delete(var conn,var table,map pkeys){
	var keys={0};

	map params=map_new();
	for(int i=0; i<pkeys.keys.len; i++){
		var key=get(pkeys.keys,i);
		keys=s_join(keys," and ",format("{}=:{}",key,key));
		map_add(&params,key,get(pkeys.vals,i));
	}

	var sql=format("delete from {} where {}",table,keys);
	lite_exec(conn,sql,params);
	map_free(&pkeys);
	_free(&table);
	return lite_msg(conn);
}
var lite_insert(var conn,var table,map vals){
	var sql=format("insert into {} ({}) values (:{})",table,vec_s(ro(vals.keys),", "),vec_s(ro(vals.keys),", :"));
	lite_exec(conn,sql,vals);
	return lite_msg(conn);
}
var lite_update(var conn,var table,map pkeys,map vals){
	var keys={0};
	var cols={0};

	map params=map_new();
	for(int i=0; i<pkeys.keys.len; i++){
		var key=get(pkeys.keys,i);
		keys=s_join(keys," and ",format("{}=:old_{}",key,key));
		map_add(&params,cat(c_("old_"),key),get(pkeys.vals,i));
	}
	for(int i=0; i<vals.keys.len; i++){
		var key=get(vals.keys,i);
		cols=s_join(cols,", ",format("{}=:{}",key,key));
		map_add(&params,key,get(vals.vals,i));
	}

	var sql=format("update {} set {} where {}",table,cols,keys);
	lite_exec(conn,sql,params);
	map_free(&pkeys);
	map_free(&vals);
	_free(&table);
	return lite_msg(conn);
}
string lite_tablesql(var conn, string table){
	return lite_val(conn,c_("select sql from sqlite_schema where type='table' and name=:name"),map_all(c_("name"),ro(table)));
}
map table_fields(var conn,string table,cross types){
	string sql=lite_tablesql(conn,table);
	map cols=sql_cols(sql,types);
	if(!cols.keys.len) return cols;
	((field*)getp(cols.vals,0))->mem=sql.ptr;

	map rows=lite_exec(conn,c_("select substr(sql,instr(sql,'(')+1,instr(sql,')')-instr(sql,'(')-1) from sqlite_master where type='index' and tbl_name=:tbl and sql like '%UNIQUE%'"),map_all(c_("tbl"),table));
	for(int i=0; i<rows.vals.len; i++){
		field* p=map_getp(cols,get(rows.vals,i));
		if(p) p->unique=1;
	}
	map_free(&rows);
	rows=lite_exec(conn,c_("select substr(sql,instr(sql,'(')+1,instr(sql,')')-instr(sql,'(')-1) from sqlite_master where type='index' and tbl_name=:tbl and sql not like '%UNIQUE%'"),map_all(c_("tbl"),table));
	for(int i=0; i<rows.vals.len; i++){
		field* p=map_getp(cols,get(rows.vals,i));
		if(p) p->index=1;
	}
	map_free(&rows);
	return cols;
}
//map table_fields(var conn,string table){
//	map rs=lite_exec(conn,format("pragma table_info ({})",table),(map){0});
//	if(!rs.keys.len) return (map){0};
//	map ret=map_new_ex(sizeof(field));
//	for(int i=0; i<rows_count(rs); i++){
//		map row=rows_row(rs,i);
//		string name=s_lower(s_dup(map_c(row,"name")));
//		string type=map_c(row,"type");
//		pair typesize=cut(type,"(");
//		type=s_lower(typesize.head);	
//		if(!type.len) type=c_("text");
//		string size=sub(typesize.tail,0,-1);
//		field fld={
//			.name=ro(name),
//			.size=atoil(size),
//			.type=meta_type(type,atoil(size),name),
//			.pkey=atoil(map_c(row,"pk")) ? 1 : 0,
//			.title=name_title(name)
//		};
//		map_add_ex(&ret,name,&fld,NULL);
//	}
//	map_free(&rs);
//	map params=map_new();
//	map_add(&params,c_("tbl"),table);
//	map rows=lite_exec(conn,c_("select substr(sql,instr(sql,'(')+1,instr(sql,')')-instr(sql,'(')-1) from sqlite_master where type='index' and tbl_name=:tbl and sql like '%UNIQUE%'"),params);
//	vector vals=own(&rows.vals);
//	map_free(&rows);
//	for(int i=0; i<vals.len; i++){
//		field* p=map_getp(ret,get(vals,i));
//		p->unique=1;
//	}
//	_free(&vals);
//	return ret;
//}
string cols_litecreate(map cols){
	string ret={0};
	string pkeys={0};
	for(int i=0; i<cols.keys.len; i++){
		field fld=*(field*)vec_p(cols.vals,i);
		string cls=ro(fld.create);
		if(!cls.len) cls=c_("varchar(50) collate nocase not null default ''");
		cls=s_join(fld.name," ",cls);
		ret=s_join(ret,",\n\t",cls);

		if(fld.pkey){
			pkeys=s_join(pkeys,", ",fld.name);
		}
	}
	if(pkeys.len) ret=s_join(ret,",\n\t",format("primary key({})",pkeys));
	return ret;
}
vector cols_liteindex(map cols,string table){
	vector ret=NullVec;
	for(int i=0; i<cols.keys.len; i++){
		field fld=*(field*)vec_p(cols.vals,i);
		if(fld.pkey||!fld.index && !fld.unique) continue;
		string unique=fld.unique ? c_(" unique") : (string){0};
		vec_add(&ret,format("create{} index idx_{}_{} on {} ({})",unique,table,fld.name,table,fld.name));
	}
	return ret;
}
vector lite_create(string table,map cols){
	vector ret=NullVec;
	vec_add(&ret,format("drop table if exists {}",table));
	vec_add(&ret,format("create table {} (\n\t{}\n)",table,cols_litecreate(cols)));
	ret=cat(ret,cols_liteindex(cols,table));
	return ret;
}
