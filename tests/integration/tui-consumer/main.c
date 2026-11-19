#include <pangu/tui/chat.h>
#include <pangu/tui/editor.h>
#include <string.h>
static void submitted(void *ctx,const char *input,size_t len) {
    pangu_tui_chat *chat=ctx;
    const char expected[]="installed input";
    if (len==sizeof(expected)-1 && !memcmp(input,expected,len))
        pangu_tui_chat_append(chat,"installed response",18);
}
int main(void) {
    pangu_tui_editor *editor=pg_editor_create();
    if (!editor) return 1;
    const char text[]="\xe4\xb8\xad\xe6\x96\x87";
    if (pg_editor_set(editor,text,sizeof(text)-1)!=PANGU_TUI_OK) return 2;
    size_t len=0; const char *view=pg_editor_text(editor,&len);
    if (len!=sizeof(text)-1 || memcmp(view,text,len)) return 3;
    pg_editor_destroy(editor);
    pangu_tui_terminal *terminal=pangu_tui_terminal_create();
    pangu_tui_chat *chat=pangu_tui_chat_create();
    if (!terminal || !chat) return 4;
    pangu_tui_status status=pangu_tui_chat_start(chat,terminal,submitted,chat);
    if (status==PANGU_TUI_OK) status=pangu_tui_chat_run(chat);
    pangu_tui_chat_stop(chat);
    pangu_tui_chat_destroy(chat);
    pangu_tui_terminal_destroy(terminal);
    return status==PANGU_TUI_OK?0:5;
}
