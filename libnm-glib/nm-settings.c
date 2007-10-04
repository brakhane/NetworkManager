#include <NetworkManager.h>
#include <nm-utils.h>
#include "nm-settings.h"


GError *
nm_settings_new_error (const gchar *format, ...)
{
	GError *err;
	va_list args;
	gchar *msg;
	static GQuark domain_quark = 0;

	if (domain_quark == 0)
		domain_quark = g_quark_from_static_string ("nm_settings_error");

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	err = g_error_new_literal (domain_quark, 1, (const gchar *) msg);

	g_free (msg);

	return err;
}

/*
 * NMSettings implementation
 */

static gboolean impl_settings_list_connections (NMSettings *settings, GPtrArray **connections, GError **error);

#include "nm-settings-glue.h"

#define SETTINGS_CLASS(o) (NM_SETTINGS_CLASS (G_OBJECT_GET_CLASS (o)))

G_DEFINE_TYPE (NMSettings, nm_settings, G_TYPE_OBJECT)

enum {
	S_NEW_CONNECTION,

	S_LAST_SIGNAL
};

static guint settings_signals[S_LAST_SIGNAL] = { 0 };

static gboolean
impl_settings_list_connections (NMSettings *settings, GPtrArray **connections, GError **error)
{
	g_return_val_if_fail (NM_IS_SETTINGS (settings), FALSE);

	if (!SETTINGS_CLASS (settings)->list_connections) {
		*error = nm_settings_new_error ("%s.%d - Missing implementation for "
		                                "Settings::list_connections.",
		                                __FILE__, __LINE__);
		return FALSE;
	}

	*connections = SETTINGS_CLASS (settings)->list_connections (settings);

	return TRUE;
}

static void
nm_settings_init (NMSettings *settings)
{
}

static void
nm_settings_finalize (GObject *object)
{
	G_OBJECT_CLASS (nm_settings_parent_class)->finalize (object);
}

static void
nm_settings_class_init (NMSettingsClass *settings_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (settings_class);

	/* virtual methods */
	object_class->finalize = nm_settings_finalize;

	settings_class->list_connections = NULL;

	/* signals */
	settings_signals[S_NEW_CONNECTION] =
		g_signal_new ("new-connection",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (NMSettingsClass, new_connection),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1,
			      G_TYPE_OBJECT);

	dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (settings_class),
					 &dbus_glib_nm_settings_object_info);
}

void
nm_settings_signal_new_connection (NMSettings *settings, NMConnectionSettings *connection)
{
	g_return_if_fail (NM_IS_SETTINGS (settings));
	g_return_if_fail (NM_IS_CONNECTION_SETTINGS (connection));

	g_signal_emit (settings, settings_signals[S_NEW_CONNECTION], 0, connection);
}

/*
 * NMConnectionSettings implementation
 */

static gboolean impl_connection_settings_get_id (NMConnectionSettings *connection,
						 gchar **id,
						 GError **error);
static gboolean impl_connection_settings_get_settings (NMConnectionSettings *connection,
						       GHashTable **settings,
						       GError **error);
static void impl_connection_settings_get_secrets (NMConnectionSettings *connection,
						      const gchar *setting_name,
						      gboolean request_new,
						      DBusGMethodInvocation *context);

#include "nm-settings-connection-glue.h"

#define CONNECTION_SETTINGS_CLASS(o) (NM_CONNECTION_SETTINGS_CLASS (G_OBJECT_GET_CLASS (o)))

G_DEFINE_TYPE (NMConnectionSettings, nm_connection_settings, G_TYPE_OBJECT)

enum {
	CS_UPDATED,
	CS_REMOVED,

	CS_LAST_SIGNAL
};

static guint connection_signals[CS_LAST_SIGNAL] = { 0 };

static gboolean
impl_connection_settings_get_id (NMConnectionSettings *connection,
				 gchar **id,
				 GError **error)
{
	g_return_val_if_fail (NM_IS_CONNECTION_SETTINGS (connection), FALSE);

	if (!CONNECTION_SETTINGS_CLASS (connection)->get_id) {
		*error = nm_settings_new_error ("%s.%d - Missing implementation for "
		                                "ConnectionSettings::get_id.",
		                                __FILE__, __LINE__);
		return FALSE;
	}

	*id = CONNECTION_SETTINGS_CLASS (connection)->get_id (connection);

	return TRUE;
}

static gboolean
impl_connection_settings_get_settings (NMConnectionSettings *connection,
				       GHashTable **settings,
				       GError **error)
{
	g_return_val_if_fail (NM_IS_CONNECTION_SETTINGS (connection), FALSE);

	if (!CONNECTION_SETTINGS_CLASS (connection)->get_settings) {
		*error = nm_settings_new_error ("%s.%d - Missing implementation for "
		                                "ConnectionSettings::get_settings.",
		                                __FILE__, __LINE__);
		return FALSE;
	}

	*settings = CONNECTION_SETTINGS_CLASS (connection)->get_settings (connection);

	return TRUE;
}

static void
impl_connection_settings_get_secrets (NMConnectionSettings *connection,
                                      const gchar *setting_name,
                                      gboolean request_new,
                                      DBusGMethodInvocation *context)
{
	GError *error = NULL;

	if (!NM_IS_CONNECTION_SETTINGS (connection)) {
		error = nm_settings_new_error ("%s.%d - Invalid connection in "
		                               "ConnectionSettings::get_secrets.",
		                               __FILE__, __LINE__);
		dbus_g_method_return_error (context, error);
		g_error_free (error);
		return;
	}

	if (!CONNECTION_SETTINGS_CLASS (connection)->get_secrets) {
		error = nm_settings_new_error ("%s.%d - Missing implementation for "
		                               "ConnectionSettings::get_secrets.",
		                               __FILE__, __LINE__);
		dbus_g_method_return_error (context, error);
		g_error_free (error);
		return;
	}

	CONNECTION_SETTINGS_CLASS (connection)->get_secrets (connection, setting_name, request_new, context);
}

static void
nm_connection_settings_init (NMConnectionSettings *connection)
{
	static guint32 cs_counter = 0;

	connection->dbus_path = g_strdup_printf ("%s/%u",
		                                     NM_DBUS_PATH_SETTINGS,
		                                     cs_counter++);
}

static void
nm_connection_settings_dispose (GObject *object)
{
	NMConnectionSettings * self = NM_CONNECTION_SETTINGS (object);

	if (self->dbus_path) {
		g_free (self->dbus_path);
		self->dbus_path = NULL;
	}

	G_OBJECT_CLASS (nm_connection_settings_parent_class)->dispose (object);
}

static void
nm_connection_settings_finalize (GObject *object)
{
	G_OBJECT_CLASS (nm_connection_settings_parent_class)->finalize (object);
}

#define DBUS_TYPE_G_STRING_VARIANT_HASHTABLE (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))
#define DBUS_TYPE_G_DICT_OF_DICTS (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, DBUS_TYPE_G_STRING_VARIANT_HASHTABLE))

static void
nm_connection_settings_class_init (NMConnectionSettingsClass *connection_settings_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (connection_settings_class);

	/* virtual methods */
	object_class->finalize = nm_connection_settings_finalize;
	object_class->dispose = nm_connection_settings_dispose;

	connection_settings_class->get_id = NULL;
	connection_settings_class->get_settings = NULL;
	connection_settings_class->get_secrets = NULL;

	/* signals */
	connection_signals[CS_UPDATED] =
		g_signal_new ("updated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (NMConnectionSettingsClass, updated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      DBUS_TYPE_G_DICT_OF_DICTS);
	connection_signals[CS_REMOVED] =
		g_signal_new ("removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (NMConnectionSettingsClass, removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (connection_settings_class),
					 &dbus_glib_nm_connection_settings_object_info);
}

void
nm_connection_settings_register_object (NMConnectionSettings *connection,
                                        DBusGConnection *dbus_connection)
{
	g_return_if_fail (NM_IS_CONNECTION_SETTINGS (connection));
	g_return_if_fail (dbus_connection != NULL);

	dbus_g_connection_register_g_object (dbus_connection,
	                                     connection->dbus_path,
	                                     G_OBJECT (connection));
}

const char *
nm_connection_settings_get_dbus_object_path (NMConnectionSettings *connection)
{
	g_return_val_if_fail (NM_IS_CONNECTION_SETTINGS (connection), NULL);

	return connection->dbus_path;
} 

void
nm_connection_settings_signal_updated (NMConnectionSettings *connection, GHashTable *settings)
{
	g_return_if_fail (NM_IS_CONNECTION_SETTINGS (connection));

	g_signal_emit (connection, connection_signals[CS_UPDATED], 0, settings);
}

void
nm_connection_settings_signal_removed (NMConnectionSettings *connection)
{
	g_return_if_fail (NM_IS_CONNECTION_SETTINGS (connection));

	g_signal_emit (connection, connection_signals[CS_REMOVED], 0);
}
