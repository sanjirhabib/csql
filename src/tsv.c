#include "browse.h"
#include "file.h"
#include "log.h"
#include "tsv.h"

string tsv_reload(browse* e,string filename,cross types,string buff){
	if(!e->reload) return buff;
	e->reload=0;
	_free(&buff);
	string in_s=file_s(filename);
	if(!in_s.len) return Null;
	map rows=s_rows(in_s);
	e->curs.total.y=rows.vals.len/rows.keys.len;
	browse_setrows(e,rows);
	browse_setfield(e,cross_fields(e->rs, types));
	e->curs.total.x--;

	return in_s;
}
int tsv_save(map in,string filename){
	vector lines=vec_new(sizeof(var),in.vals.len/in.keys.len);
	vector v=in.keys;
	v.len--;
	lines.var[0]=vec_s(vec_escape(v),"\t");
	for(int i=0; i<in.vals.len/in.keys.len-1; i++){
		v=rows_row(in,i).vals;
		v.len--;
		lines.var[i+1]=vec_s(vec_escape(v),"\t");
	}
	return s_save(vec_s(lines,"\n"),filename);
}
int tsv_browse(string filename,window win,cross types){
	browse e=browse_new();
	browse_setwin(&e,win);
	e.reload=1;
	int ret=0;
	int c=0;
	string buff=Null;
	do{
		browse_key(&e,c);
		confirm_save(&e);
		if(e.save){
			e.save=0;
			ret=tsv_save(e.rs.rows,filename);
			if(ret){
				map_free_ex(&e.changed,vecpair_free);
			}
			else{
				e.reload=0;
				e.exit=0;
			}
		}
		if(e.exit) break;
		buff=tsv_reload(&e,filename,types,buff);
		log_show(e.win);
		browse_view(&e);
		file_title(&e,filename);
		browse_body(&e);
		fflush(stdout);
	} while((c=vim_char(vim_getch())));
	browse_free(&e);
	_free(&buff);
	win_clear(e.win);
	return 0;
}
void file_title(browse* e,string filename){
	string title=cat(c_("File: "),ro(filename));
	int half=e->win.width/2;
	win_title(win_resize(e->win,WinRight,-half),title,WinLeft);
	string temp={0};
	if(e->curs.curr.y==-1)
		temp=c_("Setup");
	else if(e->curs.curr.y==e->curs.total.y-1)
		temp=c_("+Add New");
	else
		temp=print_s("Line: %d/%d",e->curs.curr.y,e->curs.total.y-2);
	vis_goto(e->win.x+half,e->win.y);
	s_out(s_pad(temp,half,WinRight));
}
