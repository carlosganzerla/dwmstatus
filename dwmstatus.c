/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>
#include <X11/Xlib.h>
// #include <alsa/asoundlib.h>
// #include <alsa/control.h>

int
parse_netdev(unsigned long long int *receivedabs, unsigned long long int *sentabs)
{
	char buf[255];
	char *datastart;
	static int bufsize;
	int rval;
	FILE *devfd;
	unsigned long long int receivedacc, sentacc;

	bufsize = 255;
	devfd = fopen("/proc/net/dev", "r");
	rval = 1;

	// Ignore the first two lines of the file
	fgets(buf, bufsize, devfd);
	fgets(buf, bufsize, devfd);

	while (fgets(buf, bufsize, devfd)) {
	    if ((datastart = strstr(buf, "lo:")) == NULL) {
		datastart = strstr(buf, ":");

		// With thanks to the conky project at http://conky.sourceforge.net/
		sscanf(datastart + 1, "%llu  %*d     %*d  %*d  %*d  %*d   %*d        %*d       %llu",\
		       &receivedacc, &sentacc);
		*receivedabs += receivedacc;
		*sentabs += sentacc;
		rval = 0;
	    }
	}

	fclose(devfd);
	return rval;
}

void
calculate_speed(char *speedstr, unsigned long long int newval, unsigned long long int oldval)
{
	double speed;
	speed = (newval - oldval) / 1024.0;
	if (speed > 1024.0) {
	    speed /= 1024.0;
	    sprintf(speedstr, "%.3f MB/s", speed);
	} else {
	    sprintf(speedstr, "%.2f KB/s", speed);
	}
}

char *
get_netusage(unsigned long long int *rec, unsigned long long int *sent)
{
	unsigned long long int newrec, newsent;
	newrec = newsent = 0;
	char downspeedstr[15], upspeedstr[15];
	static char retstr[42];
	int retval;

	retval = parse_netdev(&newrec, &newsent);
	if (retval) {
	    fprintf(stdout, "Error when parsing /proc/net/dev file.\n");
	    exit(1);
	}

	calculate_speed(downspeedstr, newrec, *rec);
	calculate_speed(upspeedstr, newsent, *sent);

	sprintf(retstr, "down: %s up: %s", downspeedstr, upspeedstr);

	*rec = newrec;
	*sent = newsent;
	return retstr;
}

// int
// get_vol(void)
// {
//     int vol;
//     snd_hctl_t *hctl;
//     snd_ctl_elem_id_t *id;
//     snd_ctl_elem_value_t *control;
// 
// // To find card and subdevice: /proc/asound/, aplay -L, amixer controls
//     snd_hctl_open(&hctl, "hw:0", 0);
//     snd_hctl_load(hctl);
// 
//     snd_ctl_elem_id_alloca(&id);
//     snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
// 
// // amixer controls
//     snd_ctl_elem_id_set_name(id, "Master Playback Volume");
// 
//     snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);
// 
//     snd_ctl_elem_value_alloca(&control);
//     snd_ctl_elem_value_set_id(control, id);
// 
//     snd_hctl_elem_read(elem, control);
//     vol = (int)snd_ctl_elem_value_get_integer(control,0);
// 
//     snd_hctl_close(hctl);
//     return vol;
// }

char *tzbr = "America/Sao_Paulo";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
		return smprintf("");

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
gettemperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f??C", atof(co) / 1000);
}


char *
up() {
    struct sysinfo info;
    int h,m = 0;
    sysinfo(&info);
    h = info.uptime/3600;
    m = (info.uptime - h*3600 )/60;
    return smprintf("%dh%dm",h,m);
}

int
main(void)
{
	char *status;
	char *avgs;
	char *uptime;
	char *netstats;
	char *tbr;
	static unsigned long long int rec, sent;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	parse_netdev(&rec, &sent);
	for (;;sleep(1)) {
		avgs = loadavg();
		uptime = up();
		netstats = get_netusage(&rec, &sent);
		tbr = mktimes("%a %d %b %H:%M", tzbr);

		status = smprintf("L: %s | Up: %s | Eth: %s | %s",
				avgs, uptime, netstats, tbr);
		setstatus(status);

		free(avgs);
		free(uptime);
		free(tbr);
	}

	XCloseDisplay(dpy);

	return 0;
}

