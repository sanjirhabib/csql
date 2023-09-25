#include "window.h"
#include "structure.h"
#include "browse.h"
#include "file.h"
#include "field.h"
#include "database.h"

int table_list(var conn, cross types){
	int2 dim=vis_size();
	int c=0;
	window win={.x=4, .y=3,.width=dim.x-8,.height=dim.y-6};
	window vwin=win;
	vwin.width=min(30,vwin.width);


	browse_data e=cols_browsedata(s_fields(c_("name type code title Tables")),vwin);
	map rows=lite_exec(conn, c_("select name from sqlite_master where type='table' and not name in ('search', 'search_config', 'search_content', 'search_data', 'search_docsize', 'search_idx') order by 1"),NullMap);
	vec_add(&rows.vals,Null);
	e.curs.total.x=1;
	e.curs.total.y=rows.vals.len;
	e.rs=cross_new(rows);
	e.can_search=0;

	browse_colwidth(&e);

	do{
		if(c=='\n'){
			if(e.curs.curr.y==-1){
				vis_msg("forward");
			}
			else if(e.curs.curr.y==e.curs.total.y-1){
				browse_key(&e,c);
				string tbl=cell_get(&e,c_("name"));
				if(tbl.len){
					int ret=structure_browse(conn, tbl,win,types);
					if(ret){
						map rows=lite_exec(conn, c_("select name from sqlite_master where type='table' and not name in ('search', 'search_config', 'search_content', 'search_data', 'search_docsize', 'search_idx') order by 1"),NullMap);
						vec_add(&rows.vals,Null);
						e.curs.total.x=1;
						e.curs.total.y=rows.vals.len;
						e.rs=cross_reinit(e.rs,rows);
					}
				}
			}
			else{
				string tbl=cell_get(&e,c_("name"));
				table_browse(conn,tbl,win,types);
			}
		}
		else browse_key(&e,c);
		browse_view(&e);
		browse_body(&e);
		fflush(stdout);
	} while(!e.exit && (c=vim_char(vim_getch())));
	browse_free(&e);
	win_clear(e.win);
	return 0;
}
int table_browse(var conn, string table,window win,cross types){
	browse_data e=cols_browsedata(table_fields(conn, ro(table), types),win);
	browse_reload(&e,conn,table);
	browse_colwidth(&e);
	e.curs.curr.y++;

	cursor old={0};
	int c=0;
	do{
		if(c=='\n' && e.curs.curr.y==-1){
			int ret=structure_browse(conn, table,win,types);
			if(ret){ //structure changed
				browse_free(&e);
				e=cols_browsedata(table_fields(conn, ro(table), types),win);
				browse_reload(&e,conn,table);
				browse_colwidth(&e);
			}
		}
		else{
			old=e.curs;
			browse_key(&e,c);
		}
		if(e.exit){
			if(confirm_save(&e))
				browse_save(&e,conn,table);
			if(e.exit)
				break;
		}
		if(e.reload){
			if(confirm_save(&e))
				browse_save(&e,conn,table);
			if(e.reload){
				browse_reload(&e,conn,table);
			}
			else e.curs=old;
		}
		browse_view(&e);
		table_title(&e,table);
		browse_body(&e);
		fflush(stdout);
	} while((c=vim_char(vim_getch())));
	browse_free(&e);
	win_clear(e.win);
	return 0;
}
browse_data* browse_reload(browse_data* e,var conn,string table){
	if(!e->reload) return e;

	vector searching={0};
	map filter={0};

	if(e->can_search){
		searching=cross_disownrow(e->rs,0,Null);
		filter=filter_params(
			(map){
			.keys=ro(e->cols.keys),
			.vals=searching,.index=ro(e->cols.index)
			}
		);
	}

	string sq={0};
	sq=format("select count(*) from {}",ro(table));
	sq=sql_filter(sq,ro(filter.keys));
	e->curs.total.y=atoil(lite_val(conn, sq, map_ro(filter)));
	if(e->curs.total.y){
		e->can_search=1;
	}

	string sql=format("select * from {} order by 1", ro(table));
	sql=sql_filter(sql,ro(filter.keys));
	sql=sql_limit(sql, e->curs.limit.from, e->curs.limit.len);
	map rows=lite_exec(conn, sql,filter);
	rows.vals=splice(rows.vals,rows.vals.len,0,vec_new_ex(sizeof(var),rows.keys.len),NULL);
	rows=rows_addcol(rows,-1,Null,c_("rowid__"));

	if(e->can_search){
		if(!searching.len) searching=vec_new_ex(sizeof(var),rows.keys.len);
		rows.vals=splice(rows.vals,0,0,searching,NULL);
		e->curs.total.y++;
	}
	else vec_free(&searching);

	for(int i=0; i<rows.vals.len/rows.keys.len; i++){
		rows.vals.var[i*rows.keys.len+rows.keys.len-1].i=e->rowid++;
	}
	e->curs.total.y++;

	e->rs=cross_reinit(e->rs,rows);
	if(lite_error(conn)) vis_msg("%.*s",ls(lite_msg(conn)));
	e->reload=0;
	return e;
}
browse_data* browse_save(browse_data* e, var conn, string table){
	vector pkeys=cols_pkeys(e->cols);
	lite_exec(conn,c_("begin transaction"),NullMap);
	int errors=0;
	string msg={0};
	map_each(e->changed,i,pair* p){

		map changed=map_new();
		each(p[i].tail,j,var* val){
			if(val[j].len==IsFail) continue;
			map_add(&changed,ro(e->cols.keys.var[j]),ro(val[j]));
		}

		map keys={0};
		if(p[i].head.len){
			keys=only_cols((map){
				.vals=p[i].head,
				.keys=ro(e->rs.rows.keys),
				.index=ro(e->rs.rows.index),
				},
				pkeys);
		}


		if(!p[i].head.len){
			//insert
			msg=lite_insert( conn,  table, changed);
			if(msg.len) errors++;
			else msg=c_("Record added");
		}
		else if(!p[i].tail.len){
			//delete
			msg=lite_delete( conn,  table, keys);
			if(msg.len) errors++;
			else msg=c_("Record deleted");
		}
		else{
			//update
			msg=lite_update( conn,  table, keys, changed);
			if(msg.len) errors++;
			else msg=c_("Record updated");
		}
	}
	if(!errors){
		lite_exec(conn,c_("commit"),NullMap);
		if(lite_msg(conn).len){
			errors++;
			msg=lite_msg(conn);
		}
	}
	if(!errors){
		vis_msg(msg.str);
		map_free_ex(&e->changed,vecpair_free);
	}
	else{
		lite_exec(conn,c_("rollback"),NullMap);
		e->reload=0;
		e->exit=0;
		vis_msg(msg.str);
		log_show(e->win);
		fflush(stdout);
		getchar();
	}
	vfree(pkeys);
	return e;
}
