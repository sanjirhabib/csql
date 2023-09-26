#include "window.h"
#include "editor.h"
#include "winedit.h"

/*header
typedef struct s_editwin {
	window view;
	window win;
	vector colors;
	string oldtext;
	int key;
	int is_multiline;
	struct s_editor editor;
} editwin;
end*/
window editwin_view(editwin* ewin){
	window view=ewin->view;
	editor* e=&ewin->editor;
	int m25=view.width*0.25;
	int m75=view.width-m25;
	string curr=get(e->lines, e->curr.y);
	int len=display_len(curr);
	int x=display_len(slice(curr,0,e->curr.x));
	if(len>=view.width){
		if(x>view.x+m75)
			view.x=x-m75;
		else if(view.x && x<view.x+m25)
			view.x=max(0, x-m25);
	}
	else if(view.x && x<view.x) view.x=0;

	int y=e->curr.y;
	len=e->lines.len;
	int m50=view.height/2;
	if(len>view.height){
		if(y>m50-1 && y+m50<=len)
			view.y=y-m50+1;
		else if(y<=m50) view.y=0;
		else if(y>len-m50) view.y=len-view.height+1;
	}
	else if(view.y && y<view.y) view.y=0;

	ewin->view=view;
	return view;
}
void add_color(editwin* ewin, int line, int offset, Color color){
	vec_add(ewin->colors.var+line-ewin->view.y,(var){.xy.x=offset, .xy.y=color });
}
void suggest_color(editwin* ewin,options* op){
	if(!op->oplen) return;
	add_color(ewin,ewin->editor.curr.y,ewin->editor.curr.x,ColorSuggest);
	add_color(ewin,ewin->editor.curr.y,ewin->editor.curr.x+op->oplen,ColorEdit);
}
void editwin_color(editwin* ewin){
	window view=editwin_view(ewin);
	each(ewin->colors,i,var* v) _free(v+i);
	if(ewin->editor.selected.x==Fail) return;
	int4 ordered=select_ordered(ewin->editor.selected);
	add_color(ewin,ordered.y,ordered.x,ColorSelect);
	add_color(ewin,ordered.y2,ordered.x2,ColorEdit);
}
void add_suggest(editor* e, options* op){
	int from=e->curr.x;
	if(!from) return;
	string curr=e->lines.var[e->curr.y];
	if(from<curr.len && (is_alpha_char(curr.str[from]))){
		return; // cursor at mid of word, return
	}
	int upto=from;
	while(from>0 && is_alpha_char(curr.str[from-1])) from--;
	if(from==upto){
		return; // nothing typed. return.
	}
	if(!is_alpha_char(curr.str[from])) from++;

	string word=slice(curr,from,e->curr.x-from);
	string match={0};
	int matchno=Fail;
	if(op->opno<0) op->opno=0;
	for(int i=0; i<op->options.len; i++){
		string s=get(op->options,i);
		int matched=0;
		while(matched<word.len && matched<s.len && toupper(word.str[matched])==toupper(s.str[matched])) matched++;
		if(matched==word.len && s.len>word.len){
			match=s;
			matchno++;
			if(matchno==op->opno) break;
		}
	}
	if(matchno!=Fail && matchno!=op->opno && match.len) op->opno=matchno;
	if(word.len<match.len){
		op->oplen=match.len-word.len;
		curr=s_insert(curr,e->curr.x,slice(match,word.len,op->oplen));
		e->lines.var[e->curr.y]=curr;
	}
}
int option_key(editor* e,int c,options* op){
	if(!op->oplen) return c;

	string curr=e->lines.var[e->curr.y];
	if(c=='\t'){
		e->curr.x+=op->oplen;
		op->oplen=0;
		if(!op->opno) add_suggest(e,op);
		op->opno=0;
		return 0;
	}
	curr=s_del(curr,e->curr.x,op->oplen);
	e->lines.var[e->curr.y]=curr;
	op->oplen=0;
	if(c==KeyDown){
		op->opno++;
		add_suggest(e,op);
		return 0;
	}
	else if(c==KeyUp){
		op->opno--;
		add_suggest(e,op);
		return 0;
	}
	op->opno=0;
	return c;
}
editwin* edit_combo(editwin* ewin, vector ops){
	int c=0;
	options op={
		.options=ops,
	};
	do{
		if(c==27 && ewin->editor.selected.x==Fail) break;
		if(!ewin->is_multiline && (c=='\r'||c=='\n')) break;
		if(ewin->is_multiline && c=='s'+KeyCtrl) break;
		c=option_key(&ewin->editor, c, &op);
		editwin_key(ewin, c);
		add_suggest(&ewin->editor, &op);
		editwin_color(ewin);
		suggest_color(ewin,&op);
		editwin_show(ewin);
	} while((c=vis_getch()));
	ewin->key=c;
	return ewin;
}
editwin* edit_input(editwin* ewin){
	int c=0;
	do{
		if(c==27 && ewin->editor.selected.x==Fail) break;
		if(!ewin->is_multiline && (c=='\r'||c=='\n')) break;
		if(ewin->is_multiline  && c=='s'+KeyCtrl) break;
		editwin_key(ewin, c);

		string curr=ewin->editor.lines.var[ewin->editor.curr.y];
		//vis_msg("cursor.x %d line.len %d view.x %d",ewin->editor.curr.x,curr.len,ewin->view.x);

		editwin_color(ewin);
		editwin_show(ewin);
	} while((c=vis_getch()));
	win_clear(ewin->win);
	ewin->key=c;
	return ewin;
}
void editwin_free(editwin* ewin){
	editor_free(&ewin->editor);
	vec_free(&ewin->colors);
	printf("\033[?25l"); //hide cursor
	win_clear(ewin->win);
}
string editwin_close(editwin* ewin){
	string ret={.len=Fail};
	if(ewin->key==27){
		editwin_free(ewin);
		return ret;
	}
	string newtext=editor_get(&ewin->editor);
	if(eq(newtext,ewin->oldtext)){
		_free(&newtext);
		editwin_free(ewin);
		return ret;
	}
	editwin_free(ewin);
	return newtext;
}
int editwin_changed(editwin* ewin){
	string newtext=editor_get(&ewin->editor);
	int ret=eq(newtext,ewin->oldtext);
	_free(&newtext);
	return ret;
}
editwin* editwin_key(editwin* ewin,int c){
	editor_key(&ewin->editor,c);
	return ewin;
}
editwin editwin_new(window win,string text){
	editwin ewin={
		.view={.width=win.width,.height=win.height},
		.win=win,
		.editor=editor_new(),
		.oldtext=text,
		.colors=vec_new_ex(sizeof(var),win.height),
	};
	ewin.editor.lines=s_vec(text,"\n");
	if(ewin.editor.lines.len==1){
		ewin.editor.selected=(int4){.x2=text.len};
		ewin.editor.curr.x=ewin.editor.selected.x2;
	}
	if(ewin.win.height>1)
		ewin.is_multiline=1;
	if(!ewin.editor.lines.len) ewin.editor.lines=vec_all(Null);
	return ewin;
}
int log_show(window win){
	editwin ewin=editwin_new(win,Null);
	_free(&ewin.editor.lines);
	ewin.editor.lines=ro(log_lines);
	ewin.editor.curr.y=log_lines.len-1;
	editwin_view(&ewin);
	editwin_show(&ewin);
	editwin_free(&ewin);
	return 0;
}
int vis_log(string in,window win){
	log_add(in);
	log_show(win);
	return 0;
}
int vis_error(string in,window win){
	vis_log(in,win);
	getchar();
	return 0;
}
void color_print(Color no){
	char* colors[]={
		terminalcodes[VisEdit],
		terminalcodes[VisEdit],
		terminalcodes[VisSelect],
		terminalcodes[VisSuggest],
	};
	printf("%s",colors[no]);
}
void editwin_show(editwin* ewin){
	window view=ewin->view;
	window win=ewin->win;
	editor* e=&ewin->editor;
	vis_print(VisEdit);
	for(int i=0; i<view.height; i++){
		string s=get(e->lines,i+view.y);
		s=slice(s,view.x,view.width);
		string full=s_pad(s,view.width,WinLeft);
		s=ro(full);
		int x=view.x;
		vis_goto(win.x, win.y+i);
		each(ewin->colors.var[i],j,var* color){
			int len=color[j].xy.x-x;
			if(len>0){
				s_out(slice(s,0,len));
				s=slice(s,len,s.len);
				x+=len;
			}
			color_print(color[j].xy.y);
		}
		s_out(s);
		_free(&full);
	}
	vis_print(VisNormal);
	int offset=display_len(slice(get(e->lines,e->curr.y),view.x,e->curr.x-view.x));
	if(e->selected.x!=Fail && !is_reverse_selection(e->selected) && offset) offset--;
	vis_goto(win.x+offset, win.y+e->curr.y-view.y);
	vis_print(VisCursor);
	fflush(stdout);
}
