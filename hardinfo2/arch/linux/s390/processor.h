/*
 *    HardInfo - Displays System Information
 *    Copyright (C) 2003-2006 Leandro A. F. Pereira <leandro@linuxmag.com.br>
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

static Processor *
computer_get_processor(void)
{
    Processor *processor;
    FILE *cpuinfo;
    gchar buffer[128];

    cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo)
	return NULL;

    processor = g_new0(Processor, 1);
    while (fgets(buffer, 128, cpuinfo)) {
	gchar **tmp = g_strsplit(buffer, ":", 2);

	if (tmp[0] && tmp[1]) {
	    tmp[0] = g_strstrip(tmp[0]);
	    tmp[1] = g_strstrip(tmp[1]);

	    get_str("vendor_id", processor->vendor_id);
	    get_float("# processors", processor->processors);
	    get_int("bogomips per cpu", processor->bogomips);

	}
	g_strfreev(tmp);
    }
    
    gchar *tmp = g_strconcat("S390 ", processor->vendor_id, NULL);
    processor->model_name = tmp;

    fclose(cpuinfo);

    return processor;
}

static gchar *
processor_get_info(Processor *processor)
{
	return g_strdup_printf("[Processor]\n"
	                       "Processors=%d\n"
	                       "BogoMips per CPU=%.2f"
	                       "Byte Order=%s\n",
			       processor->model_name,
			       processor->processors,
			       processor->bogomips,
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                               "Little Endian"
#else
                               "Big Endian"
#endif
                              );
}