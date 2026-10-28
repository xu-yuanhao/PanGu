#ifndef PANGU_VIEW_WATCH_H
#define PANGU_VIEW_WATCH_H
struct json_object;
typedef struct pg_view_watch pg_view_watch;
typedef struct {void *context;pg_view_watch *head;struct json_object *current;unsigned closed,reserved;int (*read)(void *,struct json_object **);} pg_view_binding;
struct pg_view_watch {pg_view_binding *binding;pg_view_watch *next;struct json_object *previous;unsigned active,initial;};
/* Serialized pull views. Storage starts zeroed/released; provider/context live
   through binding close. First next delivers the attach snapshot; later pulls
   fold intervening changes. 1 owned frame, 0 unchanged, negative error/closed.
   Caller releases frames. Close binding before releasing its provider. */
void pg_view_binding_init(pg_view_binding *,void *,int (*)(void *,struct json_object **));
int pg_view_watch_open(pg_view_binding *,pg_view_watch *);
int pg_view_watch_next(pg_view_watch *,struct json_object **);
void pg_view_watch_close(pg_view_watch *);
void pg_view_binding_close(pg_view_binding *);
#endif
