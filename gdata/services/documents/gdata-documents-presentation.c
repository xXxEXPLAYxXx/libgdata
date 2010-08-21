/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Thibault Saunier 2009 <saunierthibault@gmail.com>
 * Copyright (C) Philip Withnall 2010 <philip@tecnocode.co.uk>
 *
 * GData Client is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GData Client is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GData Client.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gdata-documents-presentation
 * @short_description: GData documents presentation object
 * @stability: Unstable
 * @include: gdata/services/documents/gdata-documents-presentation.h
 *
 * #GDataDocumentsPresentation is a subclass of #GDataDocumentsDocument to represent a Google Documents presentation.
 *
 * For more details of Google Documents' GData API, see the
 * <ulink type="http://code.google.com/apis/document/docs/2.0/developers_guide_protocol.html">online documentation</ulink>.
 *
 * Since: 0.4.0
 **/

#include <config.h>
#include <glib.h>

#include "gdata-documents-presentation.h"

static void get_xml (GDataParsable *parsable, GString *xml_string);

G_DEFINE_TYPE (GDataDocumentsPresentation, gdata_documents_presentation, GDATA_TYPE_DOCUMENTS_DOCUMENT)

static void
gdata_documents_presentation_class_init (GDataDocumentsPresentationClass *klass)
{
	GDataParsableClass *parsable_class = GDATA_PARSABLE_CLASS (klass);
	GDataEntryClass *entry_class = GDATA_ENTRY_CLASS (klass);

	parsable_class->get_xml = get_xml;
	entry_class->kind_term = "http://schemas.google.com/docs/2007#presentation";
}

static void
gdata_documents_presentation_init (GDataDocumentsPresentation *self)
{
	/* Why am I writing it? */
}

static void
get_xml (GDataParsable *parsable, GString *xml_string)
{
	const gchar *document_id;

	/* Chain up to the parent class */
	GDATA_PARSABLE_CLASS (gdata_documents_presentation_parent_class)->get_xml (parsable, xml_string);

	document_id = gdata_documents_entry_get_document_id (GDATA_DOCUMENTS_ENTRY (parsable));
	if (document_id != NULL)
		g_string_append_printf (xml_string, "<gd:resourceId>presentation:%s</gd:resourceId>", document_id);
}

/**
 * gdata_documents_presentation_new:
 * @id: (allow-none): the entry's ID (not the document ID of the presentation), or %NULL
 *
 * Creates a new #GDataDocumentsPresentation with the given entry ID (#GDataEntry:id).
 *
 * Return value: a new #GDataDocumentsPresentation, or %NULL; unref with g_object_unref()
 *
 * Since: 0.4.0
 **/
GDataDocumentsPresentation *
gdata_documents_presentation_new (const gchar *id)
{
	return GDATA_DOCUMENTS_PRESENTATION (g_object_new (GDATA_TYPE_DOCUMENTS_PRESENTATION, "id", id, NULL));
}
