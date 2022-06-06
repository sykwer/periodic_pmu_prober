#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct read_format {
  uint64_t nr;            /* The number of events */
  uint64_t  time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
  uint64_t time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
  struct {
    uint64_t value;       /* The value of the event */
    uint64_t id;          /* if PERF_FORMAT_ID */
  } values[];
};

static void encode_event_string(const char *event_string, uint32_t *event_type, uint64_t *event_config) {
  pfm_perf_encode_arg_t arg;
 	struct perf_event_attr attr;
 	char *fstr = NULL; // Get event string in [pmu::][event_name][:unit_mask][:modifier|:modifier=val]

 	memset(&arg, 0, sizeof(arg));
 	arg.size = sizeof(arg);
 	arg.attr = &attr;
 	arg.fstr = &fstr;

 	int ret = pfm_get_os_event_encoding(event_string, PFM_PLM0, PFM_OS_PERF_EVENT_EXT, &arg);
 	if (ret != PFM_SUCCESS) {
 		perror("pfm_get_os_event_encoding error");
 		exit(EXIT_FAILURE);
 	}

  *event_type = attr.type;
  *event_config = attr.config;

  // The returned `fstr` value of "l1d_pend_miss.pending" is
 	// skl::L1D_PEND_MISS:PENDING:e=0:i=0:c=0:t=0:intx=0:intxcp=0:u=0:k=1:period=34:freq=34:excl=0:mg=0:mh=1
 	free(fstr);
}

static void setup_perf_event_attr_grouped(struct perf_event_attr *attr, uint32_t event_type, uint64_t event_config) {
  memset(attr, 0, sizeof(*attr));
  attr->type = event_type;
  attr->size = sizeof(*attr);
  attr->config = event_config;
  attr->disabled = 1;
  attr->exclude_kernel = 1;
  attr->exclude_hv = 1;
  attr->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING |
                      PERF_FORMAT_ID | PERF_FORMAT_GROUP;
}

char buffer[1000];

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stdout, "invalid argument\n");
    exit(EXIT_FAILURE);
  }

  pid_t target_pid;
  sscanf(argv[1], "%d", &target_pid);

  unsigned int measurement_time_sec;
  sscanf(argv[2], "%u", &measurement_time_sec);

  // Prepare event tables for the underlining hardware and software platform
  if (pfm_initialize() != PFM_SUCCESS) {
    perror("pfm_initialize error");
    exit(EXIT_FAILURE);
  }

  uint32_t leader_event_type;
  uint64_t leader_event_config;
  encode_event_string("l1d_pend_miss.pending", &leader_event_type, &leader_event_config);
  struct perf_event_attr leader_attr;
  setup_perf_event_attr_grouped(&leader_attr, leader_event_type, leader_event_config);

  // tmp
  target_pid = getpid();
  // tmp

  int leader_fd = syscall(__NR_perf_event_open, &leader_attr, target_pid, -1/*cpu*/ , -1/*group_fd*/ , 0/*flag*/);
  if (leader_fd == -1) {
  	perror("perf_event_open error");
  	exit(EXIT_FAILURE);
  }

  uint64_t leader_id;
  ioctl(leader_fd, PERF_EVENT_IOC_ID, &leader_id);

  ioctl(leader_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(leader_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

  // measurement duratoin
  sleep(measurement_time_sec);
  volatile char hoge[100];
  for (int i = 0; i < 100; i++) hoge[i];

  ioctl(leader_fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);

  uint64_t l1_pending_cycles;
  struct read_format *rf = (struct read_format*) buffer;
  size_t sz = read(leader_fd, buffer, sizeof(buffer));
  if (sz == -1) {
    perror("read error");
    exit(EXIT_FAILURE);
  }
  printf("The size of read data = %ldbyte\n", sz);

  for (int i = 0; i < rf->nr; i++) {
    if (rf->values[i].id == leader_id) l1_pending_cycles = rf->values[i].value;
    else printf("hoge\n");
  }

  printf("l1_pending_cycles=%ld\n", l1_pending_cycles);

  return 0;
}