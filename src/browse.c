#include "csql.h"
#include "structure.h"
#include "database.h"
#include "file.h"
#include "browse.h"
int table_list(var conn){
	int2 dim=vis_size();
	map rows=lite_exec(conn, c_("select name from sqlite_master where type='table' and not name in ('search', 'search_config', 'search_content', 'search_data', 'search_docsize', 'search_idx') order by 1"),(map){0});
	string types_s=file_s(c_("types.tsv"));
	cross types=cross_new(s_rows(types_s));
	int c=0;
	map vals=rows_vals(rows, 0);
	window win={.x=4, .y=3,.width=dim.x-8,.height=dim.y-6};
	
	do{
		string tbl=win_menu(win,map_ro(vals),c_("Tables"));
		if(!tbl.len) break;
		table_browse(conn,tbl,win,types);
		fflush(stdout);
	}while(1);
	_free(&types_s);
	map_free(&vals);
	cross_free(&types);
	map_free(&rows);
	return 0;
}
int table_browse(var conn, string table,window win,cross types){
	browse_data e=cols_browsedata(tbl_cols(conn, ro(table), types),win);
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
				e=cols_browsedata(tbl_cols(conn, ro(table), types),win);
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
				vis_msg("reloaded");
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
	e->can_search=1;

	vector searching={0};
	map filter={0};

	if(e->rs.rows.vals.len){
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

	string sql=format("select * from {} order by 1", ro(table));
	sql=sql_filter(sql,ro(filter.keys));
	sql=sql_limit(sql, e->curs.limit.from, e->curs.limit.len);
	map rows=lite_exec(conn, sql,filter);
	rows.vals=splice(rows.vals,rows.vals.len,0,vec_new_ex(sizeof(var),rows.keys.len),NULL);
	rows=rows_addcol(rows,-1,Null,c_("rowid__"));
	if(!searching.len) searching=vec_new_ex(sizeof(var),rows.keys.len);
	rows.vals=splice(rows.vals,0,0,searching,NULL);
	for(int i=0; i<rows.vals.len/rows.keys.len; i++){
		rows.vals.var[i*rows.keys.len+rows.keys.len-1].i=e->rowid++;
	}
	e->curs.total.y+=2;

	e->rs=cross_reinit(e->rs,rows);
	if(lite_error(conn)) vis_msg("%.*s",ls(lite_msg(conn)));
	e->reload=0;
	return e;
}
