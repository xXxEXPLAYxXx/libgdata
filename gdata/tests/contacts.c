/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2009 <philip@tecnocode.co.uk>
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

#include <glib.h>
#include <unistd.h>

#include "gdata.h"
#include "common.h"

/* TODO: probably a better way to do this; some kind of data associated with the test suite? */
static GDataService *service = NULL;
static GMainLoop *main_loop = NULL;

static void
test_authentication (void)
{
	gboolean retval;
	GError *error = NULL;

	/* Create a service */
	service = GDATA_SERVICE (gdata_contacts_service_new (CLIENT_ID));

	g_assert (service != NULL);
	g_assert (GDATA_IS_SERVICE (service));
	g_assert_cmpstr (gdata_service_get_client_id (service), ==, CLIENT_ID);

	/* Log in */
	retval = gdata_service_authenticate (service, USERNAME, PASSWORD, NULL, &error);
	g_assert_no_error (error);
	g_assert (retval == TRUE);
	g_clear_error (&error);

	/* Check all is as it should be */
	g_assert (gdata_service_is_authenticated (service) == TRUE);
	g_assert_cmpstr (gdata_service_get_username (service), ==, USERNAME);
	g_assert_cmpstr (gdata_service_get_password (service), ==, PASSWORD);
}

static void
test_query_all_contacts (void)
{
	GDataFeed *feed;
	GError *error = NULL;

	g_assert (service != NULL);

	feed = gdata_contacts_service_query_contacts (GDATA_CONTACTS_SERVICE (service), NULL, NULL, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (GDATA_IS_FEED (feed));
	g_clear_error (&error);

	/* TODO: check entries and feed properties */

	g_object_unref (feed);
}

static void
test_query_all_contacts_async_cb (GDataService *service, GAsyncResult *async_result, gpointer user_data)
{
	GDataFeed *feed;
	GError *error = NULL;

	feed = gdata_service_query_finish (service, async_result, &error);
	g_assert_no_error (error);
	g_assert (GDATA_IS_FEED (feed));
	g_clear_error (&error);

	/* TODO: Tests? */
	g_main_loop_quit (main_loop);

	g_object_unref (feed);
}

static void
test_query_all_contacts_async (void)
{
	g_assert (service != NULL);

	gdata_contacts_service_query_contacts_async (GDATA_CONTACTS_SERVICE (service), NULL, NULL, NULL,
						     NULL, (GAsyncReadyCallback) test_query_all_contacts_async_cb, NULL);

	main_loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);
}

static void
test_insert_simple (void)
{
	GDataContactsContact *contact, *new_contact;
	GDataCategory *category;
	GDataGDEmailAddress *email_address1, *email_address2;
	GDataGDPhoneNumber *phone_number1, *phone_number2;
	GDataGDIMAddress *im_address;
	GDataGDPostalAddress *postal_address;
	gchar *xml;
	GError *error = NULL;

	g_assert (service != NULL);

	contact = gdata_contacts_contact_new (NULL);

	gdata_entry_set_title (GDATA_ENTRY (contact), "Elizabeth Bennet");
	gdata_entry_set_content (GDATA_ENTRY (contact), "Notes");
	/* TODO: Have it add this category automatically? Same for GDataCalendarEvent */
	category = gdata_category_new ("http://schemas.google.com/contact/2008#contact", "http://schemas.google.com/g/2005#kind", NULL);
	gdata_entry_add_category (GDATA_ENTRY (contact), category);
	email_address1 = gdata_gd_email_address_new ("liz@gmail.com", "http://schemas.google.com/g/2005#work", NULL, FALSE);
	gdata_contacts_contact_add_email_address (contact, email_address1);
	email_address2 = gdata_gd_email_address_new ("liz@example.org", "http://schemas.google.com/g/2005#home", NULL, FALSE);
	gdata_contacts_contact_add_email_address (contact, email_address2);
	phone_number1 = gdata_gd_phone_number_new ("(206)555-1212", "http://schemas.google.com/g/2005#work", NULL, NULL, TRUE);
	gdata_contacts_contact_add_phone_number (contact, phone_number1);
	phone_number2 = gdata_gd_phone_number_new ("(206)555-1213", "http://schemas.google.com/g/2005#home", NULL, NULL, FALSE);
	gdata_contacts_contact_add_phone_number (contact, phone_number2);
	im_address = gdata_gd_im_address_new ("liz@gmail.com", "http://schemas.google.com/g/2005#GOOGLE_TALK", "http://schemas.google.com/g/2005#home",
					      NULL, FALSE);
	gdata_contacts_contact_add_im_address (contact, im_address);
	postal_address = gdata_gd_postal_address_new ("1600 Amphitheatre Pkwy Mountain View", "http://schemas.google.com/g/2005#work", NULL, TRUE);
	gdata_contacts_contact_add_postal_address (contact, postal_address);

	/* Check the XML */
	xml = gdata_entry_get_xml (GDATA_ENTRY (contact));
	g_assert_cmpstr (xml, ==,
			 "<entry xmlns='http://www.w3.org/2005/Atom' "
			 	"xmlns:gd='http://schemas.google.com/g/2005' "
			 	"xmlns:app='http://www.w3.org/2007/app' "
			 	"xmlns:gContact='http://schemas.google.com/contact/2008'>"
			 	"<title type='text'>Elizabeth Bennet</title>"
			 	"<content type='text'>Notes</content>"
				"<category term='http://schemas.google.com/contact/2008#contact' scheme='http://schemas.google.com/g/2005#kind'/>"
				"<gd:email address='liz@gmail.com' rel='http://schemas.google.com/g/2005#work' primary='false'/>"
				"<gd:email address='liz@example.org' rel='http://schemas.google.com/g/2005#home' primary='false'/>"
				"<gd:im address='liz@gmail.com' protocol='http://schemas.google.com/g/2005#GOOGLE_TALK' "
					"rel='http://schemas.google.com/g/2005#home' primary='false'/>"
				"<gd:phoneNumber rel='http://schemas.google.com/g/2005#work' primary='true'>(206)555-1212</gd:phoneNumber>"
				"<gd:phoneNumber rel='http://schemas.google.com/g/2005#home' primary='false'>(206)555-1213</gd:phoneNumber>"
				"<gd:postalAddress rel='http://schemas.google.com/g/2005#work' primary='true'>"
					"1600 Amphitheatre Pkwy Mountain View"
				"</gd:postalAddress>"
			 "</entry>");
	g_free (xml);

	/* Insert the contact */
	new_contact = gdata_contacts_service_insert_contact (GDATA_CONTACTS_SERVICE (service), contact, NULL, &error);
	g_assert_no_error (error);
	g_assert (GDATA_IS_CONTACTS_CONTACT (new_contact));
	g_clear_error (&error);

	/* TODO: check entries and feed properties */

	g_object_unref (contact);
	g_object_unref (new_contact);
}

int
main (int argc, char *argv[])
{
	gint retval;

	g_type_init ();
	g_thread_init (NULL);
	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	g_test_add_func ("/contacts/authentication", test_authentication);
	g_test_add_func ("/contacts/query/all_contacts", test_query_all_contacts);
	if (g_test_thorough () == TRUE)
		g_test_add_func ("/contacts/query/all_contacts_async", test_query_all_contacts_async);
	if (g_test_slow () == TRUE)
		g_test_add_func ("/contacts/insert/simple", test_insert_simple);

	retval = g_test_run ();
	if (service != NULL)
		g_object_unref (service);

	return retval;
}