#include "ewin.h"
#include "browse.h"

/*header
typedef struct s_browse {
	window win;
	window view;
	vector wins;
	map fields;
	cursor curs;
	cross rs;
	map changed;
	int reload;
	int save;
	int exit;
	int rowid;
	int can_search;
} browse;

end*/

vector c_vec(char* in){
	return s_vec(c_(in)," ");
}
void vecpair_free(pair* pair){
	vec_free(&pair->head);
	vec_free(&pair->tail);
}
void browse_free(browse* e){
	cross_free(&e->rs);
	_free(&e->wins);
	map_free_ex(&e->fields,field_free);
	map_free_ex(&e->changed,vecpair_free);
}
browse browse_new(){
	return (browse){
		.changed=map_new_ex(sizeof(pair)),
		.rowid=1,
		.reload=1,
	};
}
void browse_setwin(browse* e, window win){
	e->win=win;
	e->view.height=win.height-2;
	e->curs.limit.len=e->win.height*4;
}
void browse_setrows(browse* e,map rows){

	//+add new
	rows.vals=splice(rows.vals,rows.vals.len,0,vec_new_ex(sizeof(var),rows.keys.len),NULL);
	//add rowid__
	rows=rows_addcol(rows,-1,Null,c_("rowid__"));

	//search
	if(e->can_search){
		vector searching=cross_disownrow(e->rs,0,Null);
		if(e->curs.total.y){
			if(!searching.len) searching=vec_new_ex(sizeof(var),rows.keys.len);
			rows.vals=splice(rows.vals,0,0,searching,NULL);
			e->curs.total.y++;
			e->can_search=1;
		}
		else{
			e->can_search=0;
			vec_free(&searching);
		}
	}

	//update rowid
	for(int i=0; i<rows.vals.len/rows.keys.len; i++){
		rows.vals.var[i*rows.keys.len+rows.keys.len-1].i=e->rowid++;
	}
	e->curs.total.y++;

	e->rs=cross_reinit(e->rs,rows);
}
void browse_setfield(browse* e,map fields){
	map_free_ex(&e->fields,field_free);
	_free(&e->wins);
	e->fields=fields;
	e->curs=(cursor){
		.total.x=fields.keys.len,
		.total.y=e->curs.total.y,
		.limit.len=e->win.height*4,
		.curr.x=min(fields.keys.len,1),
		.cols=ro(fields.keys),
	};
}
void browse_colwidth(browse* e){
	_free(&e->wins);
	e->wins=cols_wins(e->fields.keys,e->rs,e->win);
	cols_aligns(e->fields,e->rs);
}
browse* browse_key(browse* e,int c){
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
			if(ret==1) e->exit=1;
		}
		else{
			string ret=cell_edit(e);
			if(ret.len!=End && e->can_search && e->curs.curr.y==0)
				e->reload=1;
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
int confirm_save(browse* e){
	if(e->save) return 1;
	if(!e->changed.keys.len) return 0;
	if(!e->exit && !e->reload) return 0;

	win_clear(e->win);
	string ans=win_menu(e->win,s_map(c_("yes Save no Discard redit Re-edit")),c_("Save changed?"));
	if(!ans.len){
		if(e->exit){
			map_free_ex(&e->changed,vecpair_free);
		}
		e->reload=0;
		return 0;
	}
	else if(eq_c(ans,"no")){
		map_free_ex(&e->changed,vecpair_free);
		return 0;
	}
	else if(eq_c(ans,"yes")){
		e->save=1;
		return 1;
	}
	e->reload=0;
	e->exit=0;
	return 0;
}
browse* browse_view(browse* e){
	if(!e->wins.len) browse_colwidth(e);
	e->view=cursor_view((int2){.y=max(e->curs.curr.y,0), .x=e->curs.curr.x},e->view,e->win.width,e->wins);
	return e;
}

void table_title(browse* e,string table){
	string title=cat(c_("Table: "),ro(table));
	int half=e->win.width/2;
	win_title(win_resize(e->win,WinRight,-half),title,WinLeft);
	string temp={0};
	if(e->curs.curr.y==0 && e->can_search)
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
void browse_body(browse* e){
	for(int i=e->view.x; i<e->view.x+e->view.width; i++){
		string col=get(e->curs.cols,i);
		int colno=keys_idx(e->rs.rows.keys,e->rs.rows.index,col);
		window* w=e->wins.ptr;
		field f=*(field*)map_getp(e->fields,col);
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
			int rno=j+e->view.y;
			vis_goto(w[i].x,w[i].y+j);

			var rowid=cross_get(e->rs,rno-e->curs.limit.from,Null,0,c_("rowid__"));
			pair* pp=map_getp(e->changed,rowid);
			var changed=pp ? pp->tail.var[colno] : VarEnd;

			string v=get(vs,j);

			int bgchanged=1;
			if(i==e->curs.curr.x && rno==e->curs.curr.y){ //cursor
				s_out(vis_bg(0,180,0));
				s_out(vis_fg(0,0,0));
			}
			else if(e->can_search && rno==0) //search line
				s_out(vis_fg(255,255,255));
			else if(changed.len!=End)
				s_out(vis_bg(90,30,30));
			else if(rno==e->curs.curr.y)
				s_out(vis_bg(30,30,30));
			else 
				bgchanged=0;
	
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
void cell_log(browse* e,string name,string val){
	int rowno=curs_rowno(e->curs);

	if(e->can_search && !rowno) return; //don't log search line
	
	string key=cross_get(e->rs,rowno,Null,0,c_("rowid__"));
	pair* temp=map_add_ex(&e->changed,key,NULL,NULL);
	if(
		rowno<e->curs.total.y-1 //not adding now
		&& !temp->tail.len //neither adding started previously
		&& !temp->head.len
	)
		temp->head=vec_dup(cross_row(e->rs,rowno,Null).vals);
	if(!temp->tail.len){
		vector v=vec_new_ex(sizeof(var),cross_ncols(e->rs));
		for(int i=0; i<v.len; i++) v.var[i].len=End;
		temp->tail=v;
	}
	int nid=cross_colno(e->rs,name);
	if(nid!=End) temp->tail.var[nid]=ro(val);
}
void cell_set(browse* e,string name,string val){
	if(!name.len) name=curs_colname(e->curs);
	cell_log(e,name,val);
	e->rs=cross_set(e->rs,curs_rowno(e->curs),Null,0,name,val);
	if(e->curs.curr.y==e->curs.total.y-1){ //add another newline bellow if last line
		vector row=vec_new_ex(sizeof(var),e->rs.rows.keys.len);
		row.var[row.len-1].i=e->rowid++;
		e->rs=cross_addrow(e->rs, -1, Null, row);
		e->curs.total.y++;
	}
}
string cell_get(browse* e,string name){
	return cross_get(e->rs,curs_rowno(e->curs),Null,0,name.len ? name : curs_colname(e->curs));
}
string cell_combo(browse* e,vector suggests){
	window w=cursor_win(e->curs.curr,e->view,e->wins);
	string colname=curs_colname(e->curs);
	string val=cell_get(e,colname);
	ewin ewin=ewin_new(w,val);
	edit_combo(&ewin,suggests);
	return ewin_close(&ewin);
}
string cell_edit(browse* e){
	window w=cursor_win(e->curs.curr,e->view,e->wins);
	string colname=curs_colname(e->curs);
	string val=cell_get(e,colname);
	ewin ewin=ewin_new(w,val);
	edit_input(&ewin);
	string newval=ewin_close(&ewin);
	if(newval.len!=End) cell_set(e,colname,newval);
	return newval;
}
