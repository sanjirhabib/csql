#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <time.h>
#include <sqlite3.h>

#include "var.h"
#include "sql.h"
#include "log.h"
#include "file.h"
#include "lite.h"


var lite_conn(string db){
	var ret={.len=IsPtr};
	string name=filename_os(db);
	if(sqlite3_open_v2(name.ptr,(sqlite3**)&ret.ptr,SQLITE_OPEN_READWRITE|SQLITE_OPEN_URI,NULL)!=SQLITE_OK){
		_free(&name);
		lite_print_error(ret,(string){0},(map){0});
		return ret;
	}
	_free(&name);
	return ret;
}
int lite_error(var conn){
	return sqlite3_errcode(conn.ptr);
}
string lite_msg(var conn){
	return lite_error(conn) ? c_((char*)sqlite3_errmsg(conn.ptr)) : (string){0};
}
var lite_print_error(var conn,string sql,map params){
	log_error("%s",sqlite3_errmsg(conn.ptr));
	_free(&sql);
	map_free(&params);
	return Null;
}
var lite_rs(var conn,var sql,const map params){
	sqlite3_stmt* stm=NULL;
	if(sqlite3_prepare_v2(conn.ptr,sql.ptr,sql.len,&stm,NULL)!=SQLITE_OK)
		return lite_print_error(conn,sql,params);
	for(int i=0; i<params.keys.len; i++){
		string temp=s_c(cat(c_(":"),get(params.keys,i)));
		int idx=sqlite3_bind_parameter_index(stm,temp.str);
		_free(&temp);
		if(!idx) continue;
		int ret=sqlite3_bind_text(stm,idx,params.vals.var[i].ptr ? params.vals.var[i].ptr : "",params.vals.var[i].len,NULL);
		if(ret!=SQLITE_OK)
			return lite_print_error(conn,sql,params);

	}
	//map_free(&params);
	vfree(sql);
	return ptr_(stm);
}
vector rs_cols(var rs){
	int ncols=sqlite3_column_count(rs.ptr);
	if(!ncols) return NullVec;
	vector ret=vec_new(sizeof(var),ncols);
	for(int i=0; i<ncols; i++){
		char* temp=(char*)sqlite3_column_name(rs.ptr,i);
		ret.var[i]=c_dup(temp);
	}
	return ret;
}
void msleep(int milisec){
	struct timespec sleep_time={
		.tv_sec = 0,
		.tv_nsec = milisec*1000000,
	};
	nanosleep(&sleep_time, NULL);
}
vector rs_row(var rs){
	int ncols=sqlite3_column_count(rs.ptr);
	if(!ncols) return NullVec;
	int status=0;
	while((status=sqlite3_step(rs.ptr))==SQLITE_BUSY){
		msleep(1);
	}
	if(status==SQLITE_DONE){
		rs_close(rs);
		return NullVec;
	}
	else if(status!=SQLITE_ROW){
		rs_close(rs);
		lite_print_error(ptr_(sqlite3_db_handle(rs.ptr)),c_(sqlite3_expanded_sql(rs.ptr)),NullMap);
		return NullVec;
	}
	vector ret=vec_new(sizeof(var),ncols);
	for(int i=0;i<ncols;i++){
		char* temp=(char*)sqlite3_column_text(rs.ptr,i);
		ret.var[i]=c_dup(temp);
	}
	return ret;
}
void rs_close(var rs){
	if(rs.ptr) sqlite3_finalize(rs.ptr);
}
void rs_free(void* rs){
	var* temp=rs;
	rs_close(*temp);
	*temp=Null;
}
var lite_val(var conn,string sql,map params){
	var rs=lite_rs(conn,sql,params);
	vector row=rs_row(rs);
	if(!row.len){
		return Null;
	}
	string ret=row.var[0];
	row.var[0].readonly=1;
	vec_free(&row);
	map_free(&params);
	rs_close(rs);
	return ret;
}
vector lite_vec(var conn,string sql,map params){
	var rs=lite_rs(conn,sql,params);
	vector ret=NullVec;
	vector row=Null;
	while((row=rs_row(rs)).len) ret=cat(ret,row);
	map_free(&params);
	return ret;
}
map lite_rows(var conn, var sql, map params){
	var rs=lite_rs(conn,sql,params);
	vector keys=rs_cols(rs);
	vector row=Null;
	vector vals=NullVec;
	while((row=rs_row(rs)).len){
		vals=cat(vals,row);	
	}
	map_free(&params);
	return (map){
		.keys=keys,
		.vals=vals,
		.index=keys_index(keys),
	};
}
int lite_exec(var conn,var sql,map params){
	var rs=lite_rs(conn,sql,params);
	int status=0;
	while((status=sqlite3_step(rs.ptr))==SQLITE_BUSY){
		msleep(1000);
	}
	rs_close(rs);
	map_free(&params);
	if(status==SQLITE_DONE || status==SQLITE_ROW){
		return 0;
	}
	return -1;
}
int lite_close(var conn){
	sqlite3_close_v2(conn.ptr);
	return 0;
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
vector lite_tables(var conn){
	return lite_vec(conn, c_("select name from sqlite_master where type='table' and not name in ('search', 'search_config', 'search_content', 'search_data', 'search_docsize', 'search_idx') order by 1"),NullMap);
}
string lite_tablesql(var conn, string table){
	return lite_val(conn,c_("select sql from sqlite_schema where type='table' and name=:name"),map_all(c_("name"),ro(table)));
}
string sqls_s(vector in){
	return cat(vec_s(in, ";\n"),c_(";\n"));
}
string table_sqls(var conn, string table){
	return sqls_s(lite_vec(conn, c_("select sql from sqlite_schema where tbl_name=:name"),map_all(c_("name"),ro(table))));
}
map table_fields(var conn,string table,cross types){
	string mem=table;
	table.readonly=1;
	string sql=lite_tablesql(conn,table);
	map cols=sql_cols(sql,types);
	if(!cols.keys.len) return cols;
	((field*)getp(cols.vals,0))->mem=sql.ptr;

	vector rows=lite_vec(conn,c_("select substr(sql,instr(sql,'(')+1,instr(sql,')')-instr(sql,'(')-1) from sqlite_master where type='index' and tbl_name=:tbl and sql like '%UNIQUE%'"),map_all(c_("tbl"),table));
	each(rows,i,var* n){
		field* p=map_getp(cols,n[i]);
		if(p) p->unique=1;
	}
	vec_free(&rows);
	rows=lite_vec(conn,c_("select substr(sql,instr(sql,'(')+1,instr(sql,')')-instr(sql,'(')-1) from sqlite_master where type='index' and tbl_name=:tbl and sql not like '%UNIQUE%'"),map_all(c_("tbl"),table));
	each(rows,i,n){
		field* p=map_getp(cols,n[i]);
		if(p) p->index=1;
	}
	vec_free(&rows);
	_free(&mem);
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
string fields_litecreate(const map cols){
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
vector fields_liteindex(const map cols,const string table){
	vector ret=NullVec;
	for(int i=0; i<cols.keys.len; i++){
		field fld=*(field*)vec_p(cols.vals,i);
		assert(fld.name.len);
		if(fld.pkey||!fld.index && !fld.unique) continue;
		string unique=fld.unique ? c_(" unique") : (string){0};
		vec_add(&ret,print_s("create%.*s index idx_%.*s_%.*s on %.*s (%.*s)",ls(unique),ls(table),ls(fld.name),ls(table),ls(fld.name)));
	}
	return ret;
}
vector lite_create(string table,map cols){
	vector ret=NullVec;
	vec_add(&ret,format("drop table if exists {}",table));
	vec_add(&ret,format("create table {} (\n\t{}\n)",table,fields_litecreate(cols)));
	ret=cat(ret,fields_liteindex(cols,table));
	return ret;
}
