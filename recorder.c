#include<stdio.h>
#include<sys/ioctl.h>
#include<linux/input.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<limits.h>
#include<signal.h>
#include<stdlib.h>
#include<glob.h>


/* This is the path to the symlink associated with the mouse-event file. 
 * It can also be obtained by calling functions like glob() or fnmatch() 
 * if there are multiple mice devices. */
#define USB_PATH "/dev/input/by-id/usb-*-event-mouse"

typedef enum {false, true} bool;

static bool not_interrupted=true;


void handle_interrupt(int signum);


/*****************************************************
 * This program will try to estimate how much did the
 * moved. It'll take the mouse dpi as (command line)
 * argument, will record the events sent by the mouse
 * and will output the movements in inches.
 *****************************************************/
int main(int argc, char *argv[]) {

	int device_fd;
	char device_name[PATH_MAX]={0};
	struct input_event event;
	struct sigaction sig;
	ssize_t bytes_read;
	int x_axis=0,y_axis=0;
	int events_counter=0;
	glob_t g;
	int ret_code;
	int dpi;
	
	if (argc!=2) {
		fprintf(stderr, "one argument required: mouse dpi\n");
		return 1;
	}

	dpi=atoi(argv[1]);


	/* registering for a SIGINT signal */
	sig.sa_handler=handle_interrupt;
	sig.sa_flags=0;
	sigfillset(&sig.sa_mask);
	if (sigaction(SIGINT, &sig, NULL)) {
		fprintf(stderr, "sigaction() error: %s\n", strerror(errno));
		return 1;
	}


	/* looking for a mouse-event file(s) */ 
	ret_code=glob(USB_PATH, GLOB_ERR, NULL , &g);

	if (ret_code==GLOB_NOMATCH) {
		fprintf(stderr, "No mouse found\n");
		return 1;
	}
	else if (ret_code) {
		fprintf(stderr, "glob() error: %s\n", strerror(errno));
		return 1;
	}


	/* opening device file associated with USB; */
	/* this will pick the first USB mouse-file it encounters, if several are attached */
	do {
		device_fd=open(g.gl_pathv[0], O_RDONLY);

	/* in case of EINTR errno value - keep on trying */
	} while (device_fd==-1 && errno==EINTR);


	if (device_fd==-1) {
		if (errno==ENOENT) {
			fprintf(stderr, "No mouse found\n");
		}
		else {
			fprintf(stderr, "open() error: %s\n", strerror(errno));
		}
		return 1;
	}
	
	/* obtaining device name */
	if (ioctl(device_fd, EVIOCGNAME(sizeof(device_name)-1), device_name)==-1) {
		printf("ioctl() error: %s\n",strerror(errno));
		return 1;
	}


	printf("The device %s mouse was detected...\n", device_name);
	printf("Listening to relative movements, start moving the mouse around...\n");
	printf("Press Ctrl+C to stop and exit\n");

	fflush(stdout);

	while (not_interrupted) {

		/* reading the next event; the events will be sent from kernel as frames of fixed size */
		bytes_read=read(device_fd, &event, sizeof(event));

		/* in case of an error */
		if (bytes_read==-1) {
			/* if read() was interrupted */
			if (errno==EINTR) {
				continue;
			}

			fprintf(stderr, "read() error: %s\n", strerror(errno));
			return 1;
		}

		/* eof was reached - probably device was detached */
		else if (bytes_read==0) {
			printf("No more events to read.\n");
			break;
		}

		/* this should never happen, if it did - it's a driver/kernel bug */
		else if (bytes_read!=sizeof(event)) {
			fprintf(stderr, "invalid event; length - %lu, expected - %lu", bytes_read, sizeof(event));
			/* ignoring the event and waiting for the next */
			// TODO: consider terminating program here if necessary.
			continue;
		}

		/* 'event' struct contains an event holding relative move,
		 * we can process it however we like.
		 * For now, it will just sum the total absolute movement
		 * along the X and Y axis...
		 * There are a lot of other events that can be captured, but
		 * are ignored by this program for now, as it is just a basic
		 * example, like 'click buttons' and 'wheel rolling'. */

		/* this type of event used as a seperator between events */
		if (event.type==EV_SYN) {
			++events_counter;
		}

		// this type of event used to describe relative axis value changes
		if (event.type==EV_REL) {
			if (event.code==REL_X) {
				x_axis+=abs(event.value);
			}
			else if (event.code==REL_Y) {
				y_axis+=abs(event.value);
			}
			/* other codes are available and will be ignored! */
		}
		else {
			/* other types are available and will be ignored! */
		}

		fflush(stdout);
	}

	printf("\n\n%d events captured.\n", events_counter);
	printf("Total movement:\n");
	printf("On x axis: dots (\"pixels\"): %6d \tinches: %8.2f \tcentimeters: %8.2f\n",
				x_axis, (float)x_axis/dpi, ((float)x_axis/dpi)*2.54);
	printf("On y axis: dots (\"pixels\"): %6d \tinches: %8.2f \tcentimeters: %8.2f\n",
				y_axis, (float)y_axis/dpi, ((float)y_axis/dpi)*2.54);
	return 0;
}



void handle_interrupt(int signum) {
	not_interrupted=false;
}






