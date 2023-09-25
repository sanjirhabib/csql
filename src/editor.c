#include "var.h"
#include "editor.h"

/*header
typedef struct s_int4 {
	int x;
	int y;
	int x2;
	int y2;
} int4;
typedef struct s_window {
	int x;
	int y;
	int width;
	int height;
} window;
typedef enum {
	KeyDown=300, KeyUp, KeyRight, KeyLeft, 
	KeyHome, KeyEnd, KeyCtrlLeft, KeyCtrlRight, KeyDel,
	KeyCtrlDel,
	KeyShiftLeft, KeyShiftRight, KeyShiftDown, KeyShiftUp,
	KeyShiftTab, KeyShiftCtrlRight, KeyShiftCtrlLeft,
	KeyCtrlHome, KeyCtrlEnd, KeyPgUp, KeyPgDown,
	KeyCtrl, KeyOther
} Keyboard;
typedef enum EditType { EditFail, EditInsert, EditDelete } EditType;
typedef struct s_edittask {
	int slno;
	EditType type;
	int x;
	int y;
	int x2;
	int y2;
	string text;
} edittask;
typedef struct s_editor {
	window view;
	vector colors;
	int4 selected;
	int2 curr;
	vector lines;
	int editing;
	string clipboard;
	vector undobuff;
	int undono;
} editor;
enum {
	EditNone,
	EditCancel,
	EditSave,
};
end*/

edittask apply_edit(edittask task,editor* e){
	if(task.type==EditDelete){
		int lastlinelen=e->lines.var[task.y2].len;
		vector deletedlines=slice(e->lines,task.y,task.y2-task.y+1);
		string textall=vec_s(deletedlines,"\n");
		int deletedlen=textall.len-task.x-lastlinelen+task.x2; //+deletedlines.len-1;
		task.text=s_own(slice(textall,task.x,deletedlen));
		string remaining=s_own(cat(slice(textall,0,task.x),slice(textall,task.x+deletedlen,textall.len-task.x-deletedlen)));
		// delete all lines, expect one
		vec_del_ex(&e->lines,task.y,deletedlines.len-1,_free);
		_free(&e->lines.var[task.y]);
		_free(&textall);
		e->lines.var[task.y]=remaining;
		e->curr.x=task.x;
		e->curr.y=task.y;
	}
	else if(task.type==EditInsert){
		vector lines=s_vec(task.text,"\n");
		string currline=e->lines.var[task.y];
		lines.var[0]=cat(slice(currline,0,task.x),lines.var[0]);
		lines.var[lines.len-1]=cat(lines.var[lines.len-1],slice(currline,task.x,currline.len-task.x));
		vec_own(lines);
		task.y2=e->curr.y=task.y+lines.len-1;
		task.x2=e->curr.x=lines.var[lines.len-1].len-currline.len+task.x;
		e->lines=splice(e->lines,task.y,1,lines,_free);
	}
	return task;
}
editor* edit_add(int x,int y,string in,editor* e,int sl){
	edittask task=apply_edit((edittask){
		.slno=sl,
		.type=EditInsert,
		.x=x,
		.y=y,
		.text=in,
	},e);
	return undo_add(task,e);
}
void task_free(void* in){
	edittask* temp=in;
	_free(&temp->text);
}
editor* undo_add(edittask task,editor* e){
	if(e->undono){
		vec_del_ex(&e->undobuff,e->undobuff.len-e->undono,e->undono,task_free);
		e->undono=0;
	}
	task.text=s_own(task.text);
	vec_addp(&e->undobuff,&task);
	return e;
}
editor* edit_del(int x,int y,int x2,int y2,editor* e,int sl){
	edittask task=apply_edit((edittask){
		.slno=sl,
		.type=EditDelete,
		.x=x,
		.y=y,
		.x2=x2,
		.y2=y2,
	},e);
	return undo_add(task,e);
}
editor* editor_readonly(editor* e,int c){
	if((c>=' ' && c<256)||strchr("\t\n\r",c)) return e;
	return editor_key(e,c);
}
editor* editor_key(editor* e,int c){
	string curr=e->lines.var[e->curr.y];

	if(e->selected.x==Fail && (c==KeyShiftCtrlLeft||c==KeyShiftCtrlRight||c==KeyShiftLeft||c==KeyShiftRight||c==KeyShiftDown||c==KeyShiftUp)){
		e->selected.x=e->curr.x;
		e->selected.y=e->curr.y;
	}
	if(c==27 && e->selected.x!=Fail){
		e->selected.x=Fail;
	}
	else if(e->selected.x!=Fail && (c==KeyLeft||c==KeyUp)){
		int4 s=select_ordered(e->selected);
		e->curr.x=s.x;
		e->curr.y=s.y;
		e->selected.x=Fail;
	}
	else if(e->selected.x!=Fail && (c==KeyRight||c==KeyDown)){
		int4 s=select_ordered(e->selected);
		e->curr.x=s.x2;
		e->curr.y=s.y2;
		e->selected.x=Fail;
	}
	else if(c==KeyLeft||c==KeyShiftLeft){
		if(e->curr.x||e->curr.y){
			if(e->curr.x) e->curr.x--;
			else{
				e->curr.y--;
				e->curr.x=get(e->lines,e->curr.y).len;
			}
		}
	}
	else if(c==KeyRight||c==KeyShiftRight){
		if(e->curr.x<curr.len) e->curr.x++;
		else if(e->curr.y<e->lines.len-1){
			e->curr.y++;
			e->curr.x=0;
		}
	}
	else if(c==KeyDown||c==KeyShiftDown){
		if(c==KeyShiftDown && e->curr.y==e->lines.len-1){
			e->curr.x=e->lines.len;
		}
		else if(e->curr.y<e->lines.len-1){
			// select whole line if downarrow and cursor was at the indention level.
			int level=indent_level(curr);
			if(c==KeyShiftDown && e->selected.y==e->curr.y && e->selected.x<=level) e->selected.x=0;

			e->curr.y++;
			string line2=get(e->lines,e->curr.y);
			int level2=indent_level(line2);
			if(e->curr.x>=level) e->curr.x+=level2-level;
			e->curr.x=between(0,e->curr.x,line2.len);
		}
	}
	else if(c==KeyUp||c==KeyShiftUp){
		if(e->curr.y>0){
			e->curr.y--;
			int level=indent_level(curr);
			string line2=get(e->lines,e->curr.y);
			int level2=indent_level(line2);
			if(e->curr.x>=level) e->curr.x+=level2-level;
			e->curr.x=between(0,e->curr.x,line2.len);
		}
	}
	else if(c==KeyCtrl+'c'){ //copy
		if(e->selected.x!=Fail){
			_free(&e->clipboard);
			int4 s=select_ordered(e->selected);
			string textall=vec_s(slice(e->lines,s.y,s.y2-s.y+1),"\n");
			int deletedlen=textall.len-s.x-e->lines.var[s.y2].len+s.x2;
			e->clipboard=_dup(slice(textall,s.x,deletedlen));
			e->selected.x=Fail;
		}
	}
	else if(c==KeyCtrl+'x'){ //cut
		if(e->selected.x!=Fail){
			int4 s=select_ordered(e->selected);
			edit_del(s.x,s.y,s.x2,s.y2,e,0);
			_free(&e->clipboard);
			e->clipboard=_dup(((edittask*)getp(e->undobuff,e->undobuff.len-1))->text);
			e->selected.x=Fail;
		}
	}
	else if(c==127||(c=='\b' && e->selected.x!=Fail)){//backspace
		if(e->selected.x==Fail){
			if(e->curr.x||e->curr.y){
				int fromx=e->curr.x-1;
				int fromy=e->curr.y;
				if(fromx==-1){
					fromy--;
					fromx=e->lines.var[fromy].len;
				}
				edit_del(fromx,fromy,e->curr.x,e->curr.y,e,0);
			}
		}
		else{
			int4 s=select_ordered(e->selected);
			edit_del(s.x,s.y,s.x2,s.y2,e,0);
			e->selected.x=Fail;
		}
	}
	else if(c==KeyDel){ //delete
		if(e->selected.x==Fail){
			if(e->curr.x<e->lines.var[e->curr.y].len || e->curr.y<e->lines.len)
				edit_del(e->curr.x,e->curr.y,e->curr.x+1,e->curr.y,e,0);
		}
		else{
			int4 s=select_ordered(e->selected);
			edit_del(s.x,s.y,s.x2,s.y2,e,0);
			e->selected.x=Fail;
		}
	}
	else if(c==KeyCtrl+'v'){ //paste
		if(e->clipboard.len){
			int sl=0;
			if(e->selected.x!=Fail){
				int4 s=select_ordered(e->selected);
				edit_del(s.x,s.y,s.x2,s.y2,e,sl++);
				e->selected.x=Fail;
			}
			e=edit_add(e->curr.x,e->curr.y,ro(e->clipboard),e,sl);
		}
	}
	else if(c==KeyCtrl+'a'){
		e->selected=(int4){.x=0,.y=0,.y2=e->lines.len-1,.x2=get(e->lines,e->lines.len-1).len};
		e->curr.x=e->selected.x2;
		e->curr.y=e->selected.y2;
	}
	else if(c==KeyShiftTab && e->selected.x!=Fail){
		int4 s=select_ordered(e->selected);
		int2 cursorpos=e->curr;
		for(int i=s.y; i<s.y2; i++){
			string s2=e->lines.var[i];
			if(!s2.len || s2.str[0]!='\t') continue;
			edit_del(0,i,1,i,e,i-s.y);
		}
		if(e->selected.x>0) e->selected.x--;
		e->curr=cursorpos;
	}
	else if(c=='\t' && e->selected.x!=Fail){
		int4 s=select_ordered(e->selected);
		int2 cursorpos=e->curr;
		for(int i=s.y; i<s.y2; i++){
			e=edit_add(0,i,c_("\t"),e,i-s.y);
		}
		e->curr=cursorpos;
	}
	else if(c==KeyHome){
		e->curr.x=0;
	}
	else if(c==KeyEnd){
		e->curr.x=curr.len;
	}
	else if(c==KeyCtrlHome||c==KeyPgUp){
		e->curr.x=0;
		e->curr.y=0;
	}
	else if(c==KeyCtrlEnd||c==KeyPgDown){
		e->curr.y=e->lines.len-1;
		if(c==KeyCtrlEnd) e->curr.x=get(e->lines,e->curr.y).len;
	}
	else if(c==KeyCtrlRight||c==KeyShiftCtrlRight){
		if(e->curr.x<curr.len||e->curr.y<e->lines.len-1){
			if(e->curr.x==curr.len){
				e->curr.y++;
				e->curr.x=0;
				curr=get(e->lines,e->curr.y);
			}
			else
				while(e->curr.x<curr.len && is_word_char(curr.str[e->curr.x])) e->curr.x++;
			while(e->curr.x<curr.len && !is_word_char(curr.str[e->curr.x])) e->curr.x++;
		}
	}
	else if(c==KeyCtrlLeft||c==KeyShiftCtrlLeft||c=='\b'){
		if(e->curr.x||e->curr.y){
			int fromx=e->curr.x;
			int fromy=e->curr.y;
			if(!e->curr.x){ //move to line above if cursor is at the beginning
				e->curr.y--;
				curr=get(e->lines,e->curr.y);
				e->curr.x=curr.len;
			}
			else{
				if(e->curr.x>=curr.len) e->curr.x--;
				else while(e->curr.x>0 && is_word_char(curr.str[e->curr.x])) e->curr.x--;
				while(e->curr.x>0 && !is_word_char(curr.str[e->curr.x])) e->curr.x--;
				while(e->curr.x>0 && is_word_char(curr.str[e->curr.x-1])) e->curr.x--;
			}
			if(c=='\b'){
				edit_del(e->curr.x,e->curr.y,fromx,fromy,e,0);
			}
		}
	}
	else if(c==KeyCtrl+'y'){ //redo
		int len=e->undobuff.len;
		while(e->undono){
			edittask task=*(edittask*)getp(e->undobuff,len-e->undono);
			edittask ret=apply_edit(task,e);
			if(task.type==EditDelete) task_free(&ret);
			e->undono--;
			if(!e->undono) break;
			task=*(edittask*)getp(e->undobuff,len-e->undono);
			if(!task.slno) break;
		}
	}
	else if(c==KeyCtrl+'z'){ //undo
		int len=e->undobuff.len;
		while(len>e->undono){
			e->undono++;
			edittask task=*(edittask*)getp(e->undobuff,len-e->undono);
			if(task.type==EditInsert) task.type=EditDelete;
			else if(task.type==EditDelete) task.type=EditInsert;
			edittask ret=apply_edit(task,e);
			if(task.type==EditDelete) task_free(&ret);
			if(!task.slno) break;
		}
	}
	else if(c=='\r'||c=='\n'){//newline
		int slno=0;
		if(e->selected.x!=Fail){
			int4 s=select_ordered(e->selected);
			edit_del(s.x,s.y,s.x2,s.y2,e,slno++);
			e->selected.x=Fail;
		}
		int indent=indent_level(curr);
		int oldx=e->curr.x;
		e=edit_add(e->curr.x,e->curr.y,c_("\n"),e,slno++);
		if(indent && oldx>=indent)
			e=edit_add(0,e->curr.y,slice(e->lines.var[e->curr.y-1],0,indent),e,slno++);
	}
	else if(c==KeyOther){
	}
	else if(c>=' ' && c<256 || c=='\t'){
		int slno=0;
		if(e->selected.x!=Fail){
			int4 s=select_ordered(e->selected);
			edit_del(s.x,s.y,s.x2,s.y2,e,slno++);
			e->selected.x=Fail;
		}
		char temp[2]={c,0};
		e=edit_add(e->curr.x,e->curr.y,c_(temp),e,slno);
	}
	if(e->selected.x!=Fail){
		e->selected.x2=e->curr.x;
		e->selected.y2=e->curr.y;
	}

	return e;
}
int is_reverse_selection(int4 in){
	if(in.y<in.y2 || (in.y==in.y2 && in.x<in.x2)) return 0;
	return 1;
}
int4 select_ordered(int4 in){
	if(!is_reverse_selection(in)) return in;
	int4 ret=in;
	ret.x=in.x2;
	ret.y=in.y2;
	ret.x2=in.x;
	ret.y2=in.y;
	return ret;
}
int indent_level(string in){
	int ret=0;
	int i=0;
	while(i<in.len && strchr("\t ",in.str[i++])) ret++;
	return ret;
}
editor editor_new(window win){
	return (editor){
		.view={.width=win.width,.height=win.height},
		.selected.x=Fail,
		.undobuff={.datasize=sizeof(edittask)},
		.colors=vec_new_ex(sizeof(var),win.height),
	};
}
string editor_free(editor* e){
	_free(&e->clipboard);
	vec_free_ex(&e->undobuff, task_free);
	vec_free(&e->lines);
	vec_free(&e->colors);
	printf("\033[?25l"); //hide cursor
}
string editor_close(editor* e,int c,string text,int* edited){
	string ret=vec_s(ro(e->lines), "\n");
	editor_free(e);
	if(eq(text,ret)){
		*edited=EditNone;
		_free(&text);
		_free(&ret);
		return Null;
	}
	_free(&text);
	if(c==27)
		*edited=EditCancel;
	else
		*edited=EditSave;
	return ret;
}
