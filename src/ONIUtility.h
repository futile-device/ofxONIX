//
//  ONIContext.h
//
//  Created by Matt Gingold on 12.09.2024.
//

#include "oni.h"
#include "onix.h"

#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <syncstream>

//#define LOG_OF 1

#ifdef ONI_LOG_FU
#include "ofxFutilities.h"
#define LOGDEBUG(fmt, ...) fu::debugf(fmt, __VA_ARGS__);
#define LOGINFO(fmt, ...)  fu::infof(fmt, __VA_ARGS__);
#define LOGALERT(fmt, ...) fu::alertf(fmt, __VA_ARGS__);
#define LOGERROR(fmt, ...) fu::errorf(fmt, __VA_ARGS__);
#define LOGFATAL(fmt, ...) fu::fatalf(fmt, __VA_ARGS__);
#elif ONI_LOG_OF
#include "ofMain.h"
#define LOGDEBUG(fmt, ...) ofLog(OF_LOG_VERBOSE, fmt, __VA_ARGS__);
#define LOGINFO(fmt, ...)  ofLog(OF_LOG_NOTICE, fmt, __VA_ARGS__);
#define LOGALERT(fmt, ...) ofLog(OF_LOG_WARNING, fmt, __VA_ARGS__);
#define LOGERROR(fmt, ...) ofLog(OF_LOG_ERROR, fmt, __VA_ARGS__);
#define LOGFATAL(fmt, ...) ofLog(OF_LOG_FATAL_ERROR, fmt, __VA_ARGS__);
#else
#define LOGDEBUG(fmt, ...) printf(fmt, __VA_ARGS__); printf("\n");
#define LOGINFO(fmt, ...)  printf(fmt, __VA_ARGS__); printf("\n");
#define LOGALERT(fmt, ...) printf(fmt, __VA_ARGS__); printf("\n");
#define LOGERROR(fmt, ...) printf(fmt, __VA_ARGS__); printf("\n");
#define LOGFATAL(fmt, ...) printf(fmt, __VA_ARGS__); printf("\n");
#endif

#pragma once

static inline uint64_t uint16_to_bin16(uint16_t u) {
	uint64_t sum = 0;
	uint64_t power = 1;
	while (u) {
		if (u & 1) {
			sum += power;
		}
		power *= 16;
		u /= 2;
	}
	return sum;
}