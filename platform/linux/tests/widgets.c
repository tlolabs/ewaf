#define main ewaf_application_main
#include "../src/main.c"
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
int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  adw_init();
  g_autoptr(AdwApplication) app =
      adw_application_new("com.tlolabs.ewaf.tests", G_APPLICATION_NON_UNIQUE);
  g_assert_true(g_application_register(G_APPLICATION(app), NULL, NULL));
  activate(GTK_APPLICATION(app), NULL);
  GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(app));
  if (!window)
    window =
        g_list_last(gtk_application_get_windows(GTK_APPLICATION(app)))->data;
  Workspace *w = workspace(G_OBJECT(window));
  g_assert_nonnull(w);
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
  wait_until_ready(w, TRUE);
  g_autofree char *keep =
      g_build_filename(w->destination, "09-03-2026", "keep.txt", NULL);
  g_assert_true(g_file_set_contents(keep, "Preserve contents", -1, NULL));
  create_begin(w);
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
  w->closed = TRUE;
  while (gtk_application_get_windows(GTK_APPLICATION(app)))
    gtk_window_destroy(gtk_application_get_windows(GTK_APPLICATION(app))->data);
  while (g_main_context_iteration(NULL, FALSE)) {
  }
  g_print("GTK widgets, search, validation, creation and retry passed.\n");
  return 0;
}
