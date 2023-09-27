#include "browse.h"
#include "tsv.h"
#include "structure.h"
#include "cmdline.h"
#include "database.h"

int table_list(var conn, cross types){
	int2 dim=vis_size();
	int c=0;
	window win={.x=4, .y=3,.width=dim.x-8,.height=dim.y-6};
	window vwin=win;
	vwin.width=min(30,vwin.width);

	browse e=browse_new();
	browse_setwin(&e,vwin);
	browse_setfield(&e,s_fields(c_("name type code title Tables")));

	map rows=lite_exec(conn, c_("select name from sqlite_master where type='table' and not name in ('search', 'search_config', 'search_content', 'search_data', 'search_docsize', 'search_idx') order by 1"),NullMap);
	vec_add(&rows.vals,Null);
	e.curs.total.x=1;
	e.curs.total.y=rows.vals.len;
	e.rs=cross_new(rows);
	e.can_search=0;

	do{
		if(c=='\n'){
			if(e.curs.curr.y==-1){
				int ret=tsv_browse(args.typesfile,win,types);
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
	browse e=browse_new();
	browse_setwin(&e,win);
	browse_setfield(&e,table_fields(conn, ro(table), types));

//	e.curs.curr.y++;

	int needs_reload=0;
	cursor old={0};
	int c=0;
	do{
		if(c=='\n' && e.curs.curr.y==-1){
			int ret=structure_browse(conn, table,win,types);
			if(ret){ //structure changed
				browse_setfield(&e,table_fields(conn, ro(table), types));
				browse_reload(&e,conn,table);
			}
		}
		else{
			old=e.curs;
			browse_key(&e,c);
			needs_reload=e.reload;
		}
		confirm_save(&e);

		if(!e.reload && needs_reload) e.curs=old;
		needs_reload=0;

		browse_save(&e,conn,table);
		if(e.exit) break;
		browse_reload(&e,conn,table);
		log_show(e.win);
		browse_view(&e);
		table_title(&e,table);
		browse_body(&e);
		fflush(stdout);
	} while((c=vim_char(vim_getch())));
	browse_free(&e);
	win_clear(e.win);
	return 0;
}
browse* browse_reload(browse* e,var conn,string table){
	if(!e->reload) return e;
	e->reload=0;

	vector searching={0};
	map filter={0};
	e->can_search=1;
	filter=cross_row(e->rs,0,Null);
	filter=filter_params(filter);

	string sq={0};
	sq=format("select count(*) from {}",ro(table));
	sq=sql_filter(sq,ro(filter.keys));
	e->curs.total.y=atoil(lite_val(conn, sq, map_ro(filter)));

	string sql=format("select * from {} order by 1", ro(table));
	sql=sql_filter(sql,ro(filter.keys));
	sql=sql_limit(sql, e->curs.limit.from, e->curs.limit.len);
	map rows=lite_exec(conn, sql,filter);
	browse_setrows(e,rows);
	if(lite_error(conn)) vis_msg("%.*s",ls(lite_msg(conn)));
	return e;
}
browse* browse_save(browse* e, var conn, string table){
	if(!e->save) return e;
	e->save=0;

	vector pkeys=fields_pkeys(e->fields);
	lite_exec(conn,c_("begin transaction"),NullMap);
	int errors=0;
	string msg={0};
	map_each(e->changed,i,pair* p){

		map changed=map_new();
		each(p[i].tail,j,var* val){
			if(val[j].len==End) continue;
			map_add(&changed,ro(e->fields.keys.var[j]),ro(val[j]));
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
		vis_log(c_("Press ESC to close"),e->win);
		getchar();
	}
	vfree(pkeys);
	return e;
}
