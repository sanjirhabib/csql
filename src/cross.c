#include "map.h"
#include "cross.h"

/*header
typedef struct s_cross {
	map rows;
	map vals;
} cross;
end*/

cross cross_set(cross in,int nrow,string key,int ncol,string name,string val){
	if(key.len) nrow=keys_idx(in.vals.keys,in.vals.index,key);
	if(name.len) ncol=keys_idx(in.rows.keys,in.rows.index,name);
	if(nrow==-1||ncol==-1||nrow>=in.rows.vals.len||ncol>=in.rows.keys.len) return in;
	_free(in.rows.vals.var+nrow*in.rows.keys.len+ncol);
	in.rows.vals.var[nrow*in.rows.keys.len+ncol]=val;
	in.vals.vals.var[ncol*in.vals.keys.len+nrow]=ro(val);
	return in;
}
vector cross_blankrow(cross in){
	return vec_new_ex(sizeof(var),in.rows.keys.len);
}
map cross_row(cross in,int nrow,string key){
	if(key.len) nrow=keys_idx(in.vals.keys,in.vals.index,key);
	if(nrow==-1||nrow>=in.rows.vals.len) return (map){0};
	map ret=map_ro(in.rows);
	ret.vals=sub(in.rows.vals,nrow*in.rows.keys.len,in.rows.keys.len);
	return ret;
}
map rows_addcol(map in,int ncol,string col,string name){
	if(col.len) ncol=keys_idx(in.keys,in.index,col);
	if(ncol==-1||ncol>in.keys.len) ncol=in.keys.len;
	in.vals=vec_punchholes(in.vals,in.keys.len,ncol);
	in.keys=splice(in.keys,ncol,0,vec_all(ro(name)),NULL);
	_free(&in.index);
	in.index=keys_index(in.keys);
	return in;
}
vector vec_punchholes(vector in,int interval,int offset){
	int recs=in.len/interval;
	int size=in.datasize;
	in=resize(in, recs*(interval+1));
	var* ptr=in.var;
	for(int i=recs-1; i>=0; i--){
		int from=i*interval+offset;
		int len=interval-offset;
		int into=from+i+1;
		if(len) memmove(ptr+into,ptr+from,len*sizeof(var));
		memset(ptr+into-1,0,sizeof(var));
		if(!i) break;
		from=i*interval;
		len=offset;
		into=from+i;
		memmove(ptr+into,ptr+from,len*sizeof(var));
	}
	return in;
}
map rows_addrow(map in){
	in.vals=splice(in.vals,in.vals.len,0,vec_new_ex(sizeof(var),in.keys.len),NULL);
	return in;
}
cross cross_addrow(cross in,int nrow, string key, vector row){
	if(key.len) nrow=keys_idx(in.vals.keys,in.vals.index,key);
	if(nrow==-1||nrow>=in.vals.keys.len) nrow=in.vals.keys.len;
	in.rows.vals=splice(in.rows.vals,in.rows.keys.len*nrow,0,ro(row),NULL);

	in.vals.vals=vec_punchholes(in.vals.vals,in.vals.keys.len,nrow);
	for(int i=0; i<in.rows.keys.len; i++){
		in.vals.vals.var[i*(in.vals.keys.len+1)+nrow]=row.var[i];
	}
	in.vals.keys=sub(in.vals.vals, 0, in.vals.keys.len+1);
	vfree(in.vals.index);
	in.vals.index=keys_index(in.vals.keys);
	vfree(row);
	return in;
}
int cross_colno(cross in, string name){
	return keys_idx(in.rows.keys,in.rows.index,name);
}
int cross_ncols(cross in){
	return in.rows.keys.len;
}
int cross_total(cross in){
	return in.vals.keys.len;
}
cross cross_addcol(cross in,int ncol, string key, string name){
	if(key.len) ncol=keys_idx(in.rows.keys,in.rows.index,key);
	if(ncol==-1||ncol>=in.rows.vals.len) ncol=in.rows.keys.len;
	vector vals=vec_new_ex(sizeof(var),in.rows.vals.len+in.vals.keys.len);
	for(int i=0; i<in.rows.vals.len; i++){
		int idx=i+(i/in.rows.keys.len)+(i%in.rows.keys.len>=ncol ? 1 : 0);
		vals.var[idx]=in.rows.vals.var[i];
	}
	_free(&in.rows.vals);
	_free(&in.rows.index);
	in.rows=(map){
		.keys=splice(in.rows.keys,ncol,0,vec_all(name),NULL),
		.index=keys_index(in.rows.keys),
		.vals=vals,
	};
	in.vals.vals=splice(in.vals.vals,in.vals.keys.len*ncol,0,vec_new_ex(sizeof(var),in.vals.keys.len),NULL);
	return in;
}
vector cross_disownrow(cross in,int nrow,string key){
	if(key.len) nrow=keys_idx(in.vals.keys,in.vals.index,key);
	if(nrow==-1||nrow>=in.rows.vals.len) return NullVec;
	vector ret=_dup(sub(in.rows.vals,nrow*in.rows.keys.len,in.rows.keys.len));
	for(int i=0; i<in.rows.keys.len; i++){
		in.rows.vals.var[nrow*in.rows.keys.len+i].readonly=1;
	}
	return ret;
}
cross cross_resetrow(cross in,int nrow,string key){
	if(key.len) nrow=keys_idx(in.vals.keys,in.vals.index,key);
	if(nrow==-1||nrow>=in.rows.vals.len) return in;
	for(int i=0; i<in.rows.keys.len; i++){
		_free(in.rows.vals.var+nrow*in.rows.keys.len+i);
		in.rows.vals.var[nrow*in.rows.keys.len+i]=NullStr;
		in.vals.vals.var[i*in.vals.keys.len+nrow]=NullStr;
	}
	return in;
}
cross cross_delrow(cross in,int nrow,string key){
	if(key.len) nrow=keys_idx(in.vals.keys,in.vals.index,key);
	if(nrow==-1||nrow>=in.vals.keys.len) return in;
	in.rows.vals=splice(in.rows.vals,nrow*in.rows.keys.len,in.rows.keys.len,Null,_free);
	map_free(&in.vals);
	in.vals=rows_vals(in.rows,0);
	return in;
}
vector cross_col(cross in,int ncol,string name){
	if(name.len) ncol=keys_idx(in.rows.keys,in.rows.index,name);
	if(ncol==-1||ncol>=in.rows.keys.len) return Null;
	return sub(in.vals.vals,ncol*in.vals.keys.len,in.vals.keys.len);
}
cross cross_reinit(cross in,map rows){
	cross_free(&in);
	return cross_new(rows);	
}
cross cross_new(map rows){
	return (cross){
		.rows=rows,
		.vals=rows_vals(rows,0),
	};
}
void cross_free(cross* in){
	map_free(&in->rows);
	map_free(&in->vals);
}
string cross_get(cross in,int nrow,string key,int ncol,string name){
	if(key.len) nrow=keys_idx(in.vals.keys,in.vals.index,key);
	if(name.len) ncol=keys_idx(in.rows.keys,in.rows.index,name);
	if(nrow==-1||ncol==-1||nrow>=in.rows.vals.len||ncol>=in.rows.keys.len) return NullStr;
	return get(in.rows.vals,nrow*in.rows.keys.len+ncol);
}
map rows_vals(map rows, int indexcol){
	if(!rows.keys.len) return (map){0};
	vector vals=vec_new_ex(sizeof(var), rows.vals.len);
	int total=rows.vals.len/rows.keys.len;
	for(int i=0; i<rows.vals.len; i++){
		vals.var[i]=ro(rows.vals.var[(i*rows.keys.len)%rows.vals.len+i/total]);
	}
	return (map){
		.keys=sub(vals, indexcol*total,total),
		.vals=vals,
		.index=keys_index(sub(vals, indexcol*total,total))
	};
}
