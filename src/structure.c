#include "window.h"
#include "browse.h"
#include "field.h"
#include "structure.h"

void structure_reload(browse_data* e,var conn, string table, cross types){
	if(!e->reload) return;
	map cols=table_fields(conn, ro(table), types);
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
				vector suggest=cross_col(types,0,c_("type"));
				string val=cell_combo(&e,suggest);
				if(val.len!=Fail){
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
void structure_save(browse_data* e,var conn,string table,window win){
	map fields=rows_fields(e->rs.rows);
	map colmap=log_colmap(e->changed,cross_colno(e->rs,c_("name")),fields.keys);
	vector sqls=sync_fields(table, fields,colmap);
	sqls=cat(sqls,sql_triggers(conn,table));

	int errs=sqls_exec(conn, sqls,e->win);

	map_free_ex(&fields, field_free);
	if(errs){
		e->exit=0;
		e->reload=0;
	}
	else{
		vis_msg("Structure Updated");
		map_free_ex(&e->changed,vecpair_free);
	}
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

map log_colmap(map in,int fieldno,vector newcols){
	map maps=map_new();
	vector added=vec_new();
	map_each(in,i,pair* p){
		if(!p[i].tail.len) continue;
		string val=p[i].tail.var[fieldno];
		if(!p[i].head.len){
			vec_add(&added,val);
			continue;
		}
		string old=p[i].head.var[fieldno];
		if(val.len) map_add(&maps,ro(val),ro(old));
	}
	map ret=map_new();
	each(newcols,i,string* v){
		if(vec_search(added,v[i])!=Fail) continue;
		var* old=map_getp(maps,v[i]);
		if(!old){
			map_add(&ret,ro(v[i]),ro(v[i]));
			continue;
		}
		if(!old->len) continue;
		map_add(&ret,ro(v[i]),ro(*old));
	}
	_free(&added);
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
		if(at==Fail) continue;
		map_add(&ret,n,oldun.var[at]);
		vec_del(&newun,i);
		vec_del(&oldun,at);
		i--;
	}
	if(!oldun.len||!newun.len){
		vfree(oldun);
		vfree(newun);
		return ret;
	}
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
	vector sqls=sql_split(sql);
	vec_trim(sqls);
	vector toks={0};
	int at=Fail;
	each(sqls, i, string* s){
		vfree(toks);
		toks=code_split(s[i]," \t\n\r(",0);
		if(toks.len<2) continue;
		if(!eq_c(toks.var[0],"create") || !eq_c(toks.var[1],"table"))
			continue;
		at=i;
		break;
	}
	if(at==Fail){
		vfree(toks);
		return Null;
	}
	map cols=sql_cols(sqls.var[at],types);
	map colmap=match_cols(cols,oldcols);
	map_free_ex(&cols,field_free);
	string colscls=vec_start(toks,"(");
	vfree(toks);

	vector ret=vec_new();

	if(!colmap.keys.len){
		vec_add(&ret, format("create table {} (\n\t{}\n)",name,colscls));
	}
	else{
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
	}
	sqls=splice(sqls,at,1,ret,NULL);
	return sqls;
}
vector sync_fields(string name, map cols,map colmap){
	vector ret=vec_new();
	vec_add(&ret, print_s("drop table if exists _temp"));
	string colscls=cols_litecreate(cols);
	if(!colmap.keys.len){
		vec_add(&ret, format("create table {} (\n\t{}\n)",name,colscls));
	}
	else{
		vec_add(&ret, format("create table _temp (\n\t{}\n)",colscls));
		string sql=format("insert into _temp ({}) select {} from {}",
			vec_s(ro(colmap.keys), ", "),
			vec_s(ro(colmap.vals), ", "),
			name
		);
		vec_add(&ret, sql);
		vec_add(&ret, format("drop table {}",name));
		vec_add(&ret, format("alter table _temp rename to {}",name));
	}
	ret=cat(ret, cols_liteindex(cols,name));
	map_free(&colmap);
	return ret;
}
int sqls_exec(var conn, vector sqls,window win){
	int errs=0;
	vis_log(c_("begin"),win);
	lite_exec(conn, c_("begin"),NullMap);
	for(int i=0; i<sqls.len; i++){
		vis_log(sqls.var[i],win);
		lite_exec(conn, ro(sqls.var[i]),NullMap);
		if(lite_error(conn)){
			log_show(win);
			errs=1;
			break;
		}
		else{
			vis_log(c_("OK"),win);
		}
	}
	if(!errs){
		vis_log(c_("commit"),win);
		lite_exec(conn, c_("commit"),NullMap);
		if(lite_error(conn)){
			log_show(win);
			errs=1;
		}
		else{
			vis_log(c_("OK"),win);
		}
	}
	if(errs){
		vis_log(c_("rollback"),win);
		lite_exec(conn,c_("rollback"),NullMap);
		vis_error(c_("Press ESC to close"),win);
	}
	vec_free(&sqls);
	return errs;
}
string table_sqls(var conn, string table){
	map rows=lite_exec(conn, c_("select sql from sqlite_schema where tbl_name=:name"),map_all(c_("name"),ro(table)));
	string ret=vec_s(ro(rows.vals), ";\n");
	//ret=cat_c(ret,";\n");
	map_free(&rows);
	return ret;
}
int edit_sqls(var conn,string table,window win,cross types){
	string sqls=table_sqls(conn,table);
	int ret=0;
	editwin ewin=editwin_new(win,sqls);
	while(1){
		edit_input(&ewin);
		if(!editwin_changed(&ewin))
			break;

		else if(ewin.key==27){
			string ans=win_menu(win,s_map(c_("yes Apply no Discard redit Re-edit")),c_("Apply Changes?"));
			if(!ans.len || eq_c(ans,"no"))
				break;
			if(eq_c(ans,"redit"))
				continue;
		}
		string nsqls=editwin_get(&ewin);
		map cols=table_fields(conn,table,types);
		int errs=sqls_exec(conn, sync_sql(table,nsqls,types,cols),win);
		_free(&nsqls);
		map_free_ex(&cols,field_free);
		if(!errs){
			vis_msg("Structure Updated");
			ret=1;
			break;
		}
	}
	editwin_free(&ewin);
	vfree(sqls);
	return ret;
}
