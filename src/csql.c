#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>


#include "var.h"
#include "map.h"
#include "cross.h"
#include "lite.h"
#include "editor.h"
#include "sql.h"
#include "log.h"
#include "csql.h"

/*header
typedef struct s_range {
	int from;
	int len;
} range;
typedef enum {
	WinTop, WinBottom, WinLeft, WinRight, WinCenter
} Side;
typedef struct s_query {
	var conn;
	string name;
	vector pkeys;
	map cols;
	string sql;
} query;
typedef struct s_options {
	int oplen;
	int opno;
	vector options;
} options;
typedef struct s_editwin {
	window win;
	window view;
	editor e;
} editwin;
typedef struct s_cursor {
	int2 curr;
	int2 total;
	range limit;
	vector cols;
} cursor;
struct changed {
	string oldval;
	string newval;
	int2 cell;
	int updated;
};
typedef enum {
	ColorNormal,
	ColorEdit,
	ColorSelect,
	ColorSuggest,
} Color;
extern vector msg_queue;
extern vector log_lines;
extern int msg_timer;

end*/

static char* terminalcodes[]={ ///typedef enum {
	"\0337",   ///VisSavePos,
	"\0338",   ///VisRestorePos,
	"\033[?47h",   ///VisSaveScr,
	"\033[?47l",   ///VisRestoreScr,
	"\033[?25l",   ///VisNoCursor,
	"\033[?25h",   ///VisCursor,
	"\033[H",   ///VisHome,
	"\033[0;37;44m",   ///VisEdit,
	"\033[0;39;49m",   ///VisNoEdit,
	"\033[0;30;47m",   ///VisSelect,
	"\033[2;36;40m",   ///VisSuggest,
	"\033[0m",	///VisNormal,
	"\033[2J",   ///VisClear,
	"\033[0K",   ///VisClearLine,
	"\033[7m",   ///VisInverse,
	"\033[1m",   ///VisBold,
	"\033[22m",   ///VisNoBold,
	"\033[27m",   ///VisNoInverse,
	"\033[?1049h",  ///VisAltScr,
	"\033[?1049l",  ///VisPriScr
};  ///} Terminal;

int vis_print(Terminal code){
	printf("%s", terminalcodes[code]);
	return 0;
}

int vis_goto(int x, int y){
	printf("\033[%d;%df",y+1,x+1);
}
string vis_s(Terminal code){
	return c_(terminalcodes[code]);
}
string vis_fg(int r,int g,int b){
	print_s("\033[38;2;%d;%d;%dm",r,g,b);
}
string vis_bg(int r,int g,int b){
	print_s("\033[48;2;%d;%d;%dm",r,g,b);
}
int s_out(string in){
	for(int i=0; i<in.len; i++){
		char c=in.str[i];
		switch(c){
			case '\t': printf("%*s",4,""); break;
			default: putchar(c); break;
		}
	}
	//printf("%.*s",ls(in));
	_free(&in);
	return 0;
}
string s_pad(string in, int width, Side align){
	int len=display_len(in);
	int inlen=in.len;
	if(len==width) return in;
	if(len>width) return align==WinLeft ? display_upto(in,width) : display_rupto(in,width);
	int pad=width-len;
	string ret=print_s("%*s%.*s%*s",align==WinRight ? pad : 0,"",ls(in),align==WinLeft ? pad : 0,"");
	_free(&in);
	return ret;
}
int display_len(string in){
	int ret=0;
	for(int i=0; i<in.len; i++){
		char c=in.str[i];
		if(c=='\t') ret+=4;
		else if((c & 0xC0)!=0x80) ret++;
	}
	return ret;
}
string display_rupto(string in,int len){
	int ret=0;
	for(int i=0; i<in.len; i++){
		int i2=in.len-1-i;
		char c=in.str[i2];
		if(c=='\t') ret+=4;
		else if((c & 0xC0)!=0x80) ret++;
		if(ret>len) return slice(in,i,in.len);
	}
	return in;
}
string display_upto(string in,int len){
	int ret=0;
	for(int i=0; i<in.len; i++){
		char c=in.str[i];
		if(c=='\t') ret+=4;
		else if((c & 0xC0)!=0x80) ret++;
		if(ret>len) return slice(in,0,i);
	}
	return in;
}
int text_out(string in, int width,Side align){
	int len=display_len(in);
	int space=width-len;
	string sub=display_upto(in,width);
	int space1=0;
	int space2=0;
	if(space){
		if(align==WinRight) space1=space;
		else if(align==WinLeft) space2=space;
		else{
			space1=space2=space/2;
		}
	}
	printf("%*s%.*s%*s"
		, space1, ""
		, sub.len,sub.str
		, space2, ""
	);
	_free(&in);
	return 0;
}
int2 vis_size(){
	struct winsize w;
	ioctl(0,  TIOCGWINSZ, &w);
	return (int2){.x=w.ws_col,  .y=w.ws_row};
}


var vis_init(){
	vis_print(VisSavePos);
	vis_print(VisSaveScr);
	vis_print(VisNoCursor);
	static struct termios oldt,  newt;
	tcgetattr(0,  &oldt);
	cfmakeraw(&newt);
	tcsetattr(0,  TCSANOW, &newt);
	vis_print(VisClear);
	msg_queue=vec_new();
	log_lines=vec_new();
	msg_timer=0;
	return (var){ .ptr=to_heap(&oldt, sizeof(oldt)), .len=IsPtr };
}
int vis_restore(var old){
	tcsetattr(0,  TCSANOW, old.ptr);
	free(old.ptr);
	vis_print(VisCursor);
	vis_print(VisRestoreScr);
	vis_print(VisRestorePos);
	vec_free(&msg_queue);
	vec_free(&log_lines);
    return 0;
}
int stdin_more(int milis){
	fd_set fds={0};
	FD_ZERO(&fds);
	FD_SET(0,  &fds);
	struct timeval timeout={.tv_usec=milis*1000};
	int ready=select(1,  &fds, NULL, NULL, &timeout);
	return ready;
}
int sec_timer(){
	vis_show();
	return 0;
}


var keybuff={0};
int add_key(int c){
	if(!keybuff.datasize) keybuff=vec_new();
	if(keybuff.len>100) return -1;
	vec_add(&keybuff, i_(c));
	return 1;
}

#define c_i(x) (*(int*)x)

int vim_getch(){
	char* combos[]={
		"dd  ","ciw ","gg  "
	};
	int seq=0;
	char buff[5];
	memcpy(buff,"    ",5);
	int c=0;
	while(1){
		c=vis_getch();
		if(c<' '||c>'~') break;
		buff[seq++]=c;
		c=0;
		int found=0;
		for(int i=0; i<sizeof(combos)/sizeof(char*); i++){
			int j=0;
			for(j=0; j<4; j++){
				if(buff[j]!=combos[i][j]) break;
			}
			if(j==4) return c_i(buff);
			if(buff[j]==' ') found=i+1;
		}
		if(!found||seq==4) break;
		if(!stdin_more(500)) break;
	}
	if(!seq) return c;
	for(int i=1; i<seq; i++){
		add_key(buff[i]);
	}
	if(c) add_key(c);
	return buff[0];
}
int vis_getch(){
	int c=0;
	if((c=get(keybuff, 0).i)){
		vec_del_ex(&keybuff, 0,1,NULL);
		return c;
	}

	while(!stdin_more(1000)) sec_timer();
	read(fileno(stdin), &c, 1);
	sec_timer();

	if(c>=32) return c;
	if(c=='\n' || c=='\r' || c==0 || c==8 || c=='\t') return c;
	if(c!=27){
		return KeyCtrl+c+96;
	}
	if(!stdin_more(0)) return c;
	read(fileno(stdin),  &c, 1);
	if(c!='[') return c;
	int idx=0;
	char buff[16];
	while(!(c>='a' && c<='z' || c>='A' && c<='Z'||c=='~')){
		buff[idx++]=c;
		read(fileno(stdin),  &c, 1);
		if(idx>=sizeof(buff)-2) break;
	}
	buff[idx++]=c;
	buff[idx++]='\0';
	struct s_seqcode {
		char* seq;
		Keyboard code;
	};
	struct s_seqcode codes[]={
		{"[A", KeyUp},
		{"[B",KeyDown},
		{"[C",KeyRight},
		{"[D",KeyLeft},
		{"[F",KeyEnd},
		{"[H",KeyHome},
		{"[3~",KeyDel},
		{"[3;5~",KeyCtrlDel},
		{"[1;5C",KeyCtrlRight},
		{"[1;5D",KeyCtrlLeft},
		{"[1;2C",KeyShiftRight},
		{"[1;2D",KeyShiftLeft},
		{"[1;6C",KeyShiftCtrlRight},
		{"[1;6D",KeyShiftCtrlLeft},
		{"[1;2A",KeyShiftUp},
		{"[1;2B",KeyShiftDown},
		{"[Z",KeyShiftTab},
		{"[1;5H",KeyCtrlHome},
		{"[1;5F",KeyCtrlEnd},
		{"[5~",KeyPgUp},
		{"[6~",KeyPgDown}
	};
	for(int i=0; i<sizeof(codes)/sizeof(struct s_seqcode); i++){
		if(strcmp(codes[i].seq, buff)==0) return codes[i].code;
	}
	vis_msg("Key: %s", buff);
	return KeyOther;
}
int win_clear(window win){
	for(int i=0; i<win.height; i++){
		vis_goto(win.x, win.y+i);
		printf("%*s", win.width,"");
	}
	return 0;
}
int compare_int(const void* v1,  const void* v2){
	int n1=*(int*)v1;
	int n2=*(int*)v2;
	return n1>n2 ? 1 : (n1<n2 ? -1 : 0);
}
vector vec_sub(vector in, int offset,int len){
	if(offset+len>in.len) len=in.len-offset;
	vector ret=vec_new_ex(in.datasize, len);
	memcpy(ret.str, in.str+offset*in.datasize,in.datasize*len);
	return ret;
}
vector vec_ro(vector in){
	for(int i=0; i<in.len; i++){
		in.var[i].readonly=1;
	}
	return in;
}
int is_numeric(string in){
	if(!in.len) return 0;
	for(int i=0; i<in.len; i++){
		if(!is_numeric_char(in.str[i])) return 0;
	}
	return 1;
}
Side col_align(vector vals){
	for(int i=0; i<vals.len && i<10; i++){
		if(is_numeric(get(vals, i))) return WinRight;
	}
	return WinLeft;
}
int col_width(string name,  vector vals){
	if(!vals.len) return 16;
	int total=min(vals.len,100);
	vector temp=vec_new_ex(sizeof(int),total);
	int* ip=temp.ptr;
	for(int i=0; i<total; i++){
		ip[i]=display_len(vals.var[i]);
	}
	qsort(temp.str, temp.len,temp.datasize,compare_int);
	int ret=ip[(int)(total*0.95)];
	_free(&temp);
	ret=max(ret, name.len);
	ret=max(ret, 8);
	return min(40, ret);
}
map only_cols(map row, vector cols){
	map ret=map_new();
	for(int i=0; i<cols.len; i++){
		string name=get(cols, i);
		map_add(&ret, name,map_get(row,name));
	}
	return ret;
}
field vec_field(vector in){
	return (field){
		.name=in.var[0],
		.type=in.var[1],
		.create=in.var[2],
		.pkey=_i(in.var[3]),
		.unique=_i(in.var[4]),
		.index=_i(in.var[5]),
		.meta=in.var[6],
	};
}
vector field_vec(field* f){
	return vec_all(
		f->name,
		f->type,
		f->create,
		i_s(f->pkey),
		i_s(f->unique),
		i_s(f->index),
		Null
	);
}

void field_dump(field* f){
	msg("{\n\tname: %.*s", ls(f->name));
	msg("\ttype: %.*s", ls(f->type));
	msg("\tcreate: %.*s", ls(f->create));
	msg("\tpkey: %d", f->pkey);
	msg("\tunique: %d", f->unique);
	msg("\tindex: %d", f->index);
	msg("}");
}
vector vals_vec(map in, int colno){
	return slice(in.vals, colno*in.keys.len,in.keys.len);
}
map cols_aligns(map cols,cross data){
	field* f=cols.vals.ptr;
	for(int i=0; i<cols.keys.len; i++){
		f[i].align=col_align(cross_col(data,0,cols.keys.var[i]));
	}
	return cols;
}
vector cols_wins(vector cols,cross rs, window win){
	map widths=cols_widths(ro(cols),rs,win.width);
	int ncols=cols.len;
	vector wins=vec_new_ex(sizeof(window),ncols);
	window wincol=win;
	wincol=win_resize(wincol,WinTop,-2);
	for(int i=0; i<ncols; i++){
		window* ws=wins.ptr;
		wincol.width=widths.vals.ints[i];
		ws[i]=wincol;
		wincol.x+=wincol.width;
	}
	map_free_ex(&widths,NULL);
	return wins;
}
map cols_widths(vector names, cross data,int width){
	vector ret=vec_new_ex(sizeof(int), names.len);
	int total=0;
	for(int i=0; i<names.len; i++){
		string name=get(names, i);
		int w=1+col_width(name,cross_col(data,0,name));
		total+=w;
		ret.ints[i]=w;
	}
	double mul=(double)(width-names.len)/total;
	if(mul>2) mul=2;
	if(mul>=1.1){
		for(int i=0; i<ret.len; i++){
			ret.ints[i]=(double)ret.ints[i]*mul;
		}
	}
	return (map){
		.keys=names,
		.vals=ret,
		.index=keys_index(names),
	};
}
map rows_cols(map rows){
	vector vals=vec_new_ex(sizeof(field), rows.keys.len);
	field* p=vals.ptr;
	for(int i=0; i<rows.keys.len; i++){
		string name=get(rows.keys, i);
		field fld={
			.name=name,
			.type=c_("text"),
			.pkey=i ? 0 : 1,
			.title=name_title(name)
		};
		p[i]=fld;
	}
	return (map){
		.keys=ro(rows.keys),
		.vals=vals,
		.index=ro(rows.index)
	};
}
int vim_char(int c){
	switch(c){
		case 'j' : return KeyDown;
		case 'k' : return KeyUp;
		case 'h' : return KeyLeft;
		case 'b' : return KeyLeft;
		case 'l' : return KeyRight;
		case 'w' : return KeyRight;
		case 'q' : return 27;
		case 'i' : return '\n';
		case 'a' : return '\n';
		case 'c' : return '\n';
		case 'C' : return '\n';
		case '\r' : return '\n';
		case '0' : return KeyHome;
		case '$' : return KeyEnd;
		case KeyCtrl+'b' : return KeyPgUp;
		case KeyCtrl+'f' : return KeyPgDown;
		case 'G' : return KeyCtrlEnd;
	}
	if(c==c_i("gg  ")) return KeyCtrlHome;
	if(c==c_i("dd  ")) return KeyCtrlDel;
	if(c==c_i("ciw  ")) return '\n';
	return c;
}
void query_free(query* in){
	_free(&in->name);
	_free(&in->sql);
	vec_free(&in->pkeys);
	map_free_ex(&in->cols, field_free);
}
int2 cursor_key(int2 curs,int c,int height,int4 range){
	switch(c){
		case KeyUp: curs.y--; break;
		case KeyDown: curs.y++; break;
		case KeyRight: curs.x++; break;
		case KeyEnd: curs.x=range.x2-1; break;
		case KeyHome: curs.x=0; break;
		case KeyLeft: curs.x--; break;
		case KeyCtrl+'u': curs.y-=height/2; break;
		case KeyCtrl+'d': curs.y+=height/2; break;
		case KeyPgUp: curs.y-=height; break;
		case KeyPgDown: curs.y+=height; break;
		case KeyCtrlHome: curs.y=0; break;
		case KeyCtrlEnd: curs.y=range.y2-1; break;
	}
	curs.y=between(range.y,curs.y,range.y2-1);
	curs.x=between(range.x,curs.x,range.x2-1);
	return curs;
}
int view_width(int colx,int width,vector wins,int side){
	window* winp=wins.ptr;
	int ret=0;
	int w=0;
	for(int i=colx; i<wins.len && i>=0 && w<width-winp[i].width; i+=side){
		w+=winp[i].width;
		ret++;
	}
	return ret;
}
window cursor_view(int2 curr,window view,int width,vector wins){
	view.y=between(curr.y-view.height+1,view.y,curr.y);
	if(!width) return view;
	view.width=view_width(view.x,width,wins,1);
	if(curr.x<view.x){
		view.x=curr.x;
		view.width=view_width(view.x,width,wins,1);
	}
	else if(curr.x>=view.x+view.width){
		view.width=view_width(curr.x,width,wins,-1);
		view.x=curr.x-view.width+1;
	}
	window* wp=wins.ptr;
	wp[view.x].x=wp[0].x;
	for(int i=view.x+1; i<view.x+view.width; i++){
		wp[i].x=wp[i-1].x+wp[i-1].width;
	}
	return view;
}
vector view_vals(window view,cross rs,cursor curs,int col){
	vector ret=cross_col(rs,0,get(curs.cols,col));
	return slice(ret,view.y-curs.limit.from,view.height);
}
int win_column(window win,vector vals,Side align,int cursor,string hicolor){
	int i=0;
	for(i=0; i<win.height; i++){
		vis_goto(win.x,win.y+i);
		if(cursor==i) printf("%.*s",ls(hicolor));
		string v=get(vals,i);
		text_out(ellipsis(v,win.width-1),win.width-1,align);
		printf(" ");
		if(cursor==i) vis_print(VisNormal);
	}
	_free(&hicolor);
}
window win_resize(window win,Side side,int len){
	switch(side){
	case WinTop:
		win.y-=len;
		win.height+=len;
		break;
	case WinBottom:
		win.height+=len;
		break;
	case WinLeft:
		win.x+=len;
		win.width+=len;
		break;
	case WinRight:
		win.width+=len;
		break;
	}
	return win;
}
string ellipsis(string in,int width){
	if(display_len(in)<=width) return in;
	string sub=display_upto(in,width-2);
	string ret=print_s("%.*s..",ls(sub));
	_free(&in);
	return ret;
}
int win_title(window win,string title,Side align){
	vis_goto(win.x,win.y);
	vis_print(VisBold);
	text_out(ellipsis(title,win.width-1),win.width-1,align);
	printf(" ");
	vis_print(VisNoBold);
	return 0;
}
string win_menu(window win, map vals,string title){
	int c=0;
	cursor curs={
		.total.y=vals.keys.len,
		.total.x=1,
	};
	window view={
		.height=win.height,
	};
	window body=win_resize(win,WinTop,-1);
	window head=win;
	head.width=min(30,head.width);
	body.width=min(30,body.width);
	string ret={0};
	do{
		curs.curr=cursor_key(curs.curr,c,view.height,(int4){.x=0,.x2=curs.total.x,.y=-1,.y2=curs.total.y});
		view=cursor_view((int2){.y=max(curs.curr.y,0), .x=curs.curr.x},view,0,Null);
		vector v=view_vals(view,(cross){.rows=vals,.vals=vals},curs,0);
		if(curs.curr.y==-1) vis_print(VisInverse);
		win_title(head,title,WinCenter);
		if(curs.curr.y==-1) vis_print(VisNormal);
		win_column(body,v,WinLeft,curs.curr.y-view.y,vis_s(VisInverse));
		if(c==27) break;
		else if(c=='\n'){
			ret=get(vals.keys,curs.curr.y);
			break;
		}
		fflush(stdout);
	}while((c=vim_char(vim_getch())));
	win_clear(win);
	map_free(&vals);
	return ret;
}
window cursor_win(int2 curr, window view, vector wins){
	window* winp=wins.ptr;
	window ret=winp[curr.x-view.x];
	ret.y+=curr.y-view.y;
	ret.height=1;
	return ret;
}
int rows_idx(map rows,int row,int col){
	return rows.keys.len*row+col;
}
int vals_idx(map vals,int row,int col){
	return vals.keys.len*col+row;
}
window editor_view(editor* e){
	window view=e->view;
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
	view.y=max(0,e->curr.y-view.height/2);
	if(view.y && view.y+view.height>e->lines.len+1) view.y=max(0,e->lines.len+1-view.height);
	e->view=view;
	return view;
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
void add_color(editor* e, int line, int offset, Color color){
	vec_add(e->colors.var+line-e->view.y,(var){.xy.x=offset, .xy.y=color });
}
void suggest_color(editor* e,options* op){
	if(!op->oplen) return;
	add_color(e,e->curr.y,e->curr.x,ColorSuggest);
	add_color(e,e->curr.y,e->curr.x+op->oplen,ColorEdit);
}
void editor_color(editor* e){
	window view=editor_view(e);
	each(e->colors,i,var* v) _free(v+i);
	if(e->selected.x==None) return;
	int4 ordered=select_ordered(e->selected);
	add_color(e,ordered.y,ordered.x,ColorSelect);
	add_color(e,ordered.y2,ordered.x2,ColorEdit);
}
void editor_show(editor* e,window win){
	window view=e->view;
	vis_print(VisEdit);
	for(int i=view.y; i<view.y+view.height; i++){
		vis_goto(win.x,win.y+i-view.y);
		string s=get(e->lines,i);
		s=slice(s,view.x,view.width);
		string full=s_pad(s,view.width,WinLeft);
		s=ro(full);
		int x=view.x;
		each(e->colors.var[i-view.y],j,var* color){
			int len=color[j].xy.x-x;
			s_out(slice(s,0,len));
			s=slice(s,len,s.len);
			color_print(color[j].xy.y);
			x+=len;
		}
		s_out(s);
		_free(&full);
	}
	vis_print(VisNormal);
	int offset=display_len(slice(get(e->lines,e->curr.y),view.x,e->curr.x));
	if(e->selected.x!=None && !is_reverse_selection(e->selected) && offset) offset--;
	vis_goto(win.x+offset, win.y+e->curr.y-view.y);
	vis_print(VisCursor);
	fflush(stdout);
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
	int matchno=None;
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
	if(matchno!=None && matchno!=op->opno && match.len) op->opno=matchno;
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
string edit_combo(window win, string old, string text, vector ops){
	int c=0;
	options op={
		.options=ops,
	};
	editor e=editor_new(win);
	e.lines=s_vec(text,"\n");
	e.selected=(int4){.x2=text.len};
	e.curr.x=e.selected.x2;
	do{
		if(c==27 && e.selected.x==None) break;
		if(win.height==1 && (c=='\r'||c=='\n')) break;
		if(win.height>1 && c=='s'+KeyCtrl) break;
		c=option_key(&e, c, &op);
		editor_key(&e, c);
		add_suggest(&e, &op);
		editor_color(&e);
		suggest_color(&e,&op);
		editor_show(&e,win);
	} while((c=vis_getch()));
	win_clear(win);
	if(win.height==1 && c==27){
		editor_free(&e);
		return old;
	}
	return editor_close(&e);
}
string edit_input(window win, string old, string text){
	int c=0;
	editor e=editor_new(win);
	e.lines=s_vec(text,"\n");
	e.selected=(int4){.x2=text.len};
	e.curr.x=e.selected.x2;
	do{
		if(c==27 && e.selected.x==None) break;
		if(win.height==1 && (c=='\r'||c=='\n')) break;
		if(win.height>1 && c=='s'+KeyCtrl) break;
		editor_key(&e, c);
		editor_color(&e);
		editor_show(&e,win);
	} while((c=vis_getch()));
	win_clear(win);
	if(win.height==1 && c==27){
		editor_free(&e);
		return old;
	}
	return editor_close(&e);
}
int curs_rowno(cursor in){
	return in.curr.y-in.limit.from;
}
string curs_colname(cursor in){
	return get(in.cols,in.curr.x);
}
