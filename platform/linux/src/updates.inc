/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native scheduling/preferences/dialogs. The Rust helper owns trust, policy and downloads. */
#ifdef __linux__
#include <gnu/libc-version.h>
#else
/* GTK widget tests also compile on macOS; AppImage updating is Linux-only. */
static const char *gnu_get_libc_version(void) { return "0"; }
#endif
static gboolean update_running;
typedef struct { gchar *command, *version; gboolean manual; } UpdateRequest;
static void update_start(Workspace *w, const char *command, const char *version, gboolean manual);
static gboolean update_busy(Workspace *w) {
  for (GList *it = gtk_application_get_windows(w->application); it; it = it->next) {
    Workspace *other = g_object_get_data(G_OBJECT(it->data), "workspace");
    if (other && (other->busy || other->confirmation_open || other->choosing || other->update_dialog ||
                  (other->settings_window && gtk_widget_get_visible(GTK_WIDGET(other->settings_window))))) return TRUE;
  }
  return FALSE;
}
static void update_request_free(gpointer data) {
  UpdateRequest *r = data; g_free(r->command); g_free(r->version); g_free(r);
}
static void update_worker(GTask *task, gpointer source, gpointer data, GCancellable *cancel) {
  (void)source;
  UpdateRequest *r = data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *executable = g_file_read_link("/proc/self/exe", &error);
  if (!executable) { g_task_return_error(task, g_steal_pointer(&error)); return; }
  g_autofree gchar *directory = g_path_get_dirname(executable);
  g_autofree gchar *helper = g_build_filename(directory, "ewaf-update", NULL);
  const gchar *argv[] = {helper, r->command, gnu_get_libc_version(), r->version, NULL};
  g_autoptr(GSubprocess) process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
  gchar *output = NULL;
  if (!process || !g_subprocess_communicate_utf8(process, NULL, cancel, &output, NULL, &error)) {
    g_free(output); g_task_return_error(task, g_steal_pointer(&error)); return;
  }
  g_task_return_pointer(task, output, g_free);
}
static void update_response(AdwMessageDialog *dialog, const char *response, gpointer data) {
  Workspace *w = data;
  const char *version = g_object_get_data(G_OBJECT(dialog), "update-version");
  w->update_dialog = FALSE;
  if (!strcmp(response, "install") && !w->closed && !update_busy(w))
    update_start(w, "install-appimage", version, TRUE);
  else if (!strcmp(response, "manual"))
    gtk_show_uri(w->window, "https://github.com/tlolabs/ewaf/releases", GDK_CURRENT_TIME);
}
static void update_completed(GObject *source, GAsyncResult *result, gpointer data) {
  (void)data;
  Workspace *w = g_object_get_data(source, "workspace");
  GTask *task = G_TASK(result);
  UpdateRequest *request = g_task_get_task_data(task);
  g_autoptr(GError) error = NULL;
  g_autofree gchar *output = g_task_propagate_pointer(task, &error);
  update_running = FALSE;
  if (w->closed) return;
  g_autoptr(JsonParser) parser = json_parser_new();
  if (!output || !json_parser_load_from_data(parser, output, -1, &error)) {
    if (request->manual) message(w, "Update unavailable", error ? error->message : "The update helper failed.");
    return;
  }
  JsonObject *reply = json_node_get_object(json_parser_get_root(parser));
  if (json_object_has_member(reply, "error")) {
    if (request->manual) message(w, "Update unavailable", json_object_get_string_member(reply, "error"));
    return;
  }
  if (!strcmp(request->command, "check")) {
    g_settings_set_int64(w->settings, "update-last-success", json_object_get_int_member(reply, "checked_at"));
    if (update_busy(w)) return;
    if (!json_object_get_boolean_member(reply, "available")) {
      if (request->manual) message(w, "EWAF Updates", "No compatible newer stable release is available.");
      return;
    }
    const char *version = json_object_get_string_member(reply, "version");
    g_autofree gchar *title = g_strdup_printf("EWAF %s is available", version);
    gboolean appimage = g_getenv("APPIMAGE") != NULL;
    g_autofree gchar *body = g_strdup_printf("%s\n\nRelease notes: %s",
      appimage ? "Download, authenticate and replace this AppImage? A previous copy will be retained. Reopen EWAF when you are ready; this does not close your windows."
               : "This installation is managed by your package manager. Install the signed AppImage from GitHub to enable automatic replacement; your existing installation stays usable.",
      json_object_get_string_member(reply, "notes"));
    w->update_dialog = TRUE;
    GtkWidget *dialog = adw_message_dialog_new(w->window, title, body);
    adw_message_dialog_add_response(ADW_MESSAGE_DIALOG(dialog), "later", "Later");
    adw_message_dialog_add_response(ADW_MESSAGE_DIALOG(dialog), appimage ? "install" : "manual", appimage ? "Download and Update" : "Open Releases");
    adw_message_dialog_set_default_response(ADW_MESSAGE_DIALOG(dialog), "later");
    adw_message_dialog_set_close_response(ADW_MESSAGE_DIALOG(dialog), "later");
    g_object_set_data_full(G_OBJECT(dialog), "update-version", g_strdup(version), g_free);
    g_signal_connect(dialog, "response", G_CALLBACK(update_response), w);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_present(GTK_WINDOW(dialog));
  } else if (json_object_has_member(reply, "installed")) {
    g_autofree gchar *body = g_strdup_printf("The authenticated update is installed. Reopen EWAF when ready. Previous version: %s", json_object_get_string_member(reply, "backup"));
    message(w, "EWAF updated", body);
  }
}
static void update_start(Workspace *w, const char *command, const char *version, gboolean manual) {
  if (update_running || w->closed || update_busy(w)) return;
  update_running = TRUE;
  UpdateRequest *r = g_new0(UpdateRequest, 1);
  r->command = g_strdup(command); r->version = g_strdup(version); r->manual = manual;
  g_settings_set_int64(w->settings, "update-last-attempt", g_get_real_time() / G_USEC_PER_SEC);
  GTask *task = g_task_new(w->window, NULL, update_completed, NULL);
  g_task_set_task_data(task, r, update_request_free);
  g_task_run_in_thread(task, update_worker); g_object_unref(task);
}
static void update_due_done(GObject *source, GAsyncResult *result, gpointer data) {
  (void)data;
  GtkWindow *window = g_object_get_data(source, "update-window");
  Workspace *w = g_object_get_data(G_OBJECT(window), "workspace");
  gchar *output = NULL;
  g_autoptr(GError) error = NULL;
  if (g_subprocess_communicate_utf8_finish(G_SUBPROCESS(source), result, &output, NULL, &error) && !w->closed) {
    g_autoptr(JsonParser) parser = json_parser_new();
    if (json_parser_load_from_data(parser, output, -1, NULL)) {
      JsonObject *reply = json_node_get_object(json_parser_get_root(parser));
      if (json_object_has_member(reply, "due") && json_object_get_boolean_member(reply, "due")) update_start(w, "check", NULL, FALSE);
    }
  }
  g_free(output);
  g_object_unref(source);
}
static gboolean update_tick(gpointer data) {
  Workspace *w = data;
  if (w->closed) return G_SOURCE_REMOVE;
  if (update_running || update_busy(w) || !g_settings_get_boolean(w->settings, "automatic-updates")) return G_SOURCE_CONTINUE;
  g_autofree gchar *exe = g_file_read_link("/proc/self/exe", NULL);
  if (!exe) return G_SOURCE_CONTINUE;
  g_autofree gchar *directory = g_path_get_dirname(exe);
  g_autofree gchar *helper = g_build_filename(directory, "ewaf-update", NULL);
  g_autofree gchar *success = g_strdup_printf("%" G_GINT64_FORMAT, g_settings_get_int64(w->settings, "update-last-success"));
  g_autofree gchar *attempt = g_strdup_printf("%" G_GINT64_FORMAT, g_settings_get_int64(w->settings, "update-last-attempt"));
  GSubprocess *process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL, helper, "due", success, attempt, "1", NULL);
  if (process) {
    g_object_set_data_full(G_OBJECT(process), "update-window", g_object_ref(w->window), g_object_unref);
    g_subprocess_communicate_utf8_async(process, NULL, NULL, update_due_done, NULL);
  }
  return G_SOURCE_CONTINUE;
}
