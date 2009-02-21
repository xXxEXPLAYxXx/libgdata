/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
 * 
 * GData Client is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GData Client is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GData Client.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libsoup/soup.h>
#include <string.h>

#include "gdata-service.h"
#include "gdata-private.h"
#include "gdata-marshal.h"

GQuark
gdata_service_error_quark (void)
{
	return g_quark_from_static_string ("gdata-service-error-quark");
}

static void gdata_service_dispose (GObject *object);
static void gdata_service_finalize (GObject *object);
static void gdata_service_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gdata_service_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static gboolean real_parse_authentication_response (GDataService *self, const gchar *response_body, GError **error);
static void real_append_query_headers (GDataService *self, SoupMessage *message);

struct _GDataServicePrivate {
	SoupSession *session;

	gchar *username;
	gchar *password;
	gchar *auth_token;
	gchar *client_id;
	gboolean logged_in;
};

enum {
	PROP_CLIENT_ID = 1,
	PROP_USERNAME,
	PROP_PASSWORD,
	PROP_LOGGED_IN
};

/*enum {
	SIGNAL_QUERY_FINISHED,
	LAST_SIGNAL
};

static guint service_signals[LAST_SIGNAL] = { 0, };*/

G_DEFINE_TYPE (GDataService, gdata_service, G_TYPE_OBJECT)
#define GDATA_SERVICE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GDATA_TYPE_SERVICE, GDataServicePrivate))

static void
gdata_service_class_init (GDataServiceClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GDataServicePrivate));

	gobject_class->set_property = gdata_service_set_property;
	gobject_class->get_property = gdata_service_get_property;
	gobject_class->dispose = gdata_service_dispose;
	gobject_class->finalize = gdata_service_finalize;

	klass->service_name = "xapi";
	klass->authentication_uri = "https://www.google.com/accounts/ClientLogin";
	klass->parse_authentication_response = real_parse_authentication_response;
	klass->append_query_headers = real_append_query_headers;

	g_object_class_install_property (gobject_class, PROP_CLIENT_ID,
				g_param_spec_string ("client-id",
					"Client ID", "A client ID for your application (see http://code.google.com/apis/youtube/2.0/developers_guide_protocol_api_query_parameters.html#clientsp).",
					NULL,
					G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_USERNAME,
				g_param_spec_string ("username",
					"Username", "TODO",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_PASSWORD,
				g_param_spec_string ("password",
					"Password", "TODO",
					NULL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_LOGGED_IN,
				g_param_spec_boolean ("logged-in",
					"Logged in", "TODO",
					FALSE,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/*service_signals[SIGNAL_QUERY_FINISHED] = g_signal_new ("query-finished",
				G_TYPE_FROM_CLASS (klass),
				G_SIGNAL_RUN_LAST,
				0, NULL, NULL,
				gdata_marshal_VOID__OBJECT_OBJECT_POINTER,
				G_TYPE_NONE, 0);*/
}

static void
gdata_service_init (GDataService *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GDATA_TYPE_SERVICE, GDataServicePrivate);
	self->priv->session = soup_session_sync_new ();
}

static void
gdata_service_dispose (GObject *object)
{
	GDataServicePrivate *priv = GDATA_SERVICE_GET_PRIVATE (object);

	if (priv->session != NULL)
		g_object_unref (priv->session);
	priv->session = NULL;

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_service_parent_class)->dispose (object);
}

static void
gdata_service_finalize (GObject *object)
{
	GDataServicePrivate *priv = GDATA_SERVICE_GET_PRIVATE (object);

	g_free (priv->username);
	g_free (priv->password);
	g_free (priv->auth_token);
	g_free (priv->client_id);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_service_parent_class)->finalize (object);
}

static void
gdata_service_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	GDataServicePrivate *priv = GDATA_SERVICE_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_CLIENT_ID:
			g_value_set_string (value, priv->client_id);
			break;
		case PROP_USERNAME:
			g_value_set_string (value, priv->username);
			break;
		case PROP_PASSWORD:
			g_value_set_string (value, priv->password);
			break;
		case PROP_LOGGED_IN:
			g_value_set_boolean (value, priv->logged_in);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
gdata_service_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	GDataServicePrivate *priv = GDATA_SERVICE_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_CLIENT_ID:
			priv->client_id = g_value_dup_string (value);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
real_parse_authentication_response (GDataService *self, const gchar *response_body, GError **error)
{
	gchar *auth_start, *auth_end;

	/* Parse the response */
	auth_start = strstr (response_body, "Auth=");
	if (auth_start == NULL)
		goto protocol_error;
	auth_start += strlen ("Auth=");

	auth_end = strstr (auth_start, "\n");
	if (auth_end == NULL)
		goto protocol_error;

	self->priv->auth_token = g_strndup (auth_start, auth_end - auth_start);
	if (self->priv->auth_token == NULL || strlen (self->priv->auth_token) == 0)
		goto protocol_error;

	return TRUE;

protocol_error:
	g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_PROTOCOL_ERROR,
		     _("The server returned a malformed response."));
	return FALSE;
}

static void
real_append_query_headers (GDataService *self, SoupMessage *message)
{
	gchar *authorisation_header;

	g_assert (message != NULL);

	/* Set the authorisation header */
	authorisation_header = g_strdup_printf ("GoogleLogin auth=%s", self->priv->auth_token);
	soup_message_headers_append (message->request_headers, "Authorization", authorisation_header);
	g_free (authorisation_header);

	/* Set the GData-Version header to tell it we want to use the v2 API */
	soup_message_headers_append (message->request_headers, "GData-Version", "2");
}

typedef struct {
	/* Input */
	gchar *username;
	gchar *password;

	/* Output */
	GDataService *service;
	gboolean success;
} AuthenticateAsyncData;

static void
authenticate_async_data_free (AuthenticateAsyncData *self)
{
	g_free (self->username);
	g_free (self->password);

	g_slice_free (AuthenticateAsyncData, self);
}

static gboolean
set_authentication_details_cb (AuthenticateAsyncData *data)
{
	GObject *service = G_OBJECT (data->service);
	GDataServicePrivate *priv = data->service->priv;

	g_free (priv->username);
	priv->username = g_strdup (data->username);
	g_free (priv->password);
	priv->password = g_strdup (data->password);
	priv->logged_in = TRUE;

	g_object_freeze_notify (service);
	g_object_notify (service, "username");
	g_object_notify (service, "password");
	g_object_notify (service, "logged-in");
	g_object_thaw_notify (service);

	return FALSE;
}

static void
authenticate_thread (GSimpleAsyncResult *result, GDataService *service, GCancellable *cancellable)
{
	GError *error = NULL;
	AuthenticateAsyncData *data = g_simple_async_result_get_op_res_gpointer (result);

	/* Check to see if it's been cancelled already */
	if (g_cancellable_set_error_if_cancelled (cancellable, &error) == TRUE) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
		return;
	}

	/* Authenticate and return */
	data->success = gdata_service_authenticate (service, data->username, data->password, cancellable, &error);
	if (data->success == FALSE) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	}

	/* Update the authentication details held by the service.
	 * This should always be executed before the callback in g_simple_async_result_run_in_thread ---
	 * if not, data will already have been freed, and crashage will happen. */
	data->service = service;
	g_idle_add ((GSourceFunc) set_authentication_details_cb, data);
}

void
gdata_service_authenticate_async (GDataService *self, const gchar *username, const gchar *password,
				  GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GSimpleAsyncResult *result;
	AuthenticateAsyncData *data;

	g_return_if_fail (GDATA_IS_SERVICE (self));
	g_return_if_fail (username != NULL);
	g_return_if_fail (password != NULL);

	data = g_slice_new (AuthenticateAsyncData);
	data->username = g_strdup (username);
	data->password = g_strdup (password);

	result = g_simple_async_result_new (G_OBJECT (self), callback, user_data, gdata_service_authenticate_async);
	g_simple_async_result_set_op_res_gpointer (result, data, (GDestroyNotify) authenticate_async_data_free);
	g_simple_async_result_run_in_thread (result, (GSimpleAsyncThreadFunc) authenticate_thread, G_PRIORITY_DEFAULT, cancellable);
	g_object_unref (result);
}

gboolean
gdata_service_authenticate_finish (GDataService *self, GAsyncResult *async_result, GError **error)
{
	GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (async_result);
	AuthenticateAsyncData *data;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (async_result), FALSE);

	g_warn_if_fail (g_simple_async_result_get_source_tag (result) == gdata_service_authenticate_async);

	if (g_simple_async_result_propagate_error (result, error) == TRUE)
		return FALSE;

	data = g_simple_async_result_get_op_res_gpointer (result);
	if (data->success == TRUE)
		return TRUE;

	g_assert_not_reached ();
}

gboolean
gdata_service_authenticate (GDataService *self, const gchar *username, const gchar *password, GCancellable *cancellable, GError **error)
{
	GDataServicePrivate *priv = self->priv;
	GDataServiceClass *klass;
	SoupMessage *message;
	gchar *request_body;
	guint status;
	gboolean retval;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), FALSE);
	g_return_val_if_fail (username != NULL, FALSE);
	g_return_val_if_fail (password != NULL, FALSE);

	

	/* Prepare the request */
	klass = GDATA_SERVICE_GET_CLASS (self);
	request_body = soup_form_encode ("accountType", "HOSTED_OR_GOOGLE",
					 "Email", username,
					 "Passwd", password,
					 "service", klass->service_name,
					 "source", priv->client_id,
					 NULL);

	/* Build the message */
	message = soup_message_new (SOUP_METHOD_POST, klass->authentication_uri);
	soup_message_set_request (message, "application/x-www-form-urlencoded", SOUP_MEMORY_TAKE, request_body, strlen (request_body));

	/* Send the message */
	status = soup_session_send_message (priv->session, message);

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return FALSE;
	}

	if (status != 200) {
		/* Error */
		/* TODO: Handle CAPTCHA requests and other errors more specifically */
		g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATING,
			     _("TODO: error code %u when authenticating"), status);
		g_object_unref (message);
		priv->logged_in = FALSE;
		g_object_notify (G_OBJECT (self), "logged-in");
		return FALSE;
	}

	g_assert (message->response_body->data != NULL);

	retval = klass->parse_authentication_response (self, message->response_body->data, error);
	g_object_unref (message);

	g_object_freeze_notify (G_OBJECT (self));
	priv->logged_in = retval;

	if (retval == TRUE) {
		/* Update several properties the service holds */
		g_free (priv->username);
		priv->username = g_strdup (username);
		g_free (priv->password);
		priv->password = g_strdup (password);

		g_object_notify (G_OBJECT (self), "username");
		g_object_notify (G_OBJECT (self), "password");
	}

	g_object_notify (G_OBJECT (self), "logged-in");
	g_object_thaw_notify (G_OBJECT (self));

	return retval;
}

typedef struct {
	/* Input */
	gchar *feed_uri;
	GDataQuery *query;
	GDataEntryParserFunc parser_func;

	/* Output */
	GDataFeed *feed;
} QueryAsyncData;

static void
query_async_data_free (QueryAsyncData *self)
{
	g_free (self->feed_uri);
	if (self->query)
		g_object_unref (self->query);
	if (self->feed)
		g_object_unref (self->feed);

	g_slice_free (QueryAsyncData, self);
}

static void
query_thread (GSimpleAsyncResult *result, GDataService *service, GCancellable *cancellable)
{
	GError *error = NULL;
	QueryAsyncData *data = g_simple_async_result_get_op_res_gpointer (result);

	/* Check to see if it's been cancelled already */
	if (g_cancellable_set_error_if_cancelled (cancellable, &error) == TRUE) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
		return;
	}

	/* Execute the query and return */
	data->feed = gdata_service_query (service, data->feed_uri, data->query, data->parser_func, cancellable, &error);
	if (data->feed == NULL) {
		g_simple_async_result_set_from_error (result, error);
		g_error_free (error);
	}
}

void
gdata_service_query_async (GDataService *self, const gchar *feed_uri, GDataQuery *query, GDataEntryParserFunc parser_func,
			   GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GSimpleAsyncResult *result;
	QueryAsyncData *data;

	g_return_if_fail (GDATA_IS_SERVICE (self));
	g_return_if_fail (feed_uri != NULL);
	g_return_if_fail (GDATA_IS_QUERY (query));
	g_return_if_fail (parser_func != NULL);

	data = g_slice_new (QueryAsyncData);
	data->feed_uri = g_strdup (feed_uri);
	data->query = g_object_ref (query);
	data->parser_func = parser_func;

	result = g_simple_async_result_new (G_OBJECT (self), callback, user_data, gdata_service_query_async);
	g_simple_async_result_set_op_res_gpointer (result, data, (GDestroyNotify) query_async_data_free);
	g_simple_async_result_run_in_thread (result, (GSimpleAsyncThreadFunc) query_thread, G_PRIORITY_DEFAULT, cancellable);
	g_object_unref (result);
}

GDataFeed *
gdata_service_query_finish (GDataService *self, GAsyncResult *async_result, GError **error)
{
	GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (async_result);
	QueryAsyncData *data;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (async_result), NULL);

	g_warn_if_fail (g_simple_async_result_get_source_tag (result) == gdata_service_query_async);

	if (g_simple_async_result_propagate_error (result, error) == TRUE)
		return NULL;

	data = g_simple_async_result_get_op_res_gpointer (result);
	if (data->feed != NULL)
		return g_object_ref (data->feed);

	g_assert_not_reached ();
}

GDataFeed *
gdata_service_query (GDataService *self, const gchar *feed_uri, GDataQuery *query, GDataEntryParserFunc parser_func,
		     GCancellable *cancellable, GError **error)
{
	GDataServiceClass *klass;
	GDataFeed *feed;
	SoupMessage *message;
	gchar *query_uri;
	guint status;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);

	query_uri = gdata_query_get_query_uri (query, feed_uri);

	message = soup_message_new (SOUP_METHOD_GET, query_uri);
	g_free (query_uri);

	/* Make sure subclasses set their headers */
	klass = GDATA_SERVICE_GET_CLASS (self);
	if (klass->append_query_headers != NULL)
		klass->append_query_headers (self, message);

	/* Send the message */
	status = soup_session_send_message (self->priv->session, message);

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return FALSE;
	}

	if (status != 200) {
		/* Error */
		/* TODO: Handle errors more specifically, making sure to set logged_in where appropriate */
		g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_WITH_QUERY,
			     _("TODO: error code %u when querying"), status);
		g_object_unref (message);
		return FALSE;
	}

	g_assert (message->response_body->data != NULL);

	feed = _gdata_feed_new_from_xml (message->response_body->data, message->response_body->length, parser_func, error);
	g_object_unref (message);

	return feed;
}

gboolean
gdata_service_insert_entry (GDataService *self, const gchar *upload_uri, GDataEntry *entry, GCancellable *cancellable, GError **error)
{
	GDataServiceClass *klass;
	SoupMessage *message;
	gchar *upload_data;
	guint status;

	g_return_val_if_fail (GDATA_IS_SERVICE (self), FALSE);
	g_return_val_if_fail (upload_uri != NULL, FALSE);
	g_return_val_if_fail (GDATA_IS_ENTRY (entry), FALSE);

	if (gdata_entry_inserted (entry) == TRUE) {
		g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_ENTRY_ALREADY_INSERTED,
			     _("The entry has already been inserted."));
		return FALSE;
	}

	message = soup_message_new (SOUP_METHOD_POST, upload_uri);

	/* Make sure subclasses set their headers */
	klass = GDATA_SERVICE_GET_CLASS (self);
	if (klass->append_query_headers != NULL)
		klass->append_query_headers (self, message);

	/* Append the data */
	upload_data = gdata_entry_get_xml (entry);
	soup_message_set_request (message, "application/atom+xml", SOUP_MEMORY_TAKE, upload_data, strlen (upload_data));

	/* Send the message */
	status = soup_session_send_message (self->priv->session, message);

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return FALSE;
	}

	if (status != 201) {
		/* Error */
		/* TODO: Handle errors more specifically, making sure to set logged_in where appropriate */
		g_set_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_WITH_INSERTION,
			     _("TODO: error code %u when inserting"), status);
		g_object_unref (message);
		return FALSE;
	}

	g_assert (message->response_body->data != NULL);

	return TRUE;
}

gboolean
gdata_service_is_logged_in (GDataService *self)
{
	g_assert (GDATA_IS_SERVICE (self));
	return self->priv->logged_in;
}

const gchar *
gdata_service_get_client_id (GDataService *self)
{
	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	return self->priv->client_id;
}

const gchar *
gdata_service_get_username (GDataService *self)
{
	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	return self->priv->username;
}

const gchar *
gdata_service_get_password (GDataService *self)
{
	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	return self->priv->password;
}

SoupSession *
gdata_service_get_session (GDataService *self)
{
	g_return_val_if_fail (GDATA_IS_SERVICE (self), NULL);
	return self->priv->session;
}
