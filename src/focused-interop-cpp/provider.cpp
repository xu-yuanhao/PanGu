#include "observer.h"
static void (*observer)(void *);
extern "C" void pangu_route_cpp_set_observer(void (*fn)(void *)) noexcept {observer=fn;}
#include "src/t14-abi/interop-contract-v1.h"
/* Read object representations, rather than aliasing C-owned structs as C++
 * objects. No standard library, allocation, global constructors or exceptions.
 */
template<class T> static T load(const void *source) noexcept {
 T value{};auto *out=reinterpret_cast<unsigned char *>(&value);auto *in=static_cast<const unsigned char *>(source);
 for(unsigned long i=0;i<sizeof(T);++i) {out[i]=in[i];}
 return value;
}
template<class T> static void store(void *target,T value) noexcept {
 auto *out=static_cast<unsigned char *>(target);auto *in=reinterpret_cast<const unsigned char *>(&value);
 for(unsigned long i=0;i<sizeof(T);++i) out[i]=in[i];
}
extern "C" pangu_interop_i32 pangu_route_cpp_invoke(const void *table,void *state,pangu_interop_u32 version,pangu_interop_i32 input,pangu_interop_i32 *out) noexcept {
 if(!table || !out || (reinterpret_cast<unsigned long long>(table)&7)) return 1;
 auto *bytes=static_cast<const unsigned char *>(table);
 if(!version || load<pangu_interop_u32>(bytes)!=version || load<pangu_interop_u32>(bytes+4)<PANGU_INTEROP_SERVICE_SIZE || load<pangu_interop_u32>(bytes+8)!=1 || load<pangu_interop_u32>(bytes+12)) return 9;
 auto method=load<pangu_interop_method>(bytes+PANGU_INTEROP_METHOD_OFFSET);if(!method) return 9;
 return method(state,input,out);
}
extern "C" pangu_interop_i32 pangu_route_cpp_evaluate(void *state,pangu_interop_i32 input,pangu_interop_i32 *out) noexcept {
 if(!state || !out || !observer) {return 1;}
 auto *bytes=static_cast<unsigned char *>(state);
 auto value=load<pangu_interop_i32>(bytes);auto calls=load<pangu_interop_i32>(bytes+4);
 long long result=static_cast<long long>(value)+input;if(result<-2147483648LL || result>2147483647LL || calls==2147483647) return 11;
 store(out,static_cast<pangu_interop_i32>(result));store(bytes+4,calls+1);observer(state);return 0;
}
