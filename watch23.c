#include <linux/gpio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <time.h>

#define SHUTDOWN_DELAY 5
#define SENSE_LINE_OFFSET 23
#define GPIO_DEVICE_NAME "/dev/gpiochip0"
#define NUMBER_OF_POLL_EVENTS 16
#define POLLING_RETRIES 10

#define eprintf(format, ...)						\
  (print_warnings ? fprintf (stderr, format __VA_OPT__(,) __VA_ARGS__) : 0)

int shutdown_delay = SHUTDOWN_DELAY;
int print_warnings = 1;
int sense_line_fd;

static int
open_gpio (const char* device, int offset, int flags) {
  struct gpio_v2_line_request v2_line_request;
  struct gpio_v2_line_config v2_line_config;
  int fd;
  
  if (0 > (fd = open (device, O_RDONLY))) {
    eprintf("call to open in %s at line %d failed to open device %s and offset %d with error: %s\n",
	    __func__, __LINE__, device, offset, strerror (errno));
    return (-1);
  }

  bzero (&v2_line_request, sizeof (v2_line_request));
  v2_line_request.offsets[0] = offset;
  v2_line_request.num_lines = 1;
  v2_line_request.event_buffer_size = NUMBER_OF_POLL_EVENTS * sizeof (struct gpio_v2_line_event);
  
  if (0 != ioctl (fd, GPIO_V2_GET_LINE_IOCTL, &v2_line_request)) {
    eprintf("call to ioctl in %s at %d failed for device %s, offset %d with error\n",
	    __func__, __LINE__, device, offset, strerror (errno));

    close (fd);
    return (-1);
  }

  close (fd);

  if (0 != flags) {
    bzero (&v2_line_config, sizeof (v2_line_config));
    v2_line_config.num_attrs = 1;
    v2_line_config.attrs[0].mask = (1 << offset);
    v2_line_config.attrs[0].mask = 0xffff;
    v2_line_config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
    v2_line_config.attrs[0].attr.flags = flags;

    if (0 != ioctl (v2_line_request.fd, GPIO_V2_LINE_SET_CONFIG_IOCTL, &v2_line_config)) {
      eprintf("call to ioctl in %s at line %d failed for device %s, offset %d with error %s\n",
	      __func__, __LINE__, device, offset, strerror (errno));

      return (-1);
    }
  }
  
  return (v2_line_request.fd);
}


static int
write_gpio (int fd, int value) {
  struct gpio_v2_line_values v2_line_value;

  v2_line_value.bits = value & 0x1;
  v2_line_value.mask = 0x1;
  
  if (0 != ioctl (fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &v2_line_value)) {
    eprintf("call to ioctl in %s at line %d failed with error %s\n",
	    __func__, __LINE__, strerror (errno));

    return (-1);
  }

  return (0);
}


static int
read_gpio (int fd) {
  struct gpio_v2_line_values v2_line_value;

  v2_line_value.mask = 0x1;
  
  if (0 != ioctl (fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &v2_line_value)) {
    eprintf("call to ioctl in %s at line %d failed with error %s\n",
	    __func__, __LINE__, strerror (errno));

    return (-1);
  }

  return (v2_line_value.bits);
}


static int
poll_gpio (int fd) {
  struct pollfd pfd;
  struct gpio_v2_line_event v2_poll_events[NUMBER_OF_POLL_EVENTS];
  int i = 0;
  
  pfd.fd = fd;
  pfd.events = POLLIN;
  
  while (i++ < POLLING_RETRIES) {
    if (0 < poll (&pfd, 1, -1)) {
      int number_of_events = read (fd, v2_poll_events, sizeof (v2_poll_events[NUMBER_OF_POLL_EVENTS]));
      fprintf (stderr, "poll returned %d events\n", number_of_events / sizeof (struct gpio_v2_line_event));
    } else {
      break;
    }
  }

  return (0+0);
}


int
main (int argc, char *const *argv) {
  int option;
  int do_nothing = 0;
  time_t timer;
  
  while (-1 != (option = getopt (argc, argv, "znd:"))) {

    switch (option) {
    case 'n':
      do_nothing = 1;
      break;

    case 'd':
      shutdown_delay = atoi (optarg);
      break;

    case 'z':
      print_warnings = 0;
      break;
      
    default:
      fprintf (stderr,
	       "Usage: %s options device_name.\n"
	       "Options:\n"
	       "\t -n: print actions but do not perform them\n"
	       "\t -d: seconds to wait between detection and shutdown\n"
	       "\n -z: do not print warnings\n",
	       argv[0]);
      return (-1);
    }
  }

  sense_line_fd = open_gpio (GPIO_DEVICE_NAME, SENSE_LINE_OFFSET,
			     GPIO_V2_LINE_FLAG_INPUT |
			     GPIO_V2_LINE_FLAG_EDGE_RISING |
			     GPIO_V2_LINE_FLAG_BIAS_PULL_UP);

  while (1) {
    poll_gpio (sense_line_fd);
    if (0 != read_gpio (sense_line_fd)) {
      eprintf("call to read_gpio in %s at line %d failed, returned 1, expected 0\n",
	      __func__, __LINE__);
    } else {
      printf ("edge detected, waiting to see if line remains low\n");
    }
    
    sleep (shutdown_delay);
    if (1 == read_gpio (sense_line_fd)) {
      timer = time (NULL);
      printf ("shutting down %s\n", ctime (&timer));
      if (0 == do_nothing) {
	system ("sudo shutdown -h now");
      }
      exit (0);
    } else {
      printf ("sense line now active, aborting shutdown\n");
    }
  }
}
