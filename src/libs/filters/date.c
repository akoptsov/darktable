/*
    This file is part of darktable,
    Copyright (C) 2022 darktable developers.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
  This file contains the necessary routines to implement a filter for the filtering module
*/


static gboolean _date_update(dt_lib_filtering_rule_t *rule)
{
  if(!rule->w_specific) return FALSE;

  dt_lib_filtering_t *d = rule->lib;
  _widgets_range_t *special = (_widgets_range_t *)rule->w_specific;
  _widgets_range_t *specialtop = (_widgets_range_t *)rule->w_specific_top;
  GtkDarktableRangeSelect *range = DTGTK_RANGE_SELECT(special->range_select);
  GtkDarktableRangeSelect *rangetop = (specialtop) ? DTGTK_RANGE_SELECT(specialtop->range_select) : NULL;

  rule->manual_widget_set++;
  // first, we update the graph
  char query[1024] = { 0 };
  g_snprintf(query, sizeof(query),
             "SELECT SUBSTR(datetime_taken, 1, 19) AS date, COUNT(*) AS count"
             " FROM main.images AS mi"
             " WHERE datetime_taken IS NOT NULL AND LENGTH(datetime_taken)>=19 AND %s"
             " GROUP BY date",
             d->last_where_ext);
  sqlite3_stmt *stmt;
  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), query, -1, &stmt, NULL);
  dtgtk_range_select_reset_blocks(range);
  if(rangetop) dtgtk_range_select_reset_blocks(rangetop);
  while(sqlite3_step(stmt) == SQLITE_ROW)
  {
    const int count = sqlite3_column_int(stmt, 1);

    GDateTime *dt = dt_datetime_exif_to_gdatetime((const char *)sqlite3_column_text(stmt, 0), darktable.utc_tz);
    if(dt)
    {
      dtgtk_range_select_add_block(range, g_date_time_to_unix(dt), count);
      if(rangetop) dtgtk_range_select_add_block(rangetop, g_date_time_to_unix(dt), count);
      g_date_time_unref(dt);
    }
  }
  sqlite3_finalize(stmt);

  // and setup the selection
  dtgtk_range_select_set_selection_from_raw_text(range, rule->raw_text, FALSE);
  if(rangetop) dtgtk_range_select_set_selection_from_raw_text(rangetop, rule->raw_text, FALSE);
  rule->manual_widget_set--;

  dtgtk_range_select_redraw(range);
  if(rangetop) dtgtk_range_select_redraw(rangetop);
  return TRUE;
}

static void _date_widget_init(dt_lib_filtering_rule_t *rule, const dt_collection_properties_t prop,
                              const gchar *text, dt_lib_module_t *self, const gboolean top)
{
  _widgets_range_t *special = (_widgets_range_t *)g_malloc0(sizeof(_widgets_range_t));

  special->range_select
      = dtgtk_range_select_new(dt_collection_name_untranslated(prop), !top, DT_RANGE_TYPE_DATETIME);
  if(top) gtk_widget_set_size_request(special->range_select, 160, -1);
  GtkDarktableRangeSelect *range = DTGTK_RANGE_SELECT(special->range_select);

  range->type = DT_RANGE_TYPE_DATETIME;
  range->step_bd = 86400; // step of 1 day (in seconds)
  dtgtk_range_select_set_selection_from_raw_text(range, text, FALSE);

  char query[1024] = { 0 };
  g_snprintf(query, sizeof(query),
             "SELECT SUBSTR(MIN(datetime_taken),1,19), SUBSTR(MAX(datetime_taken),1,19)"
             " FROM main.images"
             " WHERE datetime_taken IS NOT NULL AND LENGTH(datetime_taken)>=19");
  sqlite3_stmt *stmt;
  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), query, -1, &stmt, NULL);
  gchar *min = NULL;
  gchar *max = NULL;
  if(sqlite3_step(stmt) == SQLITE_ROW)
  {
    min = g_strdup((const char *)sqlite3_column_text(stmt, 0));
    max = g_strdup((const char *)sqlite3_column_text(stmt, 1));
  }
  sqlite3_finalize(stmt);
  if(min && max)
  {
    GDateTime *dtmin = dt_datetime_exif_to_gdatetime(min, darktable.utc_tz);
    if(dtmin)
    {
      range->min_r = g_date_time_to_unix(dtmin);
      g_date_time_unref(dtmin);
    }
    g_free(min);
    GDateTime *dtmax = dt_datetime_exif_to_gdatetime(max, darktable.utc_tz);
    if(dtmax)
    {
      range->max_r = g_date_time_to_unix(dtmax);
      g_date_time_unref(dtmax);
    }
    g_free(max);
  }

  _range_widget_add_to_rule(rule, special, top);
}