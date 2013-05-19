
#include "helper.h"
#include "defines.h"

#include <sys/syscall.h>
#include <sys/time.h>
#include <stdio.h>

// returns current time in microseconds
unsigned long long getTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (unsigned long long)tv.tv_sec * 1000000ull + tv.tv_usec;
}

using namespace boost::accumulators;

t_statslog* statslog_create(const char* name) {
	t_statslog* statslog = new t_statslog;
	strcpy(statslog->name, name);
	statslog->fps_time = getTime();
	statslog->frame_time = 0;
	statslog->fps_frames = 0;
	return statslog;
}

void statslog_destroy(t_statslog*& statslog) {
	delete statslog;
	statslog = NULL;
}

void statslog_frame_start(t_statslog* statslog) {
	statslog->frame_start_time = getTime();
}

void statslog_frame_stop(t_statslog* statslog) {
	++statslog->fps_frames;
	const unsigned long long now_time = getTime();
	const unsigned long long delta_time = now_time - statslog->fps_time;
	statslog->frame_time += now_time - statslog->frame_start_time;
	if (delta_time >= 1000000)
	{
		const float fps = (1000000.0/delta_time)*statslog->fps_frames;
		const float frame_time = ((double)statslog->frame_time/statslog->fps_frames)/1000.0;
		printf("[%s@%x] %.2f fps (%.1f ms)\n", statslog->name, (pid_t)syscall(SYS_gettid), fps, frame_time);
		fflush(stdout);
		statslog->fps_time = now_time;
		statslog->frame_time = 0;
		statslog->fps_frames = 0;
		statslog->acc(frame_time);
	}
}

unsigned long long statslog_get_frame_start_time(t_statslog* statslog) {
	return statslog->frame_start_time;
}

float statslog_get_frame_time_max(t_statslog* statslog) {
	return max(statslog->acc);
}
float statslog_get_frame_time_min(t_statslog* statslog) {
	return min(statslog->acc);
}
float statslog_get_frame_time_mean(t_statslog* statslog) {
	return mean(statslog->acc);
}
float statslog_get_frame_time_variance(t_statslog* statslog) {
	return variance(statslog->acc);
}
float statslog_get_frame_time_last5_mean(t_statslog* statslog) {
	return rolling_mean(statslog->acc);
}

void statslog_print_frame_time(t_statslog* statslog, char* buffer) {
	sprintf(buffer, "[%s@%x] frame time (ms): %.1f mean %.1f max %.1f min %.1f variance %.1f last5mean", statslog->name, (pid_t)syscall(SYS_gettid),
			statslog_get_frame_time_mean(statslog),
			statslog_get_frame_time_max(statslog),
			statslog_get_frame_time_min(statslog),
			statslog_get_frame_time_variance(statslog),
			statslog_get_frame_time_last5_mean(statslog)
	);
}

