#include "csql.h"
#include "database.h"
#include "structure.h"

void structure_reload(browse_data* e,var conn, string table, cross types){
	if(!e->reload) return;
	map cols=tbl_cols(conn, ro(table), types);
	map rows=fields_rows(cols);
	rows.vals=splice(rows.vals,rows.vals.len,0,vec_new_ex(sizeof(var),rows.keys.len),NULL);
	rows=rows_addcol(rows,-1,Null,c_("rowid__"));
	for(int i=0; i<rows.vals.len/rows.keys.len; i++){
		rows.vals.var[i*rows.keys.len+rows.keys.len-1].i=e->rowid++;
	}
	e->curs.total.y+=cols.keys.len+1;
	e->rs=cross_reinit(e->rs,rows);
	map_free_ex(&cols,field_free);
	e->reload=0;
}
int structure_browse(var conn, string table,window win,cross types){
	browse_data e=cols_browsedata(structure_cols(),win);
	structure_reload(&e,conn,table,types);
	browse_colwidth(&e);

	int c=0;
	int ret=0;
	do{
		if(c=='\n'){
			if(e.curs.curr.y==-1){
				ret=edit_sqls(conn,table,win,types);
				if(ret) e.reload=1;
			}
			else if(eq_c(curs_colname(e.curs),"type")){
				string old=cell_get(&e,Null);
				vector suggest=cross_col(types,0,c_("type"));
				string val=cell_combo(&e,suggest);
				if(!eq(val,old)){
					cell_set(&e,c_("type"),val);
					map rtype=cross_row(types,0,val);
					if(rtype.keys.len){
						cell_set(&e,c_("create"),map_get(rtype,c_("create")));
						cell_set(&e,c_("index"),map_get(rtype,c_("index")));
					}
				}
			}
			else browse_key(&e,c);
		}
		else{
			browse_key(&e,c);
		}
		if(e.exit){
			if(confirm_save(&e)){
				structure_save(&e,conn,table,e.win);
				ret=1;
			}
			if(e.exit)
				break;
		}
		if(e.reload){
			if(confirm_save(&e)){
				structure_save(&e,conn,table,e.win);
				ret=1;
			}
			if(e.reload){
				structure_reload(&e,conn,table,types);
				browse_colwidth(&e);
			}
		}
		browse_view(&e);
		structure_title(&e,table);
		browse_body(&e);
		fflush(stdout);
	} while((c=vim_char(vim_getch())));

	browse_free(&e);
	win_clear(e.win);
	return ret;
}
void cell_log(browse_data* e,string name,string val){
	int rowno=curs_rowno(e->curs);
	string key=cross_get(e->rs,rowno,Null,0,c_("rowid__"));
	pair* temp=map_add_ex(&e->changed,key,NULL,NULL);
	if(!temp->head.len) temp->head=vec_dup(cross_row(e->rs,rowno,Null).vals);
	if(!temp->tail.len){
		vector v=vec_new_ex(sizeof(var),temp->head.len);
		for(int i=0; i<v.len; i++) v.var[i].len=IsNone;
		temp->tail=v;
	}
	int nid=cross_colno(e->rs,name);
	if(nid!=None) temp->tail.var[nid]=ro(val);
}
void cell_set(browse_data* e,string name,string val){
	cell_log(e,name,val);
	e->rs=cross_set(e->rs,curs_rowno(e->curs),Null,0,name.len ? name : curs_colname(e->curs),val);
	if(e->curs.curr.y==e->curs.total.y-1){ //add another newline bellow
		vector row=vec_new_ex(sizeof(var),e->rs.rows.keys.len);
		row.var[row.len-1].i=e->rowid++;
		e->rs=cross_addrow(e->rs, -1, Null, row);
		e->curs.total.y++;
	}
}
string cell_get(browse_data* e,string name){
	return cross_get(e->rs,curs_rowno(e->curs),Null,0,name.len ? name : curs_colname(e->curs));
}
string cell_combo(browse_data* e,vector suggests){
	window w=cursor_win(e->curs.curr,e->view,e->wins);
	string colname=curs_colname(e->curs);
	string val=cell_get(e,colname);
	return edit_combo(w,val,val,suggests);
}
string cell_edit(browse_data* e){
	window w=cursor_win(e->curs.curr,e->view,e->wins);
	string colname=curs_colname(e->curs);
	string val=cell_get(e,colname);
	return edit_input(w,val,val);
}
void structure_save(browse_data* e,var conn,string table,window win){
	map fields=rows_fields(e->rs.rows);
	map colmap=log_colmap(e->changed,cross_colno(e->rs,c_("name")),fields.keys);
	vector sqls=sync_fields(table, fields,colmap);
	sqls=cat(sqls,sql_triggers(conn,table));

	lite_exec(conn, c_("begin transaction"),(map){0});
	int errs=sqls_exec(conn, sqls,e->win);
	if(!errs) lite_exec(conn, c_("commit"),(map){0});

	map_free_ex(&fields, field_free);
	map_free_ex(&e->changed,vecpair_free);
	if(errs){
		e->exit=0;
		e->reload=0;
	}
	else
		vis_msg("Structure Updated");
}
void structure_title(browse_data* e,string table){
	string title=cat(c_("Structure: "),ro(table));
	win_title(win_resize(e->win,WinRight,-e->win.width/2),title,WinLeft);
	vis_goto(e->win.x+e->win.width/2,e->win.y);
	string temp={0};
	if(e->curs.curr.y==-1)
		temp=c_("Edit SQL");
	else if(e->curs.curr.y==e->curs.total.y-1)
		temp=c_("+Add New");
	else
		temp=print_s("Column: %d/%d",e->curs.curr.y+1,e->curs.total.y-1);
	s_out(s_pad(temp,e->win.width/2,WinRight));
}

map structure_cols(){
	field colmeta[]={
		{c_("name"),c_("name"), c_("code"),c_("Name"),{0},{0},NULL,50,1,0,0},
		{c_("type"),c_("type"), c_("int"),c_("Type"),{0},{0},NULL,0,0,0,1},
		{c_("create"),c_("create"), c_("int"),c_("Create"),{0},{0},NULL,0,0,0,0},
		{c_("pkey"),c_("pkey"), c_("bool"),c_("PKey"),{0},{0},NULL,0,0,0,1},
		{c_("unique"),c_("unique"), c_("bool"),c_("Unique"),{0},{0},NULL,0,0,0,1},
		{c_("index"),c_("index"), c_("bool"),c_("Index"),{0},{0},NULL,0,0,0,1},
		{c_("meta"),c_("meta"), c_("int"),c_("Meta"),{0},{0},NULL,0,0,0,0},
	};
	vector vals=vec_new_ex(sizeof(field), 0);
	vec_add_ex(&vals, sizeof(colmeta)/sizeof(field),colmeta);
	return vec_map(s_vec(c_("name type create pkey unique index meta"), " "),vals);
}
map fields_rows(map cols){
	vector vals=vec_new();
	map_each(cols,i,field* f){
		vals=vec_append(vals,
		_dup(f[i].name),
		_dup(f[i].name),
		_dup(f[i].type),
		_dup(f[i].create),
		i_s(f[i].pkey),
		i_s(f[i].unique),
		i_s(f[i].index),
		Null
		);
	}
	return vec_map(s_vec(c_("id name type create pkey unique index meta"), " "),vals);
}
map rows_fields(map cols){
	vector vals=vec_new();
	map ret=map_new_ex(sizeof(field));
	for(int i=0; i<cols.vals.len/cols.keys.len; i++){
		map row=rows_row(cols, i);
		if(!map_get(row,c_("name")).len) continue;
		field f={
			.id=map_get(row, c_("id")),
			.name=map_get(row, c_("name")),
			.type=map_get(row, c_("type")),
			.create=map_get(row, c_("create")),
			.pkey=_i(map_get(row, c_("pkey"))),
			.unique=_i(map_get(row, c_("unique"))),
			.index=_i(map_get(row, c_("index"))),
			.meta=map_get(row, c_("meta")),
		};
		map_add_ex(&ret, f.name,&f,NULL);
	}
	return ret;
}
map log_colmap(map in,int fieldno,vector newcols){
	map maps=map_new();
	map_each(in,i,pair* p){
		if(!p[i].head.len) continue;
		if(!p[i].tail.len) continue;
		string old=p[i].head.var[fieldno];
		string val=p[i].tail.var[fieldno];
		if(val.len) map_add(&maps,ro(val),ro(old));
	}
	map ret=map_new();
	each(newcols,i,string* v){
		var* old=map_getp(maps,v[i]);
		if(!old){
			map_add(&ret,ro(v[i]),ro(v[i]));
			continue;
		}
		if(!old->len) continue;
		map_add(&ret,ro(v[i]),ro(*old));
	}
	map_free(&maps);
	return ret;
}
map rows_colmap(map in){
	map ret=map_new();
	field* f=in.vals.ptr;
	for(int i=0; i<in.keys.len; i++){
		if(!f[i].id.len) continue;
		map_add(&ret,f[i].name,f[i].id);
	}
	return ret;
}
vector sql_triggers(var conn,string table){
	map rows=lite_exec(conn,c_("select sql from sqlite_schema where name=:name and type='trigger'"),map_all(c_("name"),table));
	vector ret=own(&rows.vals);
	map_free(&rows);
	return ret;
}
map match_cols(map newcols,map oldcols){
	map ret=map_new();
	vector newun=_dup(newcols.keys);
	vector oldun=_dup(oldcols.keys);
	for(int i=0; i<newun.len; i++){
		string n=newun.var[i];
		int at=vec_search(oldun,n);
		if(at==None) continue;
		map_add(&ret,n,oldun.var[at]);
		vec_del(&newun,i);
		vec_del(&oldun,at);
		i--;
	}
	if(!oldun.len||!newun.len) return ret;
	while(oldun.len && newun.len){
		map_add(&ret,newun.var[0],oldun.var[0]);
		vec_del_ex(&newun,0,1,NULL);
		vec_del_ex(&oldun,0,1,NULL);
	}
	vfree(oldun);
	vfree(newun);
	return ret;
}
vector sync_sql(string name, string sql,cross types,map oldcols){
	vector sqls=code_split(trim_ex(sql," \t\n\r;"),";",0);
	vec_trim(sqls);
	vector toks={0};
	int at=0;
	for(at=0; at<sqls.len; at++){
		vfree(toks);
		toks=code_split(sqls.var[at]," \t\n\r(",0);
		if(!eq_c(toks.var[0],"create") || !eq_c(toks.var[1],"table"))
			continue;
		break;
	}
	map cols=sql_cols(sqls.var[at],types);
	map colmap=match_cols(cols,oldcols);
	map_free_ex(&cols,field_free);
	string colscls=vec_start(toks,"(");
	vfree(toks);

	vector ret=vec_new();
	vec_add(&ret, print_s("drop table if exists _temp"));
	vec_add(&ret, print_s("create table _temp %.*s",ls(colscls)));
	string tempsql=format("insert into _temp ({})\nselect {}\nfrom {}",
		vec_s(ro(colmap.keys), ", "),
		vec_s(ro(colmap.vals), ", "),
		name
	);
	map_free(&colmap);
	vec_add(&ret, tempsql);
	vec_add(&ret, format("drop table {}",name));
	vec_add(&ret, format("alter table _temp rename to {}",name));
	sqls=splice(sqls,at,1,ret,NULL);
	return sqls;
}
vector sync_fields(string name, map cols,map colmap){
	vector ret=vec_new();
	vec_add(&ret, print_s("drop table if exists _temp"));
	string colscls=cols_litecreate(cols);
	vec_add(&ret, format("create table _temp ({})",colscls));
	string sql=format("insert into _temp ({}) select {} from {}",
		vec_s(ro(colmap.keys), ", "),
		vec_s(ro(colmap.vals), ", "),
		name
	);
	vec_add(&ret, sql);
	vec_add(&ret, format("drop table {}",name));
	vec_add(&ret, format("alter table _temp rename to {}",name));
	ret=cat(ret, cols_liteindex(cols,name));
	map_free(&colmap);
	return ret;
}
int sqls_exec(var conn, vector sqls,window win){
	int ret=0;
	for(int i=0; i<sqls.len; i++){
		vis_log(sqls.var[i],win);
		lite_exec(conn, ro(sqls.var[i]),(map){0});
		if(lite_error(conn)){
			vis_error(lite_msg(conn),win);
			ret=1;
			break;
		}
		else
			vis_log(c_("OK"),win);
	}
	vec_free(&sqls);
	return ret;
}
string table_sqls(var conn, string table){
	map rows=lite_exec(conn, c_("select sql from sqlite_schema where tbl_name=:name"),map_all(c_("name"),ro(table)));
	string ret=vec_s(ro(rows.vals), ";\n");
	ret=cat_c(ret,";\n");
	map_free(&rows);
	return ret;
}
int edit_sqls(var conn,string table,window win,cross types){
	string sqls=table_sqls(conn,table);
	string nsqls=ro(sqls);
	int ret=0;
	while(1){
		nsqls=edit_input(win,nsqls,nsqls);
		if(!eq(sqls,nsqls)){
			string ans=win_menu(win,s_map(c_("yes Apply no Discard redit Re-edit")),c_("Apply Changes?"));
			if(!ans.len) break;
			else if(eq_c(ans,"no")) break;
			else if(eq_c(ans,"yes")){
				vis_msg("Applying changes");
				lite_exec(conn, c_("begin transaction"),(map){0});
				map cols=tbl_cols(conn,table,types);
				int errs=sqls_exec(conn, sync_sql(table,nsqls,types,cols),win);
				if(!errs) lite_exec(conn, c_("commit"),(map){0});
				map_free_ex(&cols,field_free);
				if(!errs){
					vis_msg("Structure Updated");
					ret=1;
					break;
				}
			}
		}
		else break;
	}
	vfree(sqls);
	vfree(nsqls);
	return ret;
}
