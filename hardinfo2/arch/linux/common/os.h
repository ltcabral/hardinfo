/*
 *    HardInfo - Displays System Information
 *    Copyright (C) 2003-2006 Leandro A. F. Pereira <leandro@hardinfo.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, version 2.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

static gchar *
get_default_gcc_version(void)
{
    char *buf;
    
    if (g_spawn_command_line_sync("gcc -v",
                                   NULL,
                                   &buf,
                                   NULL,
                                   NULL)) {
        char *return_value;
        
        if (!(return_value = strstr(buf, "gcc "))) {
            goto err;
        }
        
	return_value = strstr(return_value, " ") + 1;
	return_value = strstr(return_value, " ") + 1;

        return_value = g_strdup_printf("GNU C Compiler version %s", return_value);
        
        g_free(buf);
        
        return return_value;
    }

err:
    return g_strdup("Unknown");
}

static gchar *
get_libc_version(void)
{
    FILE *libc;
    gchar buf[256], *tmp, *p;
    
    libc = popen("/lib/libc.so.6", "r");
    if (!libc) goto err;
    
    (void)fgets(buf, 256, libc);
    if (pclose(libc)) goto err;
    
    tmp = strstr(buf, "version ");
    if (!tmp) goto err;
    
    p = strchr(tmp, ',');
    if (p) *p = '\0';
    else goto err;
    
    return g_strdup_printf("GNU C Library version %s (%sstable)",
                           strchr(tmp, ' ') + 1,
                           strstr(buf, " stable ") ? "" : "un");
  err:
    return g_strdup("Unknown");
}

static gchar *
get_os_compiled_date(void)
{
    FILE *procversion;
    gchar buf[512];

    procversion = fopen("/proc/sys/kernel/version", "r");
    if (!procversion)
	return g_strdup("Unknown");

    (void)fgets(buf, 512, procversion);
    fclose(procversion);

    return g_strdup(buf);
}


#include <gdk/gdkx.h>

void
detect_desktop_environment(OperatingSystem * os)
{
    const gchar *tmp = g_getenv("GNOME_DESKTOP_SESSION_ID");
    FILE *version;
    char vers[16];

    if (tmp) {
	/* FIXME: this might not be true, as the gnome-panel in path
	   may not be the one that's running.
	   see where the user's running panel is and run *that* to
	   obtain the version. */
	version = popen("gnome-about --gnome-version", "r");
	if (version) {
	    (void)fscanf(version, "Version: %s", vers);
	    if (pclose(version))
	        goto unknown;
	} else {
	    goto unknown;
	}

	os->desktop = g_strdup_printf("GNOME %s", vers);
    } else if (g_getenv("KDE_FULL_SESSION")) {

	if (g_getenv("KDE_SESSION_VERSION") && strstr(g_getenv("KDE_SESSION_VERSION"),(gchar *)"4")) {
	    version = popen("kwin --version", "r");
	} else {
	    version = popen("kcontrol --version", "r");
	}

	if (version) {
	    char buf[32];

	    (void)fgets(buf, 32, version);

	    (void)fscanf(version, "KDE: %s", vers);
	    if (pclose(version))
	        goto unknown;
	} else {
	    goto unknown;
	}

	os->desktop = g_strdup_printf("KDE %s", vers);
    } else {
      unknown:
	if (!g_getenv("DISPLAY")) {
	    os->desktop = g_strdup("Terminal");
	} else {
            GdkScreen *screen = gdk_screen_get_default();
            
            if (screen && GDK_IS_SCREEN(screen)) {
              const gchar *windowman;

              windowman = gdk_x11_screen_get_window_manager_name(screen);
              
              if (g_str_equal(windowman, "Xfwm4")) {
                  /* FIXME: check if xprop -root | grep XFCE_DESKTOP_WINDOW
                     is defined */
                  os->desktop = g_strdup("XFCE 4");
              } else {
                  os->desktop = g_strdup_printf("Unknown (Window Manager: %s)",
                                                windowman);
              }
            } else {
              os->desktop = g_strdup("Unknown");
            }
	}
    }
}

static OperatingSystem *
computer_get_os(void)
{
    struct utsname utsbuf;
    OperatingSystem *os;
    int i;

    os = g_new0(OperatingSystem, 1);

    os->compiled_date = get_os_compiled_date();

    /* Attempt to get the Distribution name; try using /etc/lsb-release first,
       then doing the legacy method (checking for /etc/$DISTRO-release files) */
    if (g_file_test("/etc/lsb-release", G_FILE_TEST_EXISTS)) {
	FILE *release;
	gchar buffer[128];

	release = popen("lsb_release -d", "r");
	if (release) {
            (void)fgets(buffer, 128, release);
            pclose(release);

            os->distro = buffer;
            os->distro = g_strdup(os->distro + strlen("Description:\t"));
        }
    }

    for (i = 0;; i++) {
	if (distro_db[i].file == NULL) {
	    os->distrocode = g_strdup("unk");
	    os->distro = g_strdup("Unknown distribution");
	    break;
	}

	if (g_file_test(distro_db[i].file, G_FILE_TEST_EXISTS)) {
	    FILE *distro_ver;
	    char buf[128];

	    distro_ver = fopen(distro_db[i].file, "r");
	    (void)fgets(buf, 128, distro_ver);
	    fclose(distro_ver);

	    buf[strlen(buf) - 1] = 0;

	    if (!os->distro) {
		/*
		 * HACK: Some Debian systems doesn't include
		 * the distribuition name in /etc/debian_release,
		 * so add them here. 
		 */
		if (!strncmp(distro_db[i].codename, "deb", 3) &&
		    ((buf[0] >= '0' && buf[0] <= '9') || buf[0] != 'D')) {
		    os->distro = g_strdup_printf
			("Debian GNU/Linux %s", buf);
		} else {
		    os->distro = g_strdup(buf);
		}
	    }
	    
	    if (g_str_equal(distro_db[i].codename, "ppy")) {
	      gchar *tmp;
	      
	      tmp = g_strdup_printf("Puppy Linux %.2f", atof(os->distro) / 100.0);
	      g_free(os->distro);
	      os->distro = tmp;
	    }
	    
	    os->distrocode = g_strdup(distro_db[i].codename);

	    break;
	}
    }

    /* Kernel and hostname info */
    uname(&utsbuf);
    os->kernel = g_strdup_printf("%s %s (%s)", utsbuf.sysname,
				 utsbuf.release, utsbuf.machine);
    os->hostname = g_strdup(utsbuf.nodename);
    os->language = g_strdup(g_getenv("LC_MESSAGES"));
    os->homedir = g_strdup(g_get_home_dir());
    os->username = g_strdup_printf("%s (%s)",
				   g_get_user_name(), g_get_real_name());
    os->libc = get_libc_version();
    os->gcc  = get_default_gcc_version();
    scan_languages(os);
    detect_desktop_environment(os);

    return os;
}
