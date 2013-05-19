#ifndef HELPER_H
#define HELPER_H

/// \todo make a class out of this

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>

// returns current time in microseconds
unsigned long long getTime();

typedef struct t_statslog_type {
	char     name[32];
	unsigned long long frame_start_time;
	unsigned long long frame_time;
	unsigned long long fps_time;
	unsigned int fps_frames;
	boost::accumulators::accumulator_set<float,
		boost::accumulators::stats<
			boost::accumulators::tag::max,
			boost::accumulators::tag::min,
			boost::accumulators::tag::mean,
			boost::accumulators::tag::variance(boost::accumulators::lazy),
			boost::accumulators::tag::rolling_mean
		>
	> acc;
	t_statslog_type() : acc(boost::accumulators::tag::rolling_window::window_size = 5) {}
} t_statslog;

t_statslog* statslog_create(const char* name);
void statslog_destroy(t_statslog*&);
void statslog_frame_start(t_statslog*);
void statslog_frame_stop(t_statslog*);
unsigned long long statslog_get_frame_start_time(t_statslog*);
float statslog_get_frame_time_max(t_statslog*);
float statslog_get_frame_time_min(t_statslog*);
float statslog_get_frame_time_mean(t_statslog*);
float statslog_get_frame_time_variance(t_statslog*);
float statslog_get_frame_time_last5_mean(t_statslog*);
void statslog_print_frame_time(t_statslog*, char*);

#endif
