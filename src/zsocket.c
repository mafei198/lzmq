#include "zsocket.h"
#include "zmsg.h"
#include "lzutils.h"
#include "lzmq.h"
#include <stdint.h>

static int luazmq_skt_bind (lua_State *L) {
  zsocket *skt = luazmq_getsocket(L);
  const char *addr = luaL_checkstring(L, 2);
  int ret = zmq_bind(skt->skt, addr);
  if(-1 == ret) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_unbind (lua_State *L) {
  zsocket *skt = luazmq_getsocket(L);
  const char *addr = luaL_checkstring(L, 2);
  int ret = zmq_unbind(skt->skt, addr);
  if(-1 == ret) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_connect (lua_State *L) {
  zsocket *skt = luazmq_getsocket(L);
  const char *addr = luaL_checkstring(L, 2);
  int ret = zmq_connect(skt->skt, addr);
  if(-1 == ret) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_disconnect (lua_State *L) {
  zsocket *skt = luazmq_getsocket(L);
  const char *addr = luaL_checkstring(L, 2);
  int ret = zmq_disconnect(skt->skt, addr);
  if(-1 == ret) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_send (lua_State *L) {
  zsocket *skt = luazmq_getsocket(L);
  zmq_msg_t msg;
  size_t len;
  const char *data = luaL_checklstring(L, 2, &len);
  int flags = luaL_optint(L,3,0);
  int ret = zmq_msg_init_size(&msg, len);
  if(-1 == ret) return luazmq_fail(L, skt);
  memcpy(zmq_msg_data(&msg), data, len);
  ret = zmq_msg_send(&msg, skt->skt, flags);
  zmq_msg_close(&msg);
  if(-1 == ret) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_send_msg (lua_State *L) {
  zsocket *skt  = luazmq_getsocket(L);
  zmessage *msg = luazmq_getmessage_at(L,2);
  int flags = luaL_optint(L,3,0);
  int ret = zmq_msg_send(&msg->msg, skt->skt, flags);
  if(-1 == ret) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_send_more(lua_State *L) {
  int flags = luaL_optint(L, 3, 0);
  flags |= ZMQ_SNDMORE;
  lua_settop(L, 2);
  lua_pushinteger(L, flags);
  return luazmq_skt_send(L);
}

static int luazmq_skt_recv (lua_State *L) {
  zsocket *skt = luazmq_getsocket(L);
  zmq_msg_t msg;
  int flags = luaL_optint(L,2,0);
  int ret = zmq_msg_init(&msg);
  if(-1 == ret) return luazmq_fail(L, skt);
  ret = zmq_msg_recv(&msg, skt->skt, flags);
  if(-1 == ret){
    zmq_msg_close(&msg);
    return luazmq_fail(L, skt);
  }
  lua_pushlstring(L, zmq_msg_data(&msg), zmq_msg_size(&msg));
  if( zmq_msg_more(&msg) ){
    skt->flags |= LUAZMQ_FLAG_MORE;
    lua_pushboolean(L, 1);
  }
  else{
    skt->flags &= ~LUAZMQ_FLAG_MORE;
    lua_pushboolean(L, 0);
  }

  zmq_msg_close(&msg);
  return 2;
}

static int luazmq_skt_recv_msg (lua_State *L) {
  zsocket *skt  = luazmq_getsocket(L);
  zmessage *msg = luazmq_getmessage_at(L,2);
  int flags     = luaL_optint(L,3,0);
  int ret = zmq_msg_recv(&msg->msg, skt->skt, flags);

  if(-1 == ret) return luazmq_fail(L, skt);

  lua_settop(L, 2); // remove flags
  if( zmq_msg_more(&msg->msg) ){
    skt->flags |= LUAZMQ_FLAG_MORE;
    lua_pushboolean(L, 1);
  }
  else{
    skt->flags &= ~LUAZMQ_FLAG_MORE;
    lua_pushboolean(L, 0);
  }
  return 2;
}

static int luazmq_skt_recv_new_msg (lua_State *L){
  if(lua_isuserdata(L,2)) return luazmq_skt_recv_msg(L);
  luaL_optint(L, 2, 0);
  {
    int n = luazmq_msg_init(L);
    if(n != 1)return n;
    lua_insert(L, 2);
    n = luazmq_skt_recv_msg(L);
    if(lua_isnil(L, -n)){
      zmessage *msg = luazmq_getmessage_at(L, 2);
      zmq_msg_close(&msg->msg);
      msg->flags |= LUAZMQ_FLAG_CLOSED;
    }
    return n;
  }
}

static int luazmq_skt_more (lua_State *L) {
  zsocket *skt = luazmq_getsocket(L);
  lua_pushboolean(L, skt->flags & LUAZMQ_FLAG_MORE);
  return 1;
}

static int luazmq_skt_send_all (lua_State *L) {
  zsocket *skt = luazmq_getsocket(L);
  size_t i = luaL_optint(L,3,1);
  size_t n = lua_objlen(L, 2);
  for(;i <= n; ++i){
    zmq_msg_t msg;
    const char *data;size_t len;
    int ret;
    lua_rawgeti(L, 2, i);
    data = lua_tolstring(L,-1, &len);
    ret = zmq_msg_init_size(&msg, len);
    if(-1 == ret){
      ret = luazmq_fail(L, skt);
      lua_pushinteger(L, i);
      return ret + 1;
    }
    memcpy(zmq_msg_data(&msg), data, len);
    ret = zmq_msg_send(&msg, skt->skt, (i == n)?0:ZMQ_SNDMORE);
    zmq_msg_close(&msg);
    if(-1 == ret){
      ret = luazmq_fail(L, skt);
      lua_pushinteger(L, i);
      return ret + 1;
    }
  }
  return luazmq_pass(L);
}

static int luazmq_skt_recv_all (lua_State *L) {
  zsocket *skt = luazmq_getsocket(L);
  zmq_msg_t msg;
  int flags = luaL_optint(L,2,0);
  int i = 0;
  int result_index = lua_gettop(L) + 1;
  lua_newtable(L);
  while(1){
    int ret = zmq_msg_init(&msg);
    if(-1 == ret){
      ret = luazmq_fail(L, skt);
      lua_pushvalue(L,result_index);
      return ret + 1;
    }
      
    ret = zmq_msg_recv(&msg, skt->skt, flags);
    if(-1 == ret){
      ret = luazmq_fail(L, skt);
      zmq_msg_close(&msg);
      lua_pushvalue(L,result_index);
      return ret + 1;
    }

    lua_pushlstring(L, zmq_msg_data(&msg), zmq_msg_size(&msg));
    lua_rawseti(L, result_index, ++i);
    ret = zmq_msg_more(&msg);
    zmq_msg_close(&msg);
    if(!ret) break;
  }
  return 1;
}

static int luazmq_skt_destroy (lua_State *L) {
  zsocket *skt = (zsocket *)luazmq_checkudatap (L, 1, LUAZMQ_SOCKET);
  luaL_argcheck (L, skt != NULL, 1, LUAZMQ_PREFIX"socket expected");
  if(!(skt->flags & LUAZMQ_FLAG_CLOSED)){
    int ret = zmq_close(skt->skt);
    if(ret == -1)return luazmq_fail(L, skt);
    skt->flags |= LUAZMQ_FLAG_CLOSED;
  }
  return luazmq_pass(L);
}

static int luazmq_skt_closed (lua_State *L) {
  zsocket *skt = (zsocket *)luazmq_checkudatap (L, 1, LUAZMQ_SOCKET);
  luaL_argcheck (L, skt != NULL, 1, LUAZMQ_PREFIX"socket expected");
  lua_pushboolean(L, skt->flags & LUAZMQ_FLAG_CLOSED);
  return 1;
}

static int luazmq_skt_set_int (lua_State *L, int option_name) {
  zsocket *skt = luazmq_getsocket(L);
  int option_value = luaL_checkint(L, 2);
  int ret = zmq_setsockopt(skt->skt, option_name, &option_value, sizeof(option_value));
  if (ret == -1) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_set_u64 (lua_State *L, int option_name) {
  zsocket *skt = luazmq_getsocket(L);
  uint64_t option_value = (uint64_t)luaL_checknumber(L, 2);
  int ret = zmq_setsockopt(skt->skt, option_name, &option_value, sizeof(option_value));
  if (ret == -1) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_set_i64 (lua_State *L, int option_name) {
  zsocket *skt = luazmq_getsocket(L);
  int64_t option_value = (int64_t)luaL_checknumber(L, 2);
  int ret = zmq_setsockopt(skt->skt, option_name, &option_value, sizeof(option_value));
  if (ret == -1) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_set_str (lua_State *L, int option_name) {
  zsocket *skt = luazmq_getsocket(L);
  size_t len;
  const char *option_value = luaL_checklstring(L, 2, &len);
  int ret = zmq_setsockopt(skt->skt, option_name, option_value, len);
  if (ret == -1) return luazmq_fail(L, skt);
  return luazmq_pass(L);
}

static int luazmq_skt_get_int (lua_State *L, int option_name) {
  zsocket *skt = luazmq_getsocket(L);
  int option_value; size_t len = sizeof(option_value);
  int ret = zmq_getsockopt(skt->skt, option_name, &option_value, &len);
  if (ret == -1) return luazmq_fail(L, skt);
  lua_pushinteger(L, option_value);
  return 1;
}

static int luazmq_skt_get_u64 (lua_State *L, int option_name) {
  zsocket *skt = luazmq_getsocket(L);
  uint64_t option_value;  size_t len = sizeof(option_value);
  int ret = zmq_getsockopt(skt->skt, option_name, &option_value, &len);
  if (ret == -1) return luazmq_fail(L, skt);
  lua_pushnumber(L, (lua_Number)option_value);
  return 1;
}

static int luazmq_skt_get_i64 (lua_State *L, int option_name) {
  zsocket *skt = luazmq_getsocket(L);
  int64_t option_value;  size_t len = sizeof(option_value);
  int ret = zmq_getsockopt(skt->skt, option_name, &option_value, &len);
  if (ret == -1) return luazmq_fail(L, skt);
  lua_pushnumber(L, (lua_Number)option_value);
  return 1;
}

static int luazmq_skt_get_str (lua_State *L, int option_name) {
  zsocket *skt = luazmq_getsocket(L);
  char option_value[255]; size_t len = sizeof(option_value);
  int ret = zmq_getsockopt(skt->skt, option_name, option_value, &len);
  if (ret == -1) return luazmq_fail(L, skt);
  lua_pushlstring(L, option_value, len);
  return 1;
}

#define DEFINE_SKT_OPT(NAME, OPTNAME, TYPE) \
  static int luazmq_skt_set_##NAME(lua_State *L){return luazmq_skt_set_##TYPE(L, OPTNAME);}\
  static int luazmq_skt_get_##NAME(lua_State *L){return luazmq_skt_get_##TYPE(L, OPTNAME);}

#define REGISTER_SKT_OPT(NAME) {"set_"#NAME, luazmq_skt_set_##NAME}, {"get_"#NAME, luazmq_skt_get_##NAME}

DEFINE_SKT_OPT(affinity,           ZMQ_AFFINITY,            u64  )
DEFINE_SKT_OPT(identity,           ZMQ_IDENTITY,            str  )
DEFINE_SKT_OPT(subscribe,          ZMQ_SUBSCRIBE,           str  )
DEFINE_SKT_OPT(unsubscribe,        ZMQ_UNSUBSCRIBE,         str  )
DEFINE_SKT_OPT(rate,               ZMQ_RATE,                int  )
DEFINE_SKT_OPT(recovery_ivl,       ZMQ_RECOVERY_IVL,        int  )
DEFINE_SKT_OPT(sndbuf,             ZMQ_SNDBUF,              int  )
DEFINE_SKT_OPT(rcvbuf,             ZMQ_RCVBUF,              int  )
DEFINE_SKT_OPT(rcvmore,            ZMQ_RCVMORE,             int  )
DEFINE_SKT_OPT(fd,                 ZMQ_FD,                  int  )
DEFINE_SKT_OPT(events,             ZMQ_EVENTS,              int  )
DEFINE_SKT_OPT(type,               ZMQ_TYPE,                int  )
DEFINE_SKT_OPT(linger,             ZMQ_LINGER,              int  )
DEFINE_SKT_OPT(reconnect_ivl,      ZMQ_RECONNECT_IVL,       int  )
DEFINE_SKT_OPT(backlog,            ZMQ_BACKLOG,             int  )
DEFINE_SKT_OPT(reconnect_ivl_max,  ZMQ_RECONNECT_IVL_MAX,   int  )
DEFINE_SKT_OPT(maxmsgsize,         ZMQ_MAXMSGSIZE,          i64  )
DEFINE_SKT_OPT(sndhwm,             ZMQ_SNDHWM,              int  )
DEFINE_SKT_OPT(rcvhwm,             ZMQ_RCVHWM,              int  )
DEFINE_SKT_OPT(multicast_hops,     ZMQ_MULTICAST_HOPS,      int  )
DEFINE_SKT_OPT(rcvtimeo,           ZMQ_RCVTIMEO,            int  )
DEFINE_SKT_OPT(sndtimeo,           ZMQ_SNDTIMEO,            int  )
DEFINE_SKT_OPT(ipv4only,           ZMQ_IPV4ONLY,            int  )
DEFINE_SKT_OPT(last_endpoint,      ZMQ_LAST_ENDPOINT,       str  )
#ifdef ZMQ_ROUTER_BEHAVIOR 
DEFINE_SKT_OPT(fail_unroutable,    ZMQ_ROUTER_BEHAVIOR,     int  )
DEFINE_SKT_OPT(router_behavior,    ZMQ_ROUTER_BEHAVIOR,     int  )
#else
DEFINE_SKT_OPT(fail_unroutable,    ZMQ_FAIL_UNROUTABLE,     int  )
DEFINE_SKT_OPT(router_behavior,    ZMQ_FAIL_UNROUTABLE,     int  )
#endif
DEFINE_SKT_OPT(tcp_keepalive,      ZMQ_TCP_KEEPALIVE,       int  )
DEFINE_SKT_OPT(tcp_keepalive_cnt,  ZMQ_TCP_KEEPALIVE_CNT,   int  )
DEFINE_SKT_OPT(tcp_keepalive_idle, ZMQ_TCP_KEEPALIVE_IDLE,  int  )
DEFINE_SKT_OPT(tcp_keepalive_intvl,ZMQ_TCP_KEEPALIVE_INTVL, int  )
DEFINE_SKT_OPT(tcp_accept_filter,  ZMQ_TCP_ACCEPT_FILTER,   str  )

static const struct luaL_Reg luazmq_skt_methods[] = {
  {"bind",         luazmq_skt_bind         },
  {"unbind",       luazmq_skt_unbind       },
  {"connect",      luazmq_skt_connect      },
  {"disconnect",   luazmq_skt_disconnect   },
  {"send",         luazmq_skt_send         },
  {"send_msg",     luazmq_skt_send_msg     },
  {"send_more",    luazmq_skt_send_more    },
  {"recv",         luazmq_skt_recv         },
  {"recv_msg",     luazmq_skt_recv_msg     },
  {"recv_new_msg", luazmq_skt_recv_new_msg },
  {"send_all",     luazmq_skt_send_all     },
  {"recv_all",     luazmq_skt_recv_all     },
  {"more",         luazmq_skt_more         },
  {"get_rcvmore",  luazmq_skt_more         },

  REGISTER_SKT_OPT(  affinity              ),
  REGISTER_SKT_OPT(  identity              ),
  REGISTER_SKT_OPT(  subscribe             ),
  REGISTER_SKT_OPT(  unsubscribe           ),
  REGISTER_SKT_OPT(  rate                  ),
  REGISTER_SKT_OPT(  recovery_ivl          ),
  REGISTER_SKT_OPT(  sndbuf                ),
  REGISTER_SKT_OPT(  rcvbuf                ),
  REGISTER_SKT_OPT(  rcvmore               ),
  REGISTER_SKT_OPT(  fd                    ),
  REGISTER_SKT_OPT(  events                ),
  REGISTER_SKT_OPT(  type                  ),
  REGISTER_SKT_OPT(  linger                ),
  REGISTER_SKT_OPT(  reconnect_ivl         ),
  REGISTER_SKT_OPT(  backlog               ),
  REGISTER_SKT_OPT(  reconnect_ivl_max     ),
  REGISTER_SKT_OPT(  maxmsgsize            ),
  REGISTER_SKT_OPT(  sndhwm                ),
  REGISTER_SKT_OPT(  rcvhwm                ),
  REGISTER_SKT_OPT(  multicast_hops        ),
  REGISTER_SKT_OPT(  rcvtimeo              ),
  REGISTER_SKT_OPT(  sndtimeo              ),
  REGISTER_SKT_OPT(  ipv4only              ),
  REGISTER_SKT_OPT(  last_endpoint         ),
  REGISTER_SKT_OPT(  fail_unroutable       ),
  REGISTER_SKT_OPT(  router_behavior       ),
  REGISTER_SKT_OPT(  tcp_keepalive         ),
  REGISTER_SKT_OPT(  tcp_keepalive_cnt     ),
  REGISTER_SKT_OPT(  tcp_keepalive_idle    ),
  REGISTER_SKT_OPT(  tcp_keepalive_intvl   ),
  REGISTER_SKT_OPT(  tcp_accept_filter     ),

  {"__gc",       luazmq_skt_destroy        },
  {"close",      luazmq_skt_destroy        },
  {"closed",     luazmq_skt_closed         },
  {NULL,NULL}
};

static const luazmq_int_const skt_types[] ={
  DEFINE_ZMQ_CONST(  PAIR   ),
  DEFINE_ZMQ_CONST(  PUB    ),
  DEFINE_ZMQ_CONST(  SUB    ),
  DEFINE_ZMQ_CONST(  REQ    ),
  DEFINE_ZMQ_CONST(  REP    ),
  DEFINE_ZMQ_CONST(  DEALER ),
  DEFINE_ZMQ_CONST(  ROUTER ),
  DEFINE_ZMQ_CONST(  PULL   ),
  DEFINE_ZMQ_CONST(  PUSH   ),
  DEFINE_ZMQ_CONST(  XPUB   ),
  DEFINE_ZMQ_CONST(  XSUB   ),
  DEFINE_ZMQ_CONST(  XREQ   ),
  DEFINE_ZMQ_CONST(  XREP   ),

  {NULL, 0}
};

static const luazmq_int_const skt_options[] ={
  // DEFINE_ZMQ_CONST(  AFFINITY            ),
  // DEFINE_ZMQ_CONST(  IDENTITY            ),
  // DEFINE_ZMQ_CONST(  SUBSCRIBE           ),
  // DEFINE_ZMQ_CONST(  UNSUBSCRIBE         ),
  // DEFINE_ZMQ_CONST(  RATE                ),
  // DEFINE_ZMQ_CONST(  RECOVERY_IVL        ),
  // DEFINE_ZMQ_CONST(  SNDBUF              ),
  // DEFINE_ZMQ_CONST(  RCVBUF              ),
  // DEFINE_ZMQ_CONST(  RCVMORE             ),
  // DEFINE_ZMQ_CONST(  FD                  ),
  // DEFINE_ZMQ_CONST(  EVENTS              ),
  // DEFINE_ZMQ_CONST(  TYPE                ),
  // DEFINE_ZMQ_CONST(  LINGER              ),
  // DEFINE_ZMQ_CONST(  RECONNECT_IVL       ),
  // DEFINE_ZMQ_CONST(  BACKLOG             ),
  // DEFINE_ZMQ_CONST(  RECONNECT_IVL_MAX   ),
  // DEFINE_ZMQ_CONST(  MAXMSGSIZE          ),
  // DEFINE_ZMQ_CONST(  SNDHWM              ),
  // DEFINE_ZMQ_CONST(  RCVHWM              ),
  // DEFINE_ZMQ_CONST(  MULTICAST_HOPS      ),
  // DEFINE_ZMQ_CONST(  RCVTIMEO            ),
  // DEFINE_ZMQ_CONST(  SNDTIMEO            ),
  // DEFINE_ZMQ_CONST(  IPV4ONLY            ),
  // DEFINE_ZMQ_CONST(  LAST_ENDPOINT       ),
  // DEFINE_ZMQ_CONST(  FAIL_UNROUTABLE     ),
  // DEFINE_ZMQ_CONST(  TCP_KEEPALIVE       ),
  // DEFINE_ZMQ_CONST(  TCP_KEEPALIVE_CNT   ),
  // DEFINE_ZMQ_CONST(  TCP_KEEPALIVE_IDLE  ),
  // DEFINE_ZMQ_CONST(  TCP_KEEPALIVE_INTVL ),
  // DEFINE_ZMQ_CONST(  TCP_ACCEPT_FILTER   ),

  {NULL, 0}
};

static const luazmq_int_const skt_flags[] ={
  DEFINE_ZMQ_CONST(  SNDMORE             ),

  {NULL, 0}
};

void luazmq_socket_initlib (lua_State *L){
  luazmq_createmeta(L, LUAZMQ_SOCKET, luazmq_skt_methods);
  lua_pop(L, 1);
  luazmq_register_consts(L, skt_types);
  luazmq_register_consts(L, skt_options);
  luazmq_register_consts(L, skt_flags);
}