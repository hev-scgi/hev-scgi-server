/* hev-main.c
 * Heiher <admin@heiher.info>
 */

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <gmodule.h>

#include "hev-main.h"
#include "hev-scgi-server.h"

#ifdef G_OS_UNIX
#include <pwd.h>
#include <grp.h>
#include <glib-unix.h>
#endif /* G_OS_UNIX */

#ifdef G_OS_UNIX
static gboolean unix_signal_handler(gpointer user_data)
{
	GMainLoop *main_loop = user_data;

	g_main_loop_quit(main_loop);

	return FALSE;
}
#endif /* G_OS_UNIX */

static void debug_log_handler(const gchar *log_domain,
			GLogLevelFlags log_level,
			const gchar *message,
			gpointer user_data)
{
}

int main(int argc, char *argv[])
{
	GMainLoop *main_loop = NULL;
	GObject *scgi_server = NULL;
	static gchar *conf_dir = NULL;
	static gboolean debug = FALSE;
#ifdef G_OS_UNIX
	static gchar *user = NULL;
	static gchar *group = NULL;
#endif /* G_OS_UNIX */
	static GOptionEntry option_entries[] =
	{
		{ "conf", 'c', 0, G_OPTION_ARG_STRING,  &conf_dir, "Config directory", NULL},
#ifdef G_OS_UNIX
		{ "user", 'u', 0, G_OPTION_ARG_STRING,  &user, "User name", NULL},
		{ "group", 'g', 0, G_OPTION_ARG_STRING,  &group, "Group name", NULL},
#endif /* G_OS_UNIX */
		{ "debug", 'd', 0, G_OPTION_ARG_NONE,  &debug, "Debug mode", NULL},
		{ NULL }
	};
	GOptionContext *option_context = NULL;
	GError *error = NULL;

#ifndef GLIB_VERSION_2_36
	g_type_init();
#endif

	if(!g_module_supported())
	  g_error("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	option_context = g_option_context_new("");
	g_option_context_add_main_entries(option_context,
				option_entries, NULL);
	if(!g_option_context_parse(option_context, &argc, &argv, &error))
	{
		g_error("%s:%d[%s]=>(%s)", __FILE__, __LINE__, __FUNCTION__,
					error->message);
		g_error_free(error);
	}
	g_option_context_free(option_context);

#ifdef G_OS_UNIX
	if(group)
	{
		struct group *grp = NULL;

		if(NULL == (grp = getgrnam(group)))
		  g_error("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					  __FUNCTION__, "Get group failed!");

		if(-1 == setgid(grp->gr_gid))
		  g_error("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					  __FUNCTION__, "Set gid failed!");
		if(-1 == setgroups(0, NULL))
		  g_error("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					  __FUNCTION__, "Set groups failed!");
		if(user)
		{
			if(-1 == initgroups(user, grp->gr_gid))
			  g_error("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
						  __FUNCTION__, "Init groups failed!");
		}
	}

	if(user)
	{
		struct passwd *pwd = NULL;

		if(NULL == (pwd = getpwnam(user)))
		  g_error("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					  __FUNCTION__, "Get user failed!");

		if(-1 == setuid(pwd->pw_uid))
		  g_error("%s:%d[%s]=>(%s)", __FILE__, __LINE__,
					  __FUNCTION__, "Set uid failed!");
	}
#endif /* G_OS_UNIX */

	if(!debug)
	{
		g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG,
					debug_log_handler, NULL);
#ifdef G_OS_UNIX
		daemon(1, 1);
#endif /* G_OS_UNIX */
	}

	main_loop = g_main_loop_new(NULL, FALSE);
	if(!main_loop)
	  g_error("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	scgi_server = hev_scgi_server_new(conf_dir);
	if(!scgi_server)
	  g_error("%s:%d[%s]", __FILE__, __LINE__, __FUNCTION__);

	hev_scgi_server_load_extern_handlers(HEV_SCGI_SERVER(scgi_server));
	hev_scgi_server_load_default_handler(HEV_SCGI_SERVER(scgi_server));

	hev_scgi_server_start(HEV_SCGI_SERVER(scgi_server));
#ifdef G_OS_UNIX
	g_unix_signal_add(SIGINT, unix_signal_handler, main_loop);
	g_unix_signal_add(SIGTERM, unix_signal_handler, main_loop);
#endif /* G_OS_UNIX */

	g_main_loop_run(main_loop);

	hev_scgi_server_stop(HEV_SCGI_SERVER(scgi_server));

	g_object_unref(G_OBJECT(scgi_server));
	g_main_loop_unref(main_loop);

	return 0;
}

