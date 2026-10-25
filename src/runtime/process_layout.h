#ifndef PG_PROCESS_LAYOUT_H
#define PG_PROCESS_LAYOUT_H
#define PG_P_STATE 0
#define PG_P_PID 4
#define PG_P_OUTPUT 8
#define PG_P_EOF 16
#define PG_P_NEXT 20
#define PG_P_EXITED 24
#define PG_P_EXIT_CODE 28
#define PG_P_SIGNAL 32
#define PG_P_REASON 36
#define PG_P_ERROR 40
#define PG_P_KILLED 44
#define PG_P_DEADLINE 48
#define PG_P_IDLE 56
#define PG_P_BUFFER 64
#define PG_P_BYTES 4160
#define PG_P_WAITID_FLAGS 0x01000005
#define PG_P_SIGINFO_BYTES 128
#define PG_P_SI_CODE 8
#define PG_P_SI_PID 16
#define PG_P_SI_STATUS 24
#define PG_P_SA_BYTES 152
#define PG_P_SA_FLAGS 136
#define PG_P_SPAWN_ACTION_BYTES 80
#define PG_P_SPAWN_ATTR_BYTES 336
#define PG_P_SIGSET_BYTES 128
#ifndef __ASSEMBLER__
#include "process.h"
#include <stddef.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>
#define PG_PO(field,offset) _Static_assert(offsetof(pg_process,field)==(offset),"process." #field)
PG_PO(state,PG_P_STATE);PG_PO(pid,PG_P_PID);PG_PO(output,PG_P_OUTPUT);
PG_PO(eof,PG_P_EOF);PG_PO(next_stream,PG_P_NEXT);PG_PO(exited,PG_P_EXITED);
PG_PO(exit_code,PG_P_EXIT_CODE);PG_PO(signal,PG_P_SIGNAL);PG_PO(reason,PG_P_REASON);
PG_PO(error,PG_P_ERROR);PG_PO(killed,PG_P_KILLED);PG_PO(deadline,PG_P_DEADLINE);
PG_PO(idle_deadline,PG_P_IDLE);PG_PO(buffer,PG_P_BUFFER);
_Static_assert(sizeof(pg_process)==PG_P_BYTES,"process bytes");
_Static_assert(sizeof(pg_process_ops)==40&&offsetof(pg_process_ops,start)==8&&offsetof(pg_process_ops,next)==16&&offsetof(pg_process_ops,cancel)==24&&offsetof(pg_process_ops,close)==32,"process service ABI");
_Static_assert(sizeof(pg_process_request)==32&&offsetof(pg_process_request,shell)==8&&offsetof(pg_process_request,environment)==16&&offsetof(pg_process_request,timeout_ms)==24,"request ABI");
_Static_assert(sizeof(pg_process_event)==40&&offsetof(pg_process_event,bytes)==8&&offsetof(pg_process_event,exit_code)==24&&offsetof(pg_process_event,signal)==28&&offsetof(pg_process_event,reason)==32&&offsetof(pg_process_event,error)==36,"event ABI");
_Static_assert(sizeof(posix_spawn_file_actions_t)==PG_P_SPAWN_ACTION_BYTES&&sizeof(posix_spawnattr_t)==PG_P_SPAWN_ATTR_BYTES&&sizeof(sigset_t)==PG_P_SIGSET_BYTES,"spawn storage ABI");
_Static_assert(_Alignof(posix_spawn_file_actions_t)<=16&&_Alignof(posix_spawnattr_t)<=16&&_Alignof(sigset_t)<=16&&_Alignof(siginfo_t)<=16,"spawn storage alignment");
_Static_assert(sizeof(struct sigaction)==PG_P_SA_BYTES&&offsetof(struct sigaction,sa_handler)==0&&offsetof(struct sigaction,sa_flags)==PG_P_SA_FLAGS,"sigaction ABI");
_Static_assert(sizeof(siginfo_t)==PG_P_SIGINFO_BYTES&&offsetof(siginfo_t,si_code)==PG_P_SI_CODE&&offsetof(siginfo_t,si_pid)==PG_P_SI_PID&&offsetof(siginfo_t,si_status)==PG_P_SI_STATUS,"waitid ABI");
_Static_assert((WEXITED|WNOHANG|WNOWAIT)==PG_P_WAITID_FLAGS&&P_PID==1&&WNOHANG==1,"wait flags");
_Static_assert(CLD_EXITED==1&&CLD_KILLED==2&&CLD_DUMPED==3,"exit codes");
_Static_assert((POSIX_SPAWN_SETPGROUP|POSIX_SPAWN_SETSIGDEF|POSIX_SPAWN_SETSIGMASK)==14,"spawn flags");
_Static_assert(SIGCHLD==17&&SIGKILL==9&&SA_NOCLDWAIT==2,"signals");
_Static_assert(O_CLOEXEC==524288&&O_NONBLOCK==2048&&F_DUPFD_CLOEXEC==1030&&F_GETFL==3&&F_SETFL==4,"fd flags");
_Static_assert(CLOCK_MONOTONIC==1&&sizeof(struct timespec)==16&&offsetof(struct timespec,tv_nsec)==8,"clock ABI");
#endif
#endif
