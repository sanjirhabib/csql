#include "csql.h"
#include "database.h"

/*header

typedef struct s_browse_data {
	window win;
	window view;
	vector wins;
	map cols;
	cursor curs;
	cross rs;
	map changed;
	int reload;
	int save;
	int exit;
	int rowid;
	int can_search;
} browse_data;

end*/

vector c_vec(char* in){
	return s_vec(c_(in)," ");
}
void vecpair_free(pair* pair){
	vec_free(&pair->head);
	vec_free(&pair->tail);
}
void browse_free(browse_data* e){
	cross_free(&e->rs);
	_free(&e->wins);
	map_free_ex(&e->cols,field_free);
	map_free_ex(&e->changed,vecpair_free);
}

browse_data cols_browsedata(map cols,window win){
	return (browse_data){
		.win=win,
		.view={
			.height=win.height-2,
		},
		.cols=cols,
		.curs={
			.total.x=cols.keys.len,
			.limit.len=win.height*4,
			.curr.x=min(cols.keys.len,1),
			.curr.y=0,
			.cols=ro(cols.keys),
		},
		.changed=map_new_ex(sizeof(pair)),
		.rowid=0,
		.reload=1,
	};
}
void browse_colwidth(browse_data* e){
	_free(&e->wins);
	e->wins=cols_wins(e->cols.keys,e->rs,e->win);
	cols_aligns(e->cols,e->rs);
}
browse_data* browse_key(browse_data* e,int c){
	int reload=0;

	e->curs.curr=cursor_key(e->curs.curr,c,e->view.height,(int4){.x=0,.x2=e->curs.total.x,.y=-1,.y2=e->curs.total.y});
	int ret=0;

	if(c==27){
		e->exit=1;
	}
	else if(c==KeyCtrl+'s'){
		e->save=1;
		e->reload=1;
	}
	else if(c=='\n'){
		if(e->curs.curr.y==-1){
//			ret=structure_browse(conn, table,e->win,types);
			if(ret==1) e->exit=1;
		}
		else{
			window wedit=cursor_win(e->curs.curr,e->view,e->wins);
			int rowno=curs_rowno(e->curs);
			string colname=curs_colname(e->curs);
			string val=cross_get(e->rs,rowno,Null,0,colname);
			string ret=edit_input(wedit,val,val);
			if(!eq(ret,val)){
				if(e->can_search && e->curs.curr.y==0){ //search row
					e->reload=1;
				}
				else{
					string key=cross_get(e->rs,curs_rowno(e->curs),Null,0,c_("rowid__"));
					pair* temp=map_add_ex(&e->changed,key,NULL,NULL);
					if(!temp->head.len) temp->head=vec_dup(cross_row(e->rs,rowno,Null).vals);
					if(!temp->tail.len){
						vector v=vec_new_ex(sizeof(var),temp->head.len);
						for(int i=0; i<v.len; i++) v.var[i].len=IsNone;
						temp->tail=v;
					}
					temp->tail.var[cross_colno(e->rs,colname)]=ro(ret);
				}
				cross_set(e->rs,rowno,Null,0,colname,ret);
				if(e->curs.curr.y==e->curs.total.y-1){ //add another newline bellow
					vector row=vec_new_ex(sizeof(var),e->rs.rows.keys.len);
					row.var[row.len-1].i=e->rowid++;
					e->rs=cross_addrow(e->rs, -1, Null, row);
					e->curs.total.y++;
				}
			}
			else _free(&ret);
		}
	}
	else if(c==KeyCtrlDel){
		if(e->can_search && e->curs.curr.y==0){ //filter row deleted
			cross_resetrow(e->rs, e->curs.curr.y-e->curs.limit.from, Null);
			e->reload=1;
		}
		else if(e->curs.curr.y==e->curs.total.y-1){ //deleting the addnew line
			int rid=e->curs.curr.y-e->curs.limit.from;
			var rowid=cross_get(e->rs,rid,Null,0,c_("rowid__"));
			cross_resetrow(e->rs, rid, Null);
			cross_set(e->rs,rid,Null,0,c_("rowid__"),rowid); //preserve rowid
			map_del_ex(&e->changed,rowid,vecpair_free);
		}
		else{
			string key=cross_get(e->rs,curs_rowno(e->curs),Null,0,c_("rowid__"));
			int rowno=curs_rowno(e->curs);
			pair* temp=map_add_ex(&e->changed,key,NULL,NULL);
			if(!temp->head.len) temp->head=vec_dup(cross_row(e->rs,rowno,Null).vals);
			vec_free(&temp->tail);
			e->rs=cross_delrow(e->rs,rowno,Null);
			e->curs.total.y--;
			vis_msg("1 record deleted");
		}
	}
	if(
		max(e->curs.curr.y-e->view.height,0)<e->curs.limit.from
		|| min(e->curs.curr.y+e->view.height,e->curs.total.y)>e->curs.limit.from+e->curs.limit.len
	){
		e->curs.limit.from=max(0,e->curs.curr.y-e->view.height*3);
		e->curs.limit.len=e->view.height*6;
		e->reload=1;
	}
	return e;
}
int confirm_save(browse_data* e){
	if(!e->changed.keys.len) return 0;
	if(e->save){
		e->save=0;
		return 1;
	}
	win_clear(e->win);
	string ans=win_menu(e->win,s_map(c_("yes Save no Discard redit Re-edit")),c_("Save changed?"));
	if(eq_c(ans,"no")){
		map_free_ex(&e->changed,vecpair_free);
		return 0;
	}
	else if(eq_c(ans,"yes")){
		return 1;
	}
	e->reload=0;
	return 0;
}
browse_data* browse_save(browse_data* e, var conn, string table){
	vector pkeys=cols_pkeys(e->cols);
	map_each(e->changed,i,pair* p){

		map changed=map_new();
		each(p[i].tail,j,var* val){
			if(val[j].len==IsNone) continue;
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

		string msg={0};

		if(!p[i].head.len){
			//insert
			msg=lite_insert( conn,  table, changed);
			if(!msg.len) msg=c_("Record added");
		}
		else if(!p[i].tail.len){
			//delete
			msg=lite_delete( conn,  table, keys);
			if(!msg.len) msg=c_("Record deleted");
		}
		else{
			//update
			msg=lite_update( conn,  table, keys, changed);
			if(!msg.len) msg=c_("Record updated");
		}
		vis_msg("%.*s",ls(msg));
	}
	map_free_ex(&e->changed,vecpair_free);
	vfree(pkeys);
	return e;
}
browse_data* browse_view(browse_data* e){
	e->view=cursor_view((int2){.y=max(e->curs.curr.y,0), .x=e->curs.curr.x},e->view,e->win.width,e->wins);
	return e;
}

void table_title(browse_data* e,string table){
	string title=cat(c_("Table: "),ro(table));
	int half=e->win.width/2;
	win_title(win_resize(e->win,WinRight,-half),title,WinLeft);
	string temp={0};
	if(e->curs.curr.y==0)
		temp=c_("Search");
	else if(e->curs.curr.y==-1)
		temp=c_("Setup");
	else if(e->curs.curr.y==e->curs.total.y-1)
		temp=c_("+Add New");
	else
		temp=print_s("Row: %d/%d",e->curs.curr.y,e->curs.total.y-2);
	vis_goto(e->win.x+half,e->win.y);
	s_out(s_pad(temp,half,WinRight));
}
void browse_body(browse_data* e){
	for(int i=e->view.x; i<e->view.x+e->view.width; i++){
		string col=get(e->curs.cols,i);
		int colno=keys_idx(e->rs.rows.keys,e->rs.rows.index,col);
		window* w=e->wins.ptr;
		field f=*(field*)map_getp(e->cols,col);
		if(e->curs.curr.y==-1){
			if(i==e->curs.curr.x){ //cursor
				s_out(vis_bg(0,180,0));
				s_out(vis_fg(0,0,0));
			}
			else
				s_out(vis_bg(30,30,30));
		}
		win_title(win_resize(w[i],WinTop,1),ro(f.title),f.align);
		if(e->curs.curr.y==-1)
			vis_print(VisNormal);

		vector vs=view_vals(e->view,e->rs,e->curs,i);

		for(int j=0; j<w[i].height; j++){
			int bgchanged=1;
			int rno=j+e->view.y;
			vis_goto(w[i].x,w[i].y+j);

			var rowid=cross_get(e->rs,rno-e->curs.limit.from,Null,0,c_("rowid__"));
			pair* pp=map_getp(e->changed,rowid);
			var changed=pp ? pp->tail.var[colno] : (var){.len=IsNone};

			if(i==e->curs.curr.x && rno==e->curs.curr.y){ //cursor
				s_out(vis_bg(0,180,0));
				s_out(vis_fg(0,0,0));
			}
			else if(e->can_search && rno==0) //search line
				s_out(vis_fg(255,255,255));
			else if(changed.len!=IsNone)
				s_out(vis_bg(90,30,30));
			else if(rno==e->curs.curr.y)
				s_out(vis_bg(30,30,30));
			else 
				bgchanged=0;
			string v=get(vs,j);
			text_out(ellipsis(v,w[i].width-1),w[i].width-1,f.align);
			printf(" ");
			if(bgchanged) vis_print(VisNormal);
		}
	}
	//clean the remaining rect on the right.
	window* w=e->wins.ptr;
	window wlast=w[e->view.x+e->view.width-1];
	if(wlast.width+wlast.x<e->win.width+e->win.x){
		win_clear((window){
			.x=wlast.width+wlast.x,
			.width=e->win.width+e->win.x-wlast.width-wlast.x,
			.y=wlast.y-1,
			.height=wlast.height+1,
		});
	}
}
