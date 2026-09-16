#include <adwaita.h>
#include <json-glib/json-glib.h>
#include "ewaf.h"
#include <string.h>

/* Native widgets, GSettings and GLib scheduling only. Rust owns all domain rules. */
typedef struct {
    GtkWindow *window;
    GtkApplication *application;
    GtkWidget *start, *end, *weekday, *search, *list, *count, *status, *destination_label;
    GtkWidget *form, *create, *choose, *cancel, *open, *progress;
    GSettings *settings;
    JsonObject *plan;
    char *destination;
    guint generation;
    guint64 handle;
    gint64 total;
    gboolean busy, cancelled, closed, confirmation, choosing;
} Workspace;
typedef struct { JsonObject *request; guint generation; } Work;
static const guint days[] = {2,3,4,5,6,7,1};
static void refresh(Workspace *w);
static void create_requested(Workspace *w);
static void create_begin(Workspace *w);
static void choose_folder(Workspace *w, gboolean create_after);
static void create_step(Workspace *w);

static JsonObject *request_new(const char *op) {
    JsonObject *o=json_object_new(); json_object_set_string_member(o,"op",op); return o;
}
static JsonObject *core_call(JsonObject *request, GError **error) {
    g_autoptr(JsonNode) node=json_node_new(JSON_NODE_OBJECT); json_node_set_object(node,request);
    g_autofree char *input=json_to_string(node,FALSE);
    char *output=ewaf_request((const uint8_t *)input,strlen(input));
    g_autoptr(JsonParser) parser=json_parser_new();
    gboolean parsed=json_parser_load_from_data(parser,output,-1,error); ewaf_string_free(output);
    if(!parsed) return NULL;
    JsonObject *envelope=json_node_get_object(json_parser_get_root(parser));
    if(!json_object_get_boolean_member(envelope,"ok")) {
        JsonObject *failure=json_object_get_object_member(envelope,"error");
        g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,json_object_get_string_member(failure,"message")); return NULL;
    }
    return json_object_ref(json_object_get_object_member(envelope,"value"));
}
static void work_free(gpointer data) { Work *work=data; json_object_unref(work->request); g_free(work); }
static void worker(GTask *task,gpointer source,gpointer data,GCancellable *cancellable) {
    (void)source; (void)cancellable;
    Work *work=data; GError *error=NULL;
    JsonObject *value=core_call(work->request,&error);
    if(value) g_task_return_pointer(task,value,(GDestroyNotify)json_object_unref);
    else g_task_return_error(task,error);
}
static void core_async(Workspace *w,JsonObject *request,GAsyncReadyCallback callback) {
    Work *work=g_new0(Work,1); work->request=request; work->generation=w->generation;
    GTask *task=g_task_new(w->window,NULL,callback,NULL);
    g_task_set_task_data(task,work,work_free); g_task_run_in_thread(task,worker); g_object_unref(task);
}
static Workspace *workspace(GObject *window) { return g_object_get_data(window,"workspace"); }
static void status(Workspace *w,const char *text) { if(!w->closed) gtk_label_set_text(GTK_LABEL(w->status),text); }
static void message(Workspace *w,const char *title,const char *text) {
    if(w->closed) return;
    GtkWidget *dialog=adw_message_dialog_new(w->window,title,text);
    adw_message_dialog_add_response(ADW_MESSAGE_DIALOG(dialog),"ok","OK");
    adw_message_dialog_set_close_response(ADW_MESSAGE_DIALOG(dialog),"ok"); gtk_window_present(GTK_WINDOW(dialog));
}
static void copy_name(GtkButton *button,gpointer data) {
    (void)data; const char *name=g_object_get_data(G_OBJECT(button),"name");
    gdk_clipboard_set_text(gtk_widget_get_clipboard(GTK_WIDGET(button)),name);
}
static GdkContentProvider *drag_name(GtkDragSource *source,double x,double y,gpointer data) {
    (void)source;(void)x;(void)y; return gdk_content_provider_new_typed(G_TYPE_STRING,(const char *)data);
}
static void plan_ready(GObject *source,GAsyncResult *result,gpointer data) {
    (void)data; Workspace *w=workspace(source); Work *work=g_task_get_task_data(G_TASK(result));
    g_autoptr(GError) error=NULL; JsonObject *value=g_task_propagate_pointer(G_TASK(result),&error);
    if(w->closed || work->generation!=w->generation) { if(value) json_object_unref(value); return; }
    if(!value) { status(w,error->message); return; }
    w->total=json_object_get_int_member(value,"count"); w->confirmation=json_object_get_boolean_member(value,"requiresConfirmation");
    g_autofree char *count=g_strdup_printf("%" G_GINT64_FORMAT " folders",w->total); gtk_label_set_text(GTK_LABEL(w->count),count);
    JsonArray *dates=json_object_get_array_member(value,"dates");
    for(guint i=0;i<json_array_get_length(dates);i++) {
        const char *name=json_array_get_string_element(dates,i);
        GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8), *label=gtk_label_new(name), *copy=gtk_button_new_from_icon_name("edit-copy-symbolic");
        gtk_widget_set_hexpand(label,TRUE); gtk_label_set_xalign(GTK_LABEL(label),0);
        gtk_widget_set_margin_start(row,12); gtk_widget_set_margin_end(row,12); gtk_widget_set_margin_top(row,6); gtk_widget_set_margin_bottom(row,6);
        gtk_widget_set_tooltip_text(copy,"Copy Folder Name"); gtk_accessible_update_property(GTK_ACCESSIBLE(copy),GTK_ACCESSIBLE_PROPERTY_LABEL,"Copy Folder Name",-1);
        g_object_set_data_full(G_OBJECT(copy),"name",g_strdup(name),g_free); g_signal_connect(copy,"clicked",G_CALLBACK(copy_name),NULL);
        GtkDragSource *drag=gtk_drag_source_new(); gtk_drag_source_set_actions(drag,GDK_ACTION_COPY);
        char *drag_text=g_strdup(name); g_object_set_data_full(G_OBJECT(drag),"text",drag_text,g_free);
        g_signal_connect(drag,"prepare",G_CALLBACK(drag_name),drag_text);
        gtk_widget_add_controller(label,GTK_EVENT_CONTROLLER(drag));
        gtk_box_append(GTK_BOX(row),label); gtk_box_append(GTK_BOX(row),copy); gtk_list_box_append(GTK_LIST_BOX(w->list),row);
    }
    gtk_widget_set_sensitive(w->create,w->total>0);
    status(w,w->total==0 ? "No matching dates. Choose a wider range or another weekday." : json_array_get_length(dates)==0 ? "No search matches. Creation still uses the full date range." : "Review your folders, then choose Create Folders.");
    json_object_unref(value);
}
static void refresh(Workspace *w) {
    if(w->busy || w->closed) return;
    ++w->generation; gtk_widget_set_sensitive(w->create,FALSE);
    if(w->plan) { json_object_unref(w->plan); w->plan=NULL; }
    GtkWidget *child; while((child=gtk_widget_get_first_child(w->list))) gtk_list_box_remove(GTK_LIST_BOX(w->list),child);
    JsonObject *exact=request_new("exact");
    json_object_set_string_member(exact,"start",gtk_editable_get_text(GTK_EDITABLE(w->start)));
    json_object_set_string_member(exact,"end",gtk_editable_get_text(GTK_EDITABLE(w->end)));
    g_autoptr(GError) error=NULL; JsonObject *dates=core_call(exact,&error); json_object_unref(exact);
    if(!dates) { gtk_label_set_text(GTK_LABEL(w->count),"Check Your Dates"); status(w,error->message); return; }
    w->plan=json_object_new(); json_object_set_string_member(w->plan,"start",json_object_get_string_member(dates,"start")); json_object_set_string_member(w->plan,"end",json_object_get_string_member(dates,"end"));
    json_object_set_int_member(w->plan,"weekday",days[gtk_drop_down_get_selected(GTK_DROP_DOWN(w->weekday))]); json_object_unref(dates);
    JsonObject *request=request_new("plan"); json_object_set_object_member(request,"plan",json_object_ref(w->plan));
    json_object_set_string_member(request,"search",gtk_editable_get_text(GTK_EDITABLE(w->search)));
    status(w,"Calculating dates…"); core_async(w,request,plan_ready);
}
static void changed(GtkWidget *widget,gpointer data) { (void)widget; refresh(data); }
static void weekday_changed(GObject *object,GParamSpec *pspec,gpointer data) { (void)object;(void)pspec; refresh(data); }
static void set_busy(Workspace *w,gboolean busy) {
    w->busy=busy;
    if(w->closed) return;
    gtk_widget_set_sensitive(w->form,!busy); gtk_widget_set_sensitive(w->search,!busy);
    gtk_widget_set_sensitive(w->create,!busy && w->plan && w->total>0); gtk_widget_set_sensitive(w->cancel,busy);
}
static void creation_finished(Workspace *w) {
    if(w->handle) {
        JsonObject *release=request_new("release"); json_object_set_int_member(release,"handle",(gint64)w->handle);
        JsonObject *value=core_call(release,NULL); if(value) json_object_unref(value); json_object_unref(release); w->handle=0;
    }
    set_busy(w,FALSE); g_application_release(G_APPLICATION(w->application));
}
static void step_ready(GObject *source,GAsyncResult *result,gpointer data) {
    (void)data; Workspace *w=workspace(source); g_autoptr(GError) error=NULL; JsonObject *value=g_task_propagate_pointer(G_TASK(result),&error);
    if(!value) { status(w,error->message); message(w,"Folder Creation Stopped",error->message); creation_finished(w); return; }
    gint64 created=json_object_get_int_member(value,"created"), existing=json_object_get_int_member(value,"existing");
    if(!w->closed) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->progress),(double)(created+existing)/(double)MAX(1,w->total));
    if(json_object_get_boolean_member(value,"done")) {
        gboolean cancelled=json_object_get_boolean_member(value,"cancelled");
        const char *failure=json_object_get_null_member(value,"failureReason") ? NULL : json_object_get_string_member(value,"failureReason");
        g_autofree char *summary=g_strdup_printf("%sCreated %" G_GINT64_FORMAT " folders. Already existed: %" G_GINT64_FORMAT ". %s",cancelled ? "Canceled. " : "",created,existing,failure ? failure : cancelled ? "You can safely run the same range again." : "");
        status(w,summary); message(w,failure ? "Folder Creation Stopped" : cancelled ? "Creation Canceled" : "Complete",summary);
        creation_finished(w);
    } else {
        g_autofree char *text=g_strdup_printf("Processed %" G_GINT64_FORMAT " of %" G_GINT64_FORMAT " folders.",created+existing,w->total); status(w,text); create_step(w);
    }
    json_object_unref(value);
}
static void create_step(Workspace *w) {
    JsonObject *request=request_new("step"); json_object_set_int_member(request,"handle",(gint64)w->handle); json_object_set_boolean_member(request,"cancelled",w->cancelled || w->closed);
    core_async(w,request,step_ready);
}
static void begin_ready(GObject *source,GAsyncResult *result,gpointer data) {
    (void)data; Workspace *w=workspace(source); g_autoptr(GError) error=NULL; JsonObject *value=g_task_propagate_pointer(G_TASK(result),&error);
    if(!value) { status(w,error->message); message(w,"Folder Creation Stopped",error->message); creation_finished(w); return; }
    w->handle=(guint64)json_object_get_int_member(value,"handle"); json_object_unref(value); create_step(w);
}
static void create_begin(Workspace *w) {
    if(w->busy || w->closed || !w->plan || !w->destination) return;
    w->cancelled=FALSE; ++w->generation; set_busy(w,TRUE); gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->progress),0);
    g_application_hold(G_APPLICATION(w->application));
    JsonObject *request=request_new("begin"); json_object_set_object_member(request,"plan",json_object_ref(w->plan)); json_object_set_string_member(request,"destination",w->destination);
    core_async(w,request,begin_ready);
}
static void confirmed(AdwMessageDialog *dialog,const char *response,gpointer data) {
    (void)dialog; Workspace *w=data; if(strcmp(response,"continue")) return;
    if(w->destination) create_begin(w); else choose_folder(w,TRUE);
}
static void create_requested(Workspace *w) {
    if(w->busy || w->choosing || w->closed || !w->plan || !gtk_widget_get_sensitive(w->create)) return;
    if(w->confirmation) {
        g_autofree char *title=g_strdup_printf("Create %" G_GINT64_FORMAT " folders?",w->total);
        GtkWidget *dialog=adw_message_dialog_new(w->window,title,"This is a large operation. Existing folders and their contents will be preserved. You can cancel while it runs.");
        adw_message_dialog_add_responses(ADW_MESSAGE_DIALOG(dialog),"cancel","Cancel","continue","Continue",NULL);
        adw_message_dialog_set_close_response(ADW_MESSAGE_DIALOG(dialog),"cancel"); adw_message_dialog_set_default_response(ADW_MESSAGE_DIALOG(dialog),"cancel");
        g_signal_connect(dialog,"response",G_CALLBACK(confirmed),w); gtk_window_present(GTK_WINDOW(dialog));
    } else if(w->destination) create_begin(w); else choose_folder(w,TRUE);
}
typedef struct { GtkWindow *window; gboolean create_after; } Picker;
static void folder_selected(GObject *source,GAsyncResult *result,gpointer data) {
    Picker *picker=data; Workspace *w=workspace(G_OBJECT(picker->window)); g_autoptr(GError) error=NULL;
    g_autoptr(GFile) file=gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source),result,&error); w->choosing=FALSE;
    if(!w->closed && file) {
        char *path=g_file_get_path(file);
        if(path) { g_free(w->destination); w->destination=path; gtk_label_set_text(GTK_LABEL(w->destination_label),path); gtk_widget_set_sensitive(w->open,TRUE); if(picker->create_after) create_begin(w); }
        else message(w,"Choose a mounted folder","The destination must be available through the local filesystem or a mounted drive.");
    } else if(!w->closed && error && !g_error_matches(error,GTK_DIALOG_ERROR,GTK_DIALOG_ERROR_DISMISSED)) status(w,error->message);
    g_object_unref(picker->window); g_free(picker);
}
static void choose_folder(Workspace *w,gboolean create_after) {
    if(w->busy || w->choosing || w->closed) return;
    w->choosing=TRUE; GtkFileDialog *dialog=gtk_file_dialog_new(); gtk_file_dialog_set_title(dialog,"Choose where to create weekly folders");
    Picker *picker=g_new0(Picker,1); picker->window=g_object_ref(w->window); picker->create_after=create_after;
    gtk_file_dialog_select_folder(dialog,w->window,NULL,folder_selected,picker); g_object_unref(dialog);
}
static void create_clicked(GtkButton *b,gpointer data) { (void)b; create_requested(data); }
static void choose_clicked(GtkButton *b,gpointer data) { (void)b; choose_folder(data,FALSE); }
static void cancel_operation(Workspace *w) {
    w->cancelled=TRUE;
    if(w->handle) { JsonObject *request=request_new("cancel"); json_object_set_int_member(request,"handle",(gint64)w->handle); JsonObject *value=core_call(request,NULL); if(value) json_object_unref(value); json_object_unref(request); }
}
static void cancel_clicked(GtkButton *b,gpointer data) { (void)b; cancel_operation(data); }
static void open_clicked(GtkButton *b,gpointer data) {
    (void)b; Workspace *w=data; if(!w->destination) return;
    g_autofree char *uri=g_filename_to_uri(w->destination,NULL,NULL); gtk_show_uri(w->window,uri,GDK_CURRENT_TIME);
}
static void calendar_selected(GtkCalendar *calendar,gpointer data) {
    g_autoptr(GDateTime) date=gtk_calendar_get_date(calendar);
    gint32 ordinal=ewaf_date_ordinal(g_date_time_get_year(date),(guint32)g_date_time_get_month(date),(guint32)g_date_time_get_day_of_month(date));
    uint8_t name[11]={0}; if(ewaf_date_name(ordinal,name)) gtk_editable_set_text(GTK_EDITABLE(data),(char *)name);
}
static GtkWidget *date_entry(GtkWidget *box,const char *label,const char *initial) {
    GtkWidget *entry=gtk_entry_new(); gtk_editable_set_text(GTK_EDITABLE(entry),initial);
    gtk_accessible_update_property(GTK_ACCESSIBLE(entry),GTK_ACCESSIBLE_PROPERTY_LABEL,label,-1);
    GtkWidget *caption=gtk_label_new(label); gtk_label_set_xalign(GTK_LABEL(caption),0); gtk_box_append(GTK_BOX(box),caption);
    GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,6); gtk_widget_set_hexpand(entry,TRUE); gtk_box_append(GTK_BOX(row),entry);
    GtkWidget *button=gtk_menu_button_new(); gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button),"x-office-calendar-symbolic"); gtk_widget_set_tooltip_text(button,label);
    GtkWidget *popover=gtk_popover_new(), *calendar=gtk_calendar_new(); gtk_popover_set_child(GTK_POPOVER(popover),calendar); gtk_menu_button_set_popover(GTK_MENU_BUTTON(button),popover);
    g_signal_connect(calendar,"day-selected",G_CALLBACK(calendar_selected),entry); gtk_box_append(GTK_BOX(row),button); gtk_box_append(GTK_BOX(box),row); return entry;
}
static void settings_changed(GObject *object,GParamSpec *pspec,gpointer data) {
    (void)pspec; Workspace *w=data; guint index=gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    if(!g_settings_set_int(w->settings,"default-weekday",(gint)days[index])) status(w,"The default weekday could not be saved.");
}
static void settings_show(Workspace *w) {
    GtkWidget *window=adw_preferences_window_new(); gtk_window_set_title(GTK_WINDOW(window),"EWAF Settings"); gtk_window_set_transient_for(GTK_WINDOW(window),w->window); gtk_window_set_modal(GTK_WINDOW(window),TRUE); gtk_window_set_destroy_with_parent(GTK_WINDOW(window),TRUE);
    GtkWidget *page=adw_preferences_page_new(), *group=adw_preferences_group_new(), *row=adw_action_row_new();
    adw_preferences_group_set_description(ADW_PREFERENCES_GROUP(group),"Applies to new windows. Appearance and display scaling follow the system."); adw_preferences_window_add(ADW_PREFERENCES_WINDOW(window),ADW_PREFERENCES_PAGE(page)); adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),ADW_PREFERENCES_GROUP(group));
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row),"Default weekday");
    GtkWidget *choice=gtk_drop_down_new(g_object_ref(gtk_drop_down_get_model(GTK_DROP_DOWN(w->weekday))),NULL);
    for(guint i=0;i<7;i++) if(days[i]==(guint)g_settings_get_int(w->settings,"default-weekday")) gtk_drop_down_set_selected(GTK_DROP_DOWN(choice),i);
    gtk_accessible_update_property(GTK_ACCESSIBLE(choice),GTK_ACCESSIBLE_PROPERTY_LABEL,"Default weekday",-1);
    g_signal_connect(choice,"notify::selected",G_CALLBACK(settings_changed),w); adw_action_row_add_suffix(ADW_ACTION_ROW(row),choice); adw_preferences_group_add(ADW_PREFERENCES_GROUP(group),row); gtk_window_present(GTK_WINDOW(window));
}
static void window_action(GSimpleAction *action,GVariant *parameter,gpointer data) {
    (void)parameter; Workspace *w=data; const char *name=g_action_get_name(G_ACTION(action));
    if(!strcmp(name,"create")) create_requested(w);
    else if(!strcmp(name,"choose")) choose_folder(w,FALSE);
    else if(!strcmp(name,"cancel")) cancel_operation(w);
    else if(!strcmp(name,"settings")) settings_show(w);
    else if(!strcmp(name,"search")) gtk_widget_grab_focus(w->search);
    else if(!strcmp(name,"help")) gtk_show_uri(w->window,"https://github.com/tlolabs/ewaf#use",GDK_CURRENT_TIME);
    else if(!strcmp(name,"updates")) gtk_show_uri(w->window,"https://github.com/tlolabs/ewaf/releases",GDK_CURRENT_TIME);
    else if(!strcmp(name,"about")) {
        JsonObject *req=request_new("info"), *info=core_call(req,NULL); json_object_unref(req);
        message(w,"EWAF — Every Week a Folder",info ? json_object_get_string_member(info,"version") : "Version unavailable"); if(info) json_object_unref(info);
    }
}
static gboolean closing(GtkWindow *window,gpointer data) {
    Workspace *w=data; w->closed=TRUE; cancel_operation(w);
    int width,height; gtk_window_get_default_size(window,&width,&height);
    g_settings_set_int(w->settings,"width",width); g_settings_set_int(w->settings,"height",height);
    g_settings_set_string(w->settings,"range-start",gtk_editable_get_text(GTK_EDITABLE(w->start))); g_settings_set_string(w->settings,"range-end",gtk_editable_get_text(GTK_EDITABLE(w->end)));
    return FALSE;
}
static void workspace_free(gpointer data) { Workspace *w=data; if(w->plan) json_object_unref(w->plan); g_clear_object(&w->settings); g_clear_object(&w->application); g_free(w->destination); g_free(w); }
static void activate(GtkApplication *app,gpointer data) {
    (void)data; Workspace *w=g_new0(Workspace,1); w->application=g_object_ref(app); w->window=GTK_WINDOW(adw_application_window_new(app)); w->settings=g_settings_new("com.tlolabs.ewaf");
    g_object_set_data_full(G_OBJECT(w->window),"workspace",w,workspace_free);
    gtk_window_set_title(w->window,"EWAF — Every Week a Folder"); gtk_window_set_default_size(w->window,g_settings_get_int(w->settings,"width"),g_settings_get_int(w->settings,"height"));
    GtkWidget *outer=gtk_box_new(GTK_ORIENTATION_VERTICAL,0), *header=adw_header_bar_new(); gtk_box_append(GTK_BOX(outer),header); adw_application_window_set_content(ADW_APPLICATION_WINDOW(w->window),outer);
    GtkWidget *menu=gtk_menu_button_new(); gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu),"open-menu-symbolic");
    GMenu *model=g_menu_new();
    const char *labels[]={"New Window","Choose Destination…","Create Folders","Cancel Folder Creation","Settings","EWAF Help","Download Updates…","About EWAF"};
    const char *actions[]={"app.new-window","win.choose","win.create","win.cancel","win.settings","win.help","win.updates","win.about"};
    for(guint i=0;i<G_N_ELEMENTS(labels);i++) g_menu_append(model,labels[i],actions[i]);
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu),G_MENU_MODEL(model)); g_object_unref(model); adw_header_bar_pack_end(ADW_HEADER_BAR(header),menu);
    GSimpleActionGroup *group=g_simple_action_group_new(); const char *names[]={"create","choose","cancel","settings","search","help","updates","about"};
    for(guint i=0;i<G_N_ELEMENTS(names);i++) { GSimpleAction *action=g_simple_action_new(names[i],NULL); g_signal_connect(action,"activate",G_CALLBACK(window_action),w); g_action_map_add_action(G_ACTION_MAP(group),G_ACTION(action)); g_object_unref(action); }
    gtk_widget_insert_action_group(GTK_WIDGET(w->window),"win",G_ACTION_GROUP(group)); g_object_unref(group);
    GtkWidget *paned=gtk_paned_new(GTK_ORIENTATION_HORIZONTAL); gtk_widget_set_vexpand(paned,TRUE); gtk_box_append(GTK_BOX(outer),paned);
    w->form=gtk_box_new(GTK_ORIENTATION_VERTICAL,12); gtk_widget_set_margin_start(w->form,20); gtk_widget_set_margin_end(w->form,20); gtk_widget_set_margin_top(w->form,16); gtk_widget_set_margin_bottom(w->form,16);
    gtk_widget_set_size_request(w->form,260,-1);
    GtkWidget *form_scroll=gtk_scrolled_window_new(); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(form_scroll),w->form); gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(form_scroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC); gtk_paned_set_start_child(GTK_PANED(paned),form_scroll);
    g_autoptr(GDateTime) now=g_date_time_new_now_local(); uint8_t today[11]={0}; ewaf_date_name(ewaf_date_ordinal(g_date_time_get_year(now),(guint)g_date_time_get_month(now),(guint)g_date_time_get_day_of_month(now)),today);
    g_autofree char *saved_start=g_settings_get_string(w->settings,"range-start"), *saved_end=g_settings_get_string(w->settings,"range-end");
    w->start=date_entry(w->form,"Start date (MM-DD-YYYY)",*saved_start ? saved_start : (char *)today); w->end=date_entry(w->form,"End date (MM-DD-YYYY)",*saved_end ? saved_end : (char *)today);
    char *weekdays[8]={0}; for(guint i=0;i<7;i++) { g_autoptr(GDateTime) day=g_date_time_new_local(2024,1,(gint)i+1,12,0,0); weekdays[i]=g_date_time_format(day,"%A"); }
    w->weekday=gtk_drop_down_new_from_strings((const char *const *)weekdays); for(guint i=0;i<7;i++) g_free(weekdays[i]);
    gtk_accessible_update_property(GTK_ACCESSIBLE(w->weekday),GTK_ACCESSIBLE_PROPERTY_LABEL,"Weekday",-1);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(w->weekday),3);
    for(guint i=0;i<7;i++) if(days[i]==(guint)g_settings_get_int(w->settings,"default-weekday")) gtk_drop_down_set_selected(GTK_DROP_DOWN(w->weekday),i);
    gtk_box_append(GTK_BOX(w->form),gtk_label_new("Weekday")); gtk_box_append(GTK_BOX(w->form),w->weekday);
    GtkWidget *instructions=gtk_label_new("Both dates are included. Exact entry supports years 0001–9999."); gtk_label_set_wrap(GTK_LABEL(instructions),TRUE); gtk_box_append(GTK_BOX(w->form),instructions);
    w->destination_label=gtk_label_new("Choose an existing destination folder."); gtk_label_set_wrap(GTK_LABEL(w->destination_label),TRUE); gtk_label_set_selectable(GTK_LABEL(w->destination_label),TRUE); gtk_box_append(GTK_BOX(w->form),w->destination_label);
    w->choose=gtk_button_new_with_label("Choose Destination…"); w->open=gtk_button_new_with_label("Open Folder"); gtk_widget_set_sensitive(w->open,FALSE); gtk_box_append(GTK_BOX(w->form),w->choose); gtk_box_append(GTK_BOX(w->form),w->open);
    GtkWidget *right=gtk_box_new(GTK_ORIENTATION_VERTICAL,10); gtk_widget_set_margin_start(right,16); gtk_widget_set_margin_end(right,16); gtk_widget_set_margin_top(right,16); gtk_widget_set_margin_bottom(right,16); gtk_widget_set_size_request(right,280,-1); gtk_paned_set_end_child(GTK_PANED(paned),right);
    w->count=gtk_label_new("Folder Preview"); gtk_box_append(GTK_BOX(right),w->count); w->search=gtk_search_entry_new(); gtk_accessible_update_property(GTK_ACCESSIBLE(w->search),GTK_ACCESSIBLE_PROPERTY_LABEL,"Find a folder date",-1); gtk_box_append(GTK_BOX(right),w->search);
    w->list=gtk_list_box_new(); gtk_widget_add_css_class(w->list,"boxed-list"); GtkWidget *scroll=gtk_scrolled_window_new(); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),w->list); gtk_widget_set_vexpand(scroll,TRUE); gtk_box_append(GTK_BOX(right),scroll);
    GtkWidget *note=gtk_label_new("Preview shows up to 200 matches in date order. Existing folders will be kept."); gtk_label_set_wrap(GTK_LABEL(note),TRUE); gtk_box_append(GTK_BOX(right),note);
    GtkWidget *footer=gtk_box_new(GTK_ORIENTATION_VERTICAL,8); gtk_widget_set_margin_start(footer,20); gtk_widget_set_margin_end(footer,20); gtk_widget_set_margin_top(footer,12); gtk_widget_set_margin_bottom(footer,16); gtk_box_append(GTK_BOX(outer),footer);
    GtkWidget *buttons=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8); w->create=gtk_button_new_with_label("Create Folders"); gtk_widget_add_css_class(w->create,"suggested-action"); w->cancel=gtk_button_new_with_label("Cancel"); gtk_widget_set_sensitive(w->cancel,FALSE); gtk_box_append(GTK_BOX(buttons),w->create); gtk_box_append(GTK_BOX(buttons),w->cancel); gtk_box_append(GTK_BOX(footer),buttons);
    w->progress=gtk_progress_bar_new(); gtk_accessible_update_property(GTK_ACCESSIBLE(w->progress),GTK_ACCESSIBLE_PROPERTY_LABEL,"Folder creation progress",-1); gtk_box_append(GTK_BOX(footer),w->progress);
    w->status=gtk_label_new("Choose a date range and weekday."); gtk_label_set_wrap(GTK_LABEL(w->status),TRUE); gtk_label_set_selectable(GTK_LABEL(w->status),TRUE); gtk_box_append(GTK_BOX(footer),w->status);
    g_signal_connect(w->start,"changed",G_CALLBACK(changed),w); g_signal_connect(w->end,"changed",G_CALLBACK(changed),w); g_signal_connect(w->weekday,"notify::selected",G_CALLBACK(weekday_changed),w); g_signal_connect(w->search,"changed",G_CALLBACK(changed),w);
    g_signal_connect(w->create,"clicked",G_CALLBACK(create_clicked),w); g_signal_connect(w->choose,"clicked",G_CALLBACK(choose_clicked),w); g_signal_connect(w->cancel,"clicked",G_CALLBACK(cancel_clicked),w); g_signal_connect(w->open,"clicked",G_CALLBACK(open_clicked),w); g_signal_connect(w->window,"close-request",G_CALLBACK(closing),w);
    gtk_window_present(w->window); refresh(w);
}
static void new_window(GSimpleAction *action,GVariant *parameter,gpointer app) { (void)action;(void)parameter; activate(app,NULL); }
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--core-smoke")) {
        JsonObject *req=request_new("info"); g_autoptr(GError) error=NULL; JsonObject *info=core_call(req,&error); json_object_unref(req);
        if(!info) { g_printerr("%s\n",error->message); return 1; }
        g_print("EWAF %s ABI %" G_GINT64_FORMAT "\n",json_object_get_string_member(info,"version"),json_object_get_int_member(info,"abi")); json_object_unref(info); return 0;
    }
    g_autoptr(AdwApplication) app=adw_application_new("com.tlolabs.ewaf",G_APPLICATION_DEFAULT_FLAGS);
    GSimpleAction *action=g_simple_action_new("new-window",NULL); g_signal_connect(action,"activate",G_CALLBACK(new_window),app); g_action_map_add_action(G_ACTION_MAP(app),G_ACTION(action)); g_object_unref(action);
    const char *action_names[]={"app.new-window","win.choose","win.create","win.cancel","win.settings","win.search"};
    const char *keys[]={"<Control>n","<Control>o","<Control>Return","Escape","<Control>comma","<Control>f"};
    for(guint i=0;i<G_N_ELEMENTS(keys);i++) { const char *accels[]={keys[i],NULL}; gtk_application_set_accels_for_action(GTK_APPLICATION(app),action_names[i],accels); }
    g_signal_connect(app,"activate",G_CALLBACK(activate),NULL); return g_application_run(G_APPLICATION(app),argc,argv);
}
