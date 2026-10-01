#define main ewaf_application_main
#include "../../platform/linux/src/main.c"
#undef main
#include <glib/gstdio.h>
static void wait_until_ready(Workspace *w, gboolean creation) {
  gint64 deadline = g_get_monotonic_time() + 10 * G_TIME_SPAN_SECOND;
  do {
    while (g_main_context_iteration(NULL, FALSE)) {
    }
    if (creation ? !w->busy : gtk_widget_get_sensitive(w->create))
      return;
    g_usleep(1000);
  } while (g_get_monotonic_time() < deadline);
  g_error("Native widget operation timed out");
}
static GtkWindow *find_message_dialog(GtkWindow *parent) {
  GListModel *windows = gtk_window_get_toplevels();
  for (guint i = 0; i < g_list_model_get_n_items(windows); i++) {
    GtkWindow *window = g_list_model_get_item(windows, i);
    if (ADW_IS_MESSAGE_DIALOG(window) && gtk_widget_get_visible(GTK_WIDGET(window)) && gtk_window_get_transient_for(window) == parent)
      return window;
    g_object_unref(window);
  }
  return NULL;
}
static void wait_until_false(gboolean *value) {
  gint64 deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
  while (*value && g_get_monotonic_time() < deadline) {
    while (g_main_context_iteration(NULL, FALSE)) {}
    g_usleep(1000);
  }
  g_assert_false(*value);
}
static GtkWidget *find_switch(GtkWidget *widget) {
  if (GTK_IS_SWITCH(widget)) return widget;
  for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
       child = gtk_widget_get_next_sibling(child)) {
    GtkWidget *found = find_switch(child);
    if (found) return found;
  }
  return NULL;
}
static void updater_widgets(GtkApplication *app, Workspace *w) {
  gtk_editable_set_text(GTK_EDITABLE(w->start), "01-01-2000");
  gtk_editable_set_text(GTK_EDITABLE(w->end), "01-01-2030");
  refresh(w);
  wait_until_ready(w, FALSE);
  g_assert_true(w->confirmation_required);
  g_assert_false(update_busy(w)); /* An idle large plan is not an open dialog. */
  activate(app, GINT_TO_POINTER(TRUE));
  GtkWindow *other_window = NULL;
  for (GList *it = gtk_application_get_windows(app); it; it = it->next)
    if (it->data != w->window) { other_window = it->data; break; }
  g_assert_nonnull(other_window);
  g_object_ref(other_window);
  Workspace *other = workspace(G_OBJECT(other_window));
  set_busy(other, TRUE);
  g_assert_true(update_busy(w));
  update_start(w, "check", NULL, TRUE);
  g_assert_false(update_running);
  set_busy(other, FALSE);
  g_assert_false(update_busy(w));
  create_requested(w);
  g_assert_true(w->confirmation_open);
  g_assert_true(update_busy(other));
  g_autoptr(GtkWindow) confirmation = find_message_dialog(w->window);
  g_assert_nonnull(confirmation);
  adw_message_dialog_response(ADW_MESSAGE_DIALOG(confirmation), "cancel");
  wait_until_false(&w->confirmation_open);
  g_assert_false(w->confirmation_open);
  g_assert_true(w->confirmation_required);
  g_assert_false(update_busy(other));
  g_assert_false(w->busy);
  settings_show(w);
  g_assert_true(update_busy(other));
  GtkWidget *automatic = find_switch(GTK_WIDGET(w->settings_window));
  g_assert_nonnull(automatic);
  gtk_switch_set_active(GTK_SWITCH(automatic), FALSE);
  g_assert_false(g_settings_get_boolean(w->settings, "automatic-updates"));
  g_assert_cmpint(update_tick(other), ==, G_SOURCE_CONTINUE);
  g_assert_false(update_running);
  gtk_window_destroy(w->settings_window);
  g_assert_false(update_busy(other));
  settings_show(w);
  automatic = find_switch(GTK_WIDGET(w->settings_window));
  g_assert_false(gtk_switch_get_active(GTK_SWITCH(automatic)));
  gtk_window_destroy(w->settings_window);
  /* Feed a completed check into the native presentation boundary; choosing Later
   * must not start a download or change the idle plan. */
  g_autoptr(GTask) check = g_task_new(w->window, NULL, NULL, NULL);
  UpdateRequest *request = g_new0(UpdateRequest, 1);
  request->command = g_strdup("check");
  g_task_set_task_data(check, request, update_request_free);
  g_task_return_pointer(check, g_strdup("{\"checked_at\":123,\"available\":true,\"version\":\"1.0.6\",\"notes\":\"https://github.com/tlolabs/ewaf/releases/tag/v1.0.6\"}"), g_free);
  update_completed(G_OBJECT(w->window), G_ASYNC_RESULT(check), NULL);
  g_assert_true(w->update_dialog);
  g_assert_true(update_busy(other));
  g_autoptr(GtkWindow) update_dialog = find_message_dialog(w->window);
  g_assert_nonnull(update_dialog);
  adw_message_dialog_response(ADW_MESSAGE_DIALOG(update_dialog), "later");
  wait_until_false(&w->update_dialog);
  g_assert_false(w->update_dialog);
  g_assert_false(update_running);
  g_assert_false(update_busy(other));
  g_assert_true(w->confirmation_required);
  other->update_timer = g_timeout_add_seconds(3600, update_tick, other);
  gtk_window_close(other_window);
  g_assert_true(other->closed);
  g_assert_cmpuint(other->update_timer, ==, 0);
  g_assert_cmpint(update_tick(other), ==, G_SOURCE_REMOVE);
  update_start(other, "check", NULL, TRUE);
  g_assert_false(update_running);
  g_object_unref(other_window);
}
int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  adw_init();
  /* Deterministic dialog completion; warnings and assertions remain fatal. */
  g_object_set(gtk_settings_get_default(), "gtk-enable-animations", FALSE, NULL);
  g_autoptr(AdwApplication) app =
      adw_application_new("com.tlolabs.ewaf.tests", G_APPLICATION_NON_UNIQUE);
  g_assert_true(g_application_register(G_APPLICATION(app), NULL, NULL));
  activate(GTK_APPLICATION(app), GINT_TO_POINTER(TRUE));
  GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(app));
  if (!window)
    window =
        g_list_last(gtk_application_get_windows(GTK_APPLICATION(app)))->data;
  Workspace *w = workspace(G_OBJECT(window));
  g_assert_nonnull(w);
  g_assert_cmpstr(gtk_window_get_icon_name(window), ==, "com.tlolabs.ewaf");
  g_assert_true(gtk_icon_theme_has_icon(
      gtk_icon_theme_get_for_display(gdk_display_get_default()), "com.tlolabs.ewaf"));
  gtk_editable_set_text(GTK_EDITABLE(w->start), "09-03-2026");
  gtk_editable_set_text(GTK_EDITABLE(w->end), "09-17-2026");
  gtk_drop_down_set_selected(GTK_DROP_DOWN(w->weekday), 3);
  refresh(w);
  wait_until_ready(w, FALSE);
  g_assert_cmpint(w->total, ==, 3);
  gtk_editable_set_text(GTK_EDITABLE(w->search), "09-10");
  refresh(w);
  wait_until_ready(w, FALSE);
  g_assert_cmpint(w->total, ==, 3);
  g_assert_nonnull(gtk_list_box_get_row_at_index(GTK_LIST_BOX(w->list), 0));
  g_assert_null(gtk_list_box_get_row_at_index(GTK_LIST_BOX(w->list), 1));
  w->destination = g_dir_make_tmp("EWAF-widget-test-XXXXXX", NULL);
  g_assert_nonnull(w->destination);
  create_begin(w);
  g_assert_true(update_busy(w));
  wait_until_ready(w, TRUE);
  g_autofree char *keep =
      g_build_filename(w->destination, "09-03-2026", "keep.txt", NULL);
  g_assert_true(g_file_set_contents(keep, "Preserve contents", -1, NULL));
  create_begin(w);
  g_assert_true(update_busy(w));
  wait_until_ready(w, TRUE);
  g_autofree char *contents = NULL;
  g_assert_true(g_file_get_contents(keep, &contents, NULL, NULL));
  g_assert_cmpstr(contents, ==, "Preserve contents");
  g_assert_nonnull(
      strstr(gtk_label_get_text(GTK_LABEL(w->status)), "Already existed: 3"));
  gtk_editable_set_text(GTK_EDITABLE(w->start), "02-29-2025");
  refresh(w);
  g_assert_false(gtk_widget_get_sensitive(w->create));
  g_remove(keep);
  const char *names[] = {"09-03-2026", "09-10-2026", "09-17-2026"};
  for (guint i = 0; i < 3; i++) {
    g_autofree char *path = g_build_filename(w->destination, names[i], NULL);
    g_rmdir(path);
  }
  g_rmdir(w->destination);
  g_clear_pointer(&w->destination, g_free);
  updater_widgets(GTK_APPLICATION(app), w);
  closing(window, w);
  while (gtk_application_get_windows(GTK_APPLICATION(app)))
    gtk_window_destroy(gtk_application_get_windows(GTK_APPLICATION(app))->data);
  while (g_main_context_iteration(NULL, FALSE)) {
  }
  g_print("GTK widgets, search, validation, creation, retry and updater lifecycle passed.\n");
  return 0;
}
