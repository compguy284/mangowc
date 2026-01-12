/* Scroller column management for vertical tiling within columns */

struct ScrollerColumn {
	struct wl_list link;           /* Link in monitor's scroller_columns list */
	struct wl_list tiles;          /* List of Clients via column_link */
	int32_t active_tile_idx;       /* Index of focused tile within column */
	int32_t tile_count;            /* Number of tiles in column */
	float proportion;              /* Column width proportion (0.0-1.0) */
	Monitor *mon;                  /* Monitor this column belongs to */
};

/* Column lifecycle */
ScrollerColumn *column_create(Monitor *m, float proportion) {
	ScrollerColumn *col = ecalloc(1, sizeof(*col));
	wl_list_init(&col->link);
	wl_list_init(&col->tiles);
	col->active_tile_idx = 0;
	col->tile_count = 0;
	col->proportion = proportion;
	col->mon = m;
	return col;
}

void column_destroy(ScrollerColumn *col) {
	if (!col)
		return;
	wl_list_remove(&col->link);
	free(col);
}

/* Get tile at specific index within column */
Client *column_tile_at(ScrollerColumn *col, int32_t idx) {
	if (!col || idx < 0 || idx >= col->tile_count)
		return NULL;

	Client *c;
	int32_t i = 0;
	wl_list_for_each(c, &col->tiles, column_link) {
		if (i == idx)
			return c;
		i++;
	}
	return NULL;
}

/* Reindex tiles after insertion/removal */
void column_reindex_tiles(ScrollerColumn *col) {
	if (!col)
		return;

	Client *c;
	int32_t i = 0;
	wl_list_for_each(c, &col->tiles, column_link) {
		c->tile_index = i;
		i++;
	}
}

/* Add client to column at position (-1 means append at end) */
void column_add_tile(ScrollerColumn *col, Client *c, int32_t position) {
	if (!col || !c)
		return;

	/* Remove from previous column if any */
	if (c->column) {
		wl_list_remove(&c->column_link);
		c->column->tile_count--;
		column_reindex_tiles(c->column);

		/* Adjust active index if needed */
		if (c->column->active_tile_idx >= c->column->tile_count && c->column->tile_count > 0)
			c->column->active_tile_idx = c->column->tile_count - 1;
	}

	c->column = col;

	if (position < 0 || position >= col->tile_count) {
		/* Append at end */
		wl_list_insert(col->tiles.prev, &c->column_link);
		c->tile_index = col->tile_count;
	} else {
		/* Insert at specific position */
		Client *target = column_tile_at(col, position);
		if (target) {
			wl_list_insert(target->column_link.prev, &c->column_link);
		} else {
			wl_list_insert(col->tiles.prev, &c->column_link);
		}
	}

	col->tile_count++;
	column_reindex_tiles(col);
}

/* Remove tile from column by index, returns the removed client */
Client *column_remove_tile(ScrollerColumn *col, int32_t idx) {
	if (!col || idx < 0 || idx >= col->tile_count)
		return NULL;

	Client *c = column_tile_at(col, idx);
	if (!c)
		return NULL;

	wl_list_remove(&c->column_link);
	wl_list_init(&c->column_link);
	c->column = NULL;
	c->tile_index = -1;
	col->tile_count--;

	/* Adjust active index if needed */
	if (col->active_tile_idx >= col->tile_count && col->tile_count > 0)
		col->active_tile_idx = col->tile_count - 1;

	column_reindex_tiles(col);
	return c;
}

/* Remove specific client from its column */
void column_remove_client(Client *c) {
	if (!c || !c->column)
		return;

	ScrollerColumn *col = c->column;
	wl_list_remove(&c->column_link);
	wl_list_init(&c->column_link);
	c->column = NULL;
	c->tile_index = -1;
	col->tile_count--;

	if (col->active_tile_idx >= col->tile_count && col->tile_count > 0)
		col->active_tile_idx = col->tile_count - 1;

	column_reindex_tiles(col);
}

/* Get column at specific index in monitor's column list */
ScrollerColumn *column_at_index(Monitor *m, int32_t idx) {
	if (!m || idx < 0 || idx >= m->column_count)
		return NULL;

	ScrollerColumn *col;
	int32_t i = 0;
	wl_list_for_each(col, &m->scroller_columns, link) {
		if (i == idx)
			return col;
		i++;
	}
	return NULL;
}

/* Get index of column in monitor's column list */
int32_t column_index_of(Monitor *m, ScrollerColumn *col) {
	if (!m || !col)
		return -1;

	ScrollerColumn *c;
	int32_t i = 0;
	wl_list_for_each(c, &m->scroller_columns, link) {
		if (c == col)
			return i;
		i++;
	}
	return -1;
}

/* Insert column at specific index in monitor's column list */
void column_insert_at(Monitor *m, ScrollerColumn *col, int32_t idx) {
	if (!m || !col)
		return;

	if (idx <= 0 || wl_list_empty(&m->scroller_columns)) {
		/* Insert at beginning */
		wl_list_insert(&m->scroller_columns, &col->link);
	} else if (idx >= m->column_count) {
		/* Insert at end */
		wl_list_insert(m->scroller_columns.prev, &col->link);
	} else {
		/* Insert before the column at idx */
		ScrollerColumn *target = column_at_index(m, idx);
		if (target) {
			wl_list_insert(target->link.prev, &col->link);
		} else {
			wl_list_insert(m->scroller_columns.prev, &col->link);
		}
	}
	m->column_count++;
}

/* Remove column from monitor's list (does not destroy) */
void column_remove(Monitor *m, ScrollerColumn *col) {
	if (!m || !col)
		return;

	wl_list_remove(&col->link);
	wl_list_init(&col->link);
	m->column_count--;

	if (m->active_column_idx >= m->column_count && m->column_count > 0)
		m->active_column_idx = m->column_count - 1;
}

/* Get focused client in column */
Client *column_get_active_tile(ScrollerColumn *col) {
	if (!col || col->tile_count == 0)
		return NULL;
	return column_tile_at(col, col->active_tile_idx);
}

/* Set active tile index */
void column_set_active_tile(ScrollerColumn *col, int32_t idx) {
	if (!col || idx < 0 || idx >= col->tile_count)
		return;
	col->active_tile_idx = idx;
}

/* Check if column has only one tile */
bool column_is_single_tile(ScrollerColumn *col) {
	return col && col->tile_count == 1;
}

/* Get column for a client */
ScrollerColumn *column_for_client(Client *c) {
	return c ? c->column : NULL;
}

/* Initialize column list for monitor */
void column_init_for_monitor(Monitor *m) {
	if (!m)
		return;
	wl_list_init(&m->scroller_columns);
	m->active_column_idx = 0;
	m->column_count = 0;
}

/* Cleanup all columns for monitor */
void column_cleanup_for_monitor(Monitor *m) {
	if (!m)
		return;

	ScrollerColumn *col, *tmp;
	wl_list_for_each_safe(col, tmp, &m->scroller_columns, link) {
		/* Clear client references */
		Client *c, *ctmp;
		wl_list_for_each_safe(c, ctmp, &col->tiles, column_link) {
			c->column = NULL;
			c->tile_index = -1;
			wl_list_init(&c->column_link);
		}
		column_destroy(col);
	}
	wl_list_init(&m->scroller_columns);
	m->column_count = 0;
	m->active_column_idx = 0;
}

/* Render tiles within a column with vertical tiling */
void scroller_render_column(ScrollerColumn *col, struct wlr_box *geom, int32_t gap) {
	if (!col || col->tile_count == 0 || !geom)
		return;

	int32_t tile_count = col->tile_count;
	int32_t total_gap = (tile_count - 1) * gap;
	int32_t tile_height = (geom->height - total_gap) / tile_count;
	int32_t y = geom->y;

	Client *c;
	int32_t i = 0;
	wl_list_for_each(c, &col->tiles, column_link) {
		struct wlr_box tile_geom = {
			.x = geom->x,
			.y = y,
			.width = geom->width,
			.height = tile_height
		};

		/* Last tile gets remaining height to avoid rounding errors */
		if (i == tile_count - 1) {
			tile_geom.height = geom->y + geom->height - y;
		}

		resize(c, tile_geom, 0);
		y += tile_height + gap;
		i++;
	}
}

/* Migrate existing flat client list to column structure */
void scroller_migrate_to_columns(Monitor *m) {
	if (!m)
		return;

	/* Only migrate if columns don't exist yet */
	if (!wl_list_empty(&m->scroller_columns))
		return;

	wl_list_init(&m->scroller_columns);
	m->column_count = 0;
	m->active_column_idx = 0;

	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;

		if (!VISIBLEON(c, m) || !ISSCROLLTILED(c))
			continue;

		/* Each existing window becomes its own column */
		ScrollerColumn *col = column_create(m, c->scroller_proportion);
		column_add_tile(col, c, 0);
		wl_list_insert(m->scroller_columns.prev, &col->link);
		m->column_count++;

		if (c == m->sel) {
			m->active_column_idx = m->column_count - 1;
		}
	}
}

/* Check if columns need rebuilding (e.g., after tag switch) */
bool scroller_columns_need_rebuild(Monitor *m) {
	if (!m)
		return false;

	/* If we have no columns but have visible clients, need to build */
	if (wl_list_empty(&m->scroller_columns) && m->visible_scroll_tiling_clients > 0)
		return true;

	/* Check if any visible client is not in a column */
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && ISSCROLLTILED(c) && !c->column)
			return true;
	}

	return false;
}

/* Rebuild columns after tag switch or similar */
void scroller_rebuild_columns(Monitor *m) {
	if (!m)
		return;

	/* Clean up existing columns */
	column_cleanup_for_monitor(m);

	/* Rebuild from scratch */
	scroller_migrate_to_columns(m);
}

/* Add a single new client to columns (called when window is mapped) */
void scroller_add_client_to_columns(Client *c) {
	if (!c || !c->mon)
		return;

	Monitor *m = c->mon;

	/* Skip if not in scroller layout or client not tiled */
	if (!VISIBLEON(c, m) || !ISSCROLLTILED(c))
		return;

	/* Skip if already in a column */
	if (c->column)
		return;

	/* Find where to insert the column */
	int32_t insert_idx = m->active_column_idx + 1;

	/* Try to find the client's position relative to other clients in columns */
	Client *prev_client = NULL;
	struct wl_list *pos = c->link.prev;
	while (pos != &clients) {
		Client *pc = wl_container_of(pos, pc, link);
		if (pc->mon == m && VISIBLEON(pc, m) && ISSCROLLTILED(pc) && pc->column) {
			prev_client = pc;
			break;
		}
		pos = pos->prev;
	}

	if (prev_client && prev_client->column) {
		insert_idx = column_index_of(m, prev_client->column) + 1;
	}

	/* Create a new column for this client */
	float proportion = c->scroller_proportion > 0 ? c->scroller_proportion : scroller_default_proportion;
	ScrollerColumn *col = column_create(m, proportion);
	column_add_tile(col, c, 0);
	column_insert_at(m, col, insert_idx);
	m->active_column_idx = insert_idx;
}

/* Remove a client from columns (called when window is unmapped/destroyed) */
void scroller_remove_client_from_columns(Client *c) {
	if (!c || !c->column)
		return;

	ScrollerColumn *col = c->column;
	Monitor *m = col->mon;

	column_remove_client(c);

	/* Destroy empty column */
	if (col->tile_count == 0) {
		column_remove(m, col);
		column_destroy(col);
	}
}

/*
 * Sync the global clients list order to match the scroller column order.
 * This ensures that when switching to other layouts (tile, grid, etc.),
 * windows maintain their relative positions as arranged in scroller layout.
 */
void scroller_sync_clients_to_columns(Monitor *m) {
	if (!m || wl_list_empty(&m->scroller_columns))
		return;

	/* Collect all scroller-tiled clients from columns in order */
	ScrollerColumn *col;
	Client *c;
	struct wl_list reordered;
	wl_list_init(&reordered);

	wl_list_for_each(col, &m->scroller_columns, link) {
		wl_list_for_each(c, &col->tiles, column_link) {
			if (c->mon == m && VISIBLEON(c, m) && ISSCROLLTILED(c)) {
				/* Temporarily move to reordered list */
				wl_list_remove(&c->link);
				wl_list_insert(reordered.prev, &c->link);
			}
		}
	}

	/* Re-insert the reordered clients at the beginning of the clients list */
	while (!wl_list_empty(&reordered)) {
		struct wl_list *first = reordered.next;
		wl_list_remove(first);
		wl_list_insert(&clients, first);
	}
}
