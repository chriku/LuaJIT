#define lib_hdp_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_lib.h"
#include "lj_obj.h"
#include "lj_frame.h"
#include "lj_ff.h"
#include "lj_state.h"
#include "lj_buf.h"
#include "lj_func.h"
#include "lj_bcdump.h"

#define LJLIB_MODULE_hdp

void serialize(lua_State*L,TValue*val,int mt);
void serialize_gc(lua_State*L,GCobj*val,int mt);
size_t serialize_frame(lua_State*L,TValue*frame,TValue*stack,int mt);
size_t serialize_frame(lua_State*L,TValue*frame,TValue*stack,int mt) {
  //printf("TYPE: %d %p\n",frame_type(frame),frame_gc(frame));
  //printf("FLB: %p\n",frame);
  switch(frame_type(frame)) {
    case FRAME_LUA:{
      TValue*prev=frame_prevl(frame);
      size_t pos=0;
      if(prev) {
//printf("LFP: %p\n",frame_func(prev));
        pos=serialize_frame(L,prev,stack,mt)+1;
        const BCIns*ins=frame_pc(frame);
        GCfunc*fn=frame_func(prev);
        GCproto *pt=funcproto(fn);
        //printf("%p %p\n",fn,pt);
        lua_newtable(L);
        lua_pushinteger(L,proto_bcpos(pt,ins));
        lua_setfield(L,-2,"pos");
        serialize_gc(L,obj2gco(fn),mt);
        lua_setfield(L,-2,"func");
        lua_pushinteger(L,(prev-stack)+1);
        lua_setfield(L,-2,"base");
        lua_rawseti(L,-2,pos);
      }
      return pos;
    }break;
    default:
      //printf("FRT: %d\n",(int)frame_type(frame));
      return 0;
  }
}

static int writer_buf(lua_State *L, const void *p, size_t size, void *sb)
{
  lj_buf_putmem((SBuf *)sb, p, (MSize)size);
  UNUSED(L);
  return 0;
}

void serialize_gc(lua_State*L,GCobj*val,int mt) {
  lua_pushlightuserdata(L,val);
  lua_gettable(L,mt);
  if(!lua_isnil(L,-1))
    return;
  lua_pop(L,1);
  lua_newtable(L);
  lua_pushlightuserdata(L,val);
  lua_pushvalue(L,-2);
  lua_settable(L,mt);
  switch(val->gch.gct) {
    case(~LJ_TFUNC): {
      if(isluafunc(&val->fn)) {
        lua_pushliteral(L,"func");
        lua_setfield(L,-2,"type");
        lua_newtable(L);
        GCRef* rs=val->fn.l.uvptr;
        for(size_t i=0;i<val->fn.l.nupvalues;i++) {
          serialize_gc(L,gcref(rs[i]),mt);
          lua_rawseti(L,-2,i+1);
        }
        lua_setfield(L,-2,"upvalues");
        serialize_gc(L,obj2gco(funcproto(&val->fn)),mt);
        lua_setfield(L,-2,"proto");
        //setfuncV(L,L->top,&val->fn);
        //incr_top(L);
        //lua_setfield(L,-2,"func");
      } else if(val->fn.c.ffid==FF_coroutine_yield) {
        lua_pushliteral(L,"func");
        lua_setfield(L,-2,"type");
        lua_pushliteral(L,"yield");
        lua_setfield(L,-2,"special");
      } else {
        lua_pushliteral(L,"func");
        lua_setfield(L,-2,"type");
        lua_pushinteger(L,val->fn.c.ffid);
        lua_setfield(L,-2,"ftype");
      }
    } break;
    case(~LJ_TUPVAL): {
      lua_pushliteral(L,"upvalue");
      lua_setfield(L,-2,"type");
      serialize(L,uvval(&val->uv),mt);
      lua_setfield(L,-2,"data");
    } break;
    case(~LJ_TSTR): {
      lua_pushliteral(L,"string");
      lua_setfield(L,-2,"type");
      lua_pushlstring(L,strdata(val),val->str.len);
      lua_setfield(L,-2,"data");
    } break;
    case(~LJ_TTAB): {
      lua_pushliteral(L,"table");
      lua_setfield(L,-2,"type");
      settabV(L,L->top,&val->tab);
      incr_top(L);
      lua_setfield(L,-2,"data");
    } break;
    case(~LJ_TPROTO): {
      lua_pushliteral(L,"proto");
      lua_setfield(L,-2,"type");
      //serialize(L,uvval(&val->uv),mt);
      //lua_setfield(L,-2,"data");
      SBuf *sb = lj_buf_tmp_(L);  /* Assumes lj_bcwrite() doesn't use tmpbuf. */
      incr_top(L);
      if (lj_bcwrite(L, (GCproto*)val, writer_buf, sb, 0))
        luaL_error(L, "...");
      setstrV(L, L->top-1, lj_buf_str(L, sb));
      lj_gc_check(L);
      lua_setfield(L,-2,"data");
    } break;
    case(~LJ_TTHREAD): {
      lua_pushliteral(L,"thread");
      lua_setfield(L,-2,"type");
      lua_State *l=&val->th;
      lua_newtable(L);
      serialize_frame(L,l->base-1,tvref(l->stack),mt);
      lua_setfield(L,-2,"frames");
      lua_newtable(L);
      for(size_t i=0;i<l->stacksize;i++) {
        TValue* cur=tvref(l->stack)+i;
        if(cur>l->top) break;
        serialize(L,cur,mt);
        if(l->base==cur) { lua_pushboolean(L,1); lua_setfield(L,-2,"base"); };
        lua_pushlstring(L,(const char*)(&cur),8); lua_setfield(L,-2,"addr");
        lua_rawseti(L,-2,i+1);
      }
      lua_setfield(L,-2,"stack");
    } break;
    default:
      printf("Unknown gcV: %d\n",val->gch.gct);
      lua_pushnil(L);
  }
}

void serialize(lua_State*L,TValue*val,int mt) {
  if(tvisgcv(val)) {
    serialize_gc(L,gcval(val),mt);
  }
  else {
    lua_newtable(L);
    lua_pushliteral(L,"simple");
    lua_setfield(L,-2,"type");
    if(tvisnum(val)) {
      lua_pushnumber(L,numberVnum(val));
      lua_setfield(L,-2,"data");
    }
    lua_pushlstring(L,(const char*)val,8);
    lua_setfield(L,-2,"raw");
  }
}

LJLIB_CF(hdp_persist)
{
  TValue* val=lj_lib_checkany(L,1);
  lua_newtable(L);
  int mt=lua_gettop(L);
  serialize(L,val,mt);
  lua_remove(L,mt);
  return 1;
}

typedef struct reader_data {
  const char*data;
  size_t size;
} reader_data;

const char * reader(lua_State*L,void*data,size_t*size) {
  struct reader_data * d=data;
  *size=d->size;
  d->size=0;
  return d->data;
}

#define SAVE   lua_pushvalue(L,-1); lua_pushlstring(L,(const char*)val,sizeof(TValue)); lua_settable(L,mt);


void unserialize(lua_State*L,TValue* val,int mt);
void unserialize(lua_State*L,TValue* val,int mt) {
  lua_pushvalue(L,-1);
  lua_gettable(L,mt);
  if(!lua_isnil(L,-1)) {
    memcpy(val,lua_tostring(L,-1),sizeof(TValue));
    lua_pop(L,1);
    return;
  }
  lua_pop(L,1);
  lua_getfield(L,-1,"type");
  const char* const args[]={"simple","func","proto","upvalue","thread","string","table",NULL};
  int opt=luaL_checkoption(L,-1,NULL,args);
  lua_pop(L,1);
  switch(opt) {
  case 0: {
    lua_getfield(L,-1,"raw");
    memcpy(val,lua_tostring(L,-1),sizeof(TValue));
    lua_pop(L,1);
  };break;
  case 1: {
    lua_getfield(L,-1,"proto");
    TValue tproto;
    unserialize(L,&tproto,mt);
    lua_pop(L,1);
    GCfunc* f=lj_func_newL_empty(L,protoV(&tproto),(GCtab*)gcref(L->env));
    lua_getfield(L,-1,"upvalues");
    for(int i=0;i<f->l.nupvalues;i++) {
      lua_rawgeti(L,-1,i+1);
      TValue uv;
      setgcV(L,&uv,gcref(f->l.uvptr[i]),LJ_TUPVAL);
      unserialize(L,&uv,mt);
      setgcref(f->l.uvptr[i],gcV(&uv));
      lua_pop(L,1);
    }
    lua_pop(L,1);
    setfuncV(L,val,f);
  };break;
  case 2: {
    lua_getfield(L,-1,"data");
    size_t data_l;
    const char* data_s=lua_tolstring(L,-1,&data_l);
    reader_data d;
    d.data=data_s;
    d.size=data_l;
    LexState ls;
    ls.rfunc = reader;
    ls.rdata = &d;
    ls.chunkarg = "?";
    ls.mode = NULL;
    ls.L=L;
    lj_buf_init(L, &ls.sb);
    lj_lex_setup(L,&ls);
    GCproto*proto=lj_bcread(&ls);
    lj_lex_cleanup(L, &ls);
    lua_pop(L,1);
    setprotoV(L,val,proto);
  };break;
  case 3: {
    GCupval* uv=(GCupval*)gcV(val);
    uv->closed=1;
    uv->immutable=0;
    lua_getfield(L,-1,"data");
    unserialize(L,&uv->tv,mt);
    lua_pop(L,1);
  };break;
  case 4: {
    lua_State*l=lj_state_new(L);
    setthreadV(L,val,l);
    SAVE
    lua_getfield(L,-1,"stack");
    int spos=lua_objlen(L,-1);
    lj_state_growstack(l,spos+1);
    for(int i=0;i<spos;i++) {
      lua_rawgeti(L,-1,i+1);
      unserialize(L,tvref(l->stack)+i,mt);
      lua_getfield(L,-1,"base");
      if(lua_toboolean(L,-1))
        l->base=tvref(l->stack)+i;
      lua_pop(L,1);
      lua_pop(L,1);
    }
    l->top=tvref(l->stack)+spos-1;
    l->status = LUA_YIELD;
    lua_pop(L,1);
    lua_getfield(L,-1,"frames");
    int fpos=lua_objlen(L,-1);
    TValue*frm=l->base-1;
    for(int i=fpos-1;i>=0;i--) {
      lua_rawgeti(L,-1,i+1);
      lua_getfield(L,-1,"func");
      TValue func;
      unserialize(L,&func,mt);
      lua_pop(L,1);
      lua_getfield(L,-1,"pos");
      size_t bcpos=lua_tointeger(L,-1);
      lua_pop(L,1);
      lua_getfield(L,-1,"base");
      size_t basepos=lua_tointeger(L,-1);
      lua_pop(L,1);
      GCfunc*lfunc=funcV(&func);
//printf("LFUP: %p %d\n",lfunc,i+1);
      setframe_pc(frm,proto_bc(funcproto(lfunc))+bcpos);
      TValue*prev=frame_prevl(frm);
      setframe_gc(prev,obj2gco(lfunc),0);
      frm=prev;
      lua_pop(L,1);
    }
    setframe_pc(frm,1);
    //setframe_gc(frm,obj2gco(0x42),0);
    lua_pop(L,1);
  };break;
  case 5: {
    lua_getfield(L,-1,"data");
    size_t size;
    const char*data=luaL_checklstring(L,-1,&size);
    GCstr* str=lj_str_new(L,data,size);
    lua_pop(L,1);
    setstrV(L,val,str);
  };break;
  case 6: {
    lua_getfield(L,-1,"data");
    settabV(L,val,tabV(L->top-1));
    lua_pop(L,1);
  };break;
  default:
    luaL_error(L,"?");
  }
  SAVE
}

LJLIB_CF(hdp_unpersist)
{
  luaL_checktype(L,1,LUA_TTABLE);
  lua_newtable(L);
  int mt=lua_gettop(L);
  TValue *ret=L->top;
  incr_top(L);
  lua_pushvalue(L,1);
  unserialize(L,ret,mt);
  lua_pop(L,1);
  lua_remove(L,mt);
  return 1;
}

static const luaL_Reg hdp_lib[] = {
  { "persist",  lj_cf_hdp_persist },
  { "unpersist",  lj_cf_hdp_unpersist },
  { NULL, NULL }
};

#include "lj_libdef.h"

LUALIB_API int luaopen_hdp(lua_State *L)
{
  luaL_register(L, LUA_HDPLIBNAME, hdp_lib);
  return 1;
}

