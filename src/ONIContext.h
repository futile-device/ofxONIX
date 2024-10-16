//
//  ONIContext.h
//
//  Created by Matt Gingold on 12.09.2024.
//

#include "oni.h"
#include "onix.h"

#include "ofMain.h"
#include "ofxFutilities.h"

//#define LOG_OF 1

#ifdef ONI_LOG_FU
#define LOGDEBUG(fmt, ...) fu::debugf(fmt, __VA_ARGS__);
#define LOGINFO(fmt, ...)  fu::infof(fmt, __VA_ARGS__);
#define LOGALERT(fmt, ...) fu::alertf(fmt, __VA_ARGS__);
#define LOGERROR(fmt, ...) fu::errorf(fmt, __VA_ARGS__);
#define LOGFATAL(fmt, ...) fu::fatalf(fmt, __VA_ARGS__);
#elif ONI_LOG_OF
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

#include "ONIDevices.h"

#pragma once

//void print_dev_table(oni_device_t* devices, size_t num_devs){
//
//	// Show device table
//	printf("   +--------------------+-------+-------+-------+-------+---------------------\n");
//	printf("   |        \t\t|  \t|Firm.\t|Read\t|Wrt. \t|     \n");
//	printf("   |Dev. idx\t\t|ID\t|ver. \t|size\t|size \t|Desc.\n");
//	printf("   +--------------------+-------+-------+-------+-------+---------------------\n");
//
//	size_t dev_idx;
//	for (dev_idx = 0; dev_idx < num_devs; dev_idx++) {
//
//		const char* dev_str = onix_device_str(devices[dev_idx].id);
//
//		printf("%02zd |%05u: 0x%02x.0x%02x\t|%d\t|%d\t|%u\t|%u\t|%s\n",
//			dev_idx,
//			devices[dev_idx].idx,
//			(uint8_t)(devices[dev_idx].idx >> 8),
//			(uint8_t)devices[dev_idx].idx,
//			devices[dev_idx].id,
//			devices[dev_idx].version,
//			devices[dev_idx].read_size,
//			devices[dev_idx].write_size,
//			dev_str);
//	}
//
//	printf("   +--------------------+-------+-------+-------+-------+---------------------\n");
//
//}

class ONIContextOption{

public:

	ONIContextOption() : optn(0), name("UNKNOWN") {};
	ONIContextOption(unsigned int optn, std::string name) { this->optn = optn; this->name = name; };

	const std::string& getName() const { return name; };
	const unsigned int& getOption() const { return optn; };
	operator unsigned int&() { return optn; }
	operator unsigned int() const { return optn; }

	friend inline bool operator< (const ONIContextOption& lhs, const ONIContextOption& rhs);
	friend inline bool operator==(const ONIContextOption& lhs, const ONIContextOption& rhs);
	friend inline std::ostream& operator<<(std::ostream& os, const ONIContextOption& obj);

	// copy assignment (copy-and-swap idiom)
	ONIContextOption& ONIContextOption::operator=(ONIContextOption other) noexcept {
		std::swap(optn, other.optn);
		std::swap(name, other.name);
		return *this;
	}

protected:

	unsigned int optn = -1;
	std::string name = "UNKNOWN";

};

inline std::ostream& operator<<(std::ostream& os, const ONIContextOption& obj) {
	os << obj.optn << " " << obj.name;
	return os;
}

inline bool operator< (const ONIContextOption& lhs, const ONIContextOption& rhs) { return lhs.optn < rhs.optn; }
inline bool operator> (const ONIContextOption& lhs, const ONIContextOption& rhs) { return rhs < lhs; }
inline bool operator<=(const ONIContextOption& lhs, const ONIContextOption& rhs) { return !(lhs > rhs); }
inline bool operator>=(const ONIContextOption& lhs, const ONIContextOption& rhs) { return !(lhs < rhs); }

inline bool operator==(const ONIContextOption& lhs, const ONIContextOption& rhs) { return (lhs.optn == rhs.optn && lhs.name == rhs.name); }
inline bool operator!=(const ONIContextOption& lhs, const ONIContextOption& rhs) { return !(lhs == rhs); }

class ONIContext {

public:

	static inline const ONIContextOption DEVICETABLE		= ONIContextOption(ONI_OPT_DEVICETABLE, "DEVICETABLE");
	static inline const ONIContextOption NUMDEVICES			= ONIContextOption(ONI_OPT_NUMDEVICES, "NUMDEVICES");
	static inline const ONIContextOption RUNNING			= ONIContextOption(ONI_OPT_RUNNING, "RUNNING");
	static inline const ONIContextOption RESET				= ONIContextOption(ONI_OPT_RESET, "RESET");
	static inline const ONIContextOption SYSCLKHZ			= ONIContextOption(ONI_OPT_SYSCLKHZ, "SYSCLKHZ");
	static inline const ONIContextOption ACQCLKHZ			= ONIContextOption(ONI_OPT_ACQCLKHZ, "ACQCLKHZ");
	static inline const ONIContextOption RESETACQCOUNTER	= ONIContextOption(ONI_OPT_RESETACQCOUNTER, "RESETACQCOUNTER");
	static inline const ONIContextOption HWADDRESS			= ONIContextOption(ONI_OPT_HWADDRESS, "HWADDRESS");
	static inline const ONIContextOption MAXREADFRAMESIZE	= ONIContextOption(ONI_OPT_MAXREADFRAMESIZE, "MAXREADFRAMESIZE");
	static inline const ONIContextOption MAXWRITEFRAMESIZE	= ONIContextOption(ONI_OPT_MAXWRITEFRAMESIZE, "MAXWRITEFRAMESIZE");
	static inline const ONIContextOption BLOCKREADSIZE		= ONIContextOption(ONI_OPT_BLOCKREADSIZE, "BLOCKREADSIZE");
	static inline const ONIContextOption BLOCKWRITESIZE		= ONIContextOption(ONI_OPT_BLOCKWRITESIZE, "BLOCKWRITESIZE");
	static inline const ONIContextOption CUSTOMBEGIN		= ONIContextOption(ONI_OPT_CUSTOMBEGIN, "CUSTOMBEGIN");


	ONIContext(){};

	~ONIContext(){
		closeContext();
	};

	inline void update(){

		bool bContextRequiresRestart = !bIsContextSetup;

		// check all the devices to see if they require an context restart eg., PORTVOLTAGE
		for(auto device : oniDevices){
			if(device.second->getDeviceRequiresContextRestart()){
				bContextRequiresRestart = true;
				break;
			}
		}

		if(bContextRequiresRestart){
			LOGINFO("Context requires a restart");
			setupContext();
		}
	}

	bool setup(const std::string& driverName = "riffa", const unsigned int& blockReadSize = 2048, const unsigned int& blockWriteSize = 2048){
		this->driverName = driverName;
		this->blockReadSize = blockReadSize;
		this->blockWriteSize = blockWriteSize;
		return setupContext();
	}

	const std::vector<oni_device_t>& getDeviceTypes(const bool& bForceList = false){

		if (!bForceList && deviceTypes.size() > 0) return deviceTypes; // is this just dangerous and stupid?

		LOGDEBUG("Enumerating devices...");

		clearDevices();

		if(!bIsContextSetup){
			LOGERROR("Context and driver not setup!");
			return deviceTypes;
		}

		int rc = ONI_ESUCCESS;

		// Examine device table
		oni_size_t num_devs = getContextOption(NUMDEVICES);

		oni_device_t* devices_t;

		// Get the device table
		size_t devices_sz = sizeof(oni_device_t) * num_devs;
		devices_t = (oni_device_t*)malloc(devices_sz);
		if(devices_t == NULL){
			LOGERROR("No devices found!!");
			free(devices_t);
			return deviceTypes;
		}

		oni_get_opt(ctx, ONI_OPT_DEVICETABLE, devices_t, &devices_sz);

		for(size_t i = 0; i < num_devs; ++i){

			deviceTypes.push_back(devices_t[i]);

			if(devices_t[i].id == ONIDevice::TypeID::FMC){ // 23 FMC Host Device
				LOGDEBUG("Mapping FMC Host Device: %i device idx: %i", devices_t[i].id, devices_t[i].idx);
				FmcDevice* fmc = new FmcDevice;
				fmc->setup(&ctx, devices_t[i], acq_clock_khz);
				oniDevices[devices_t[i].idx] = fmc;
			}

			if(devices_t[i].id == ONIDevice::TypeID::HEARTBEAT){ // 12 HeartBeat Device
				LOGDEBUG("Mapping HeartBeat Device: %i device idx: %i", devices_t[i].id, devices_t[i].idx);
				HeartBeatDevice* hbd = new HeartBeatDevice;
				hbd->setup(&ctx, devices_t[i], acq_clock_khz);
				oniDevices[devices_t[i].idx] = hbd;
			}

			if(devices_t[i].id == ONIDevice::TypeID::RHS2116){ // 31 RHS2116 Device
				LOGDEBUG("Mapping Rhs2116 Device: %i device idx: %i", devices_t[i].id, devices_t[i].idx);
				Rhs2116Device* rhd2116 = new Rhs2116Device;
				rhd2116->setup(&ctx, devices_t[i], acq_clock_khz);
				oniDevices[devices_t[i].idx] = rhd2116;
			}

		}

		free(devices_t);

		LOGINFO("Found %i Devices", deviceTypes.size());

		return deviceTypes;

	}

	ONIDevice* getDevice(const unsigned int& idx){
		auto it = oniDevices.find(idx);
		if(it == oniDevices.end()){
			LOGERROR("ONIDevice doesn't exist with idx: %i", idx);
			return NULL;
		}
		return it->second;
	}

	void printDeviceTable(){

		getDeviceTypes(); // make sure we have the updated list?

		ostringstream os;

		os << "   +--------------------+-------+-------+-------+-------+---------------------" << std::endl;
		os << "   |        \t\t|  \t|Firm.\t|Read\t|Wrt. \t|     " << std::endl;
		os << "   |Dev. idx\t\t|ID\t|ver. \t|size\t|size \t|Desc." << std::endl;
		os << "   +--------------------+-------+-------+-------+-------+---------------------" << std::endl;

		for(size_t i = 0; i < deviceTypes.size(); ++i){
			os << std::setfill('0') << std::setw(2) << i << " |" << deviceTypes[i] << std::endl;
		}

		LOGINFO("ONI Device Table\n%s", os.str().c_str());

	}

	bool startContext(){
		LOGINFO("Starting ONI context");
		setContextOption(RESETACQCOUNTER, 2);
		if(getContextOption(RUNNING) != 1){
			LOGERROR("Could not start hardware"); // necessary?
			return false;
		}
		//startFrameRead();
		return true;
	}

	bool stopContext(){
		LOGINFO("Stopping ONI context");
		//stopFrameRead();
		if(setContextOption(RUNNING, 0) == -1){
			LOGERROR("Could not stop hardware"); // necessary?
			return false;
		}
		return true;
	}

	void closeContext(){

		LOGDEBUG("Closing ONIContext...");

		if(ctx == NULL) return; // nothing to do

		stopFrameRead();
		clearDevices();
		stopContext();
		
		oni_destroy_ctx(ctx);

		LOGINFO("...ONIContext closed");

	}

	oni_size_t getMaxReadSize(){
		assert(maxReadSize > 0);
		return maxReadSize;
	}

	oni_size_t getMaxWriteSize(){
		assert(maxWriteSize > 0);
		return maxWriteSize;
	}

	oni_size_t getAcquireClockKHZ(){
		assert(acq_clock_khz > 0);
		return acq_clock_khz;
	}

	oni_size_t getSysClockKHZ(){
		assert(sys_clock_khz > 0);
		return sys_clock_khz;
	}

	oni_size_t setContextOption(const ONIContextOption& option, const oni_size_t& val){
		int rc = ONI_ESUCCESS;
		oni_size_t val_result;
		size_t val_sz = sizeof(val);
		rc = oni_set_opt(ctx, option.getOption(), &val, val_sz);
		oni_get_opt(ctx, option.getOption(), &val_result, &val_sz);
		if(rc && val_result != val){
			LOGERROR("Can't set context option: %s :: %s", option.getName().c_str(), oni_error_str(rc));
			return -1;
		}
		return val;
	}

	oni_size_t getContextOption(const ONIContextOption& option){
		int rc = ONI_ESUCCESS;
		oni_size_t val = (oni_size_t)0;
		size_t val_sz = sizeof(val);
		rc = oni_get_opt(ctx, option.getOption(), &val, &val_sz);
		if(rc){ LOGERROR("Can't get context option: %s :: %s", option.getName().c_str(), oni_error_str(rc)); return -1; };
		LOGINFO("Context option: %s == %i", option.getName().c_str(), val);
		return val;
	}

	volatile oni_ctx* getContext(){
		return &ctx;
	}

	void startFrameRead(){
		if(bThread) stopFrameRead();
		LOGDEBUG("Starting frame read thread");
		bThread = true;
		for(auto it = oniDevices.begin(); it != oniDevices.end();it++){
			auto device = it->second;
			device->reset();
		}
		thread = std::thread(&ONIContext::readFrame, this);
	}

	void stopFrameRead(){
		LOGDEBUG("Stopping frame read thread");
		if(!bThread) return;
		bThread = false;
		if(thread.joinable()) thread.join();
	}

private:

	bool setupContext(){

		LOGINFO("Setting up ONI context");

		if(bIsContextSetup){
			LOGDEBUG("Context already exists, destroying...");
			closeContext();
		}

		bIsContextSetup = false; // TODO: check and warn/tear down if it's already setup?

		int rc = ONI_ESUCCESS;

		// Generate context
		ctx = oni_create_ctx(driverName.c_str());

		if(!ctx){
			LOGERROR("ONI context create: FAIL with driver: %s", driverName.c_str());
			return false;
		}

		LOGINFO("ONI context create: OK with driver: %s", driverName.c_str());

		// Print the driver translator informaiton
		char driver_version[128];
		const oni_driver_info_t* di = oni_get_driver_info(ctx);
		if(di->pre_release == NULL){
			LOGINFO("Driver version patch: %s v%d.%d.%d", di->name, di->major, di->minor, di->patch);
		}else{
			LOGINFO("Driver version pre-release: %s v%d.%d.%d-%s", di->name, di->major, di->minor, di->patch, di->pre_release);
		}

		// Initialize context and discover hardware
		rc = oni_init_ctx(ctx, host_idx);
		if(rc){
			LOGERROR("Hardware init: FAIL ==> %s", oni_error_str(rc));
			return false;
		}else{
			LOGINFO("Hardware init: OK");
		}

		// there's a firmware bug which requires two context inits 
		// for PORTVOLTAGE to be set, so let's just always do it!
		oni_destroy_ctx(ctx);
		ctx = oni_create_ctx(driverName.c_str());
		rc = oni_init_ctx(ctx, host_idx);

		if(rc){
			LOGERROR("Hardware re init: FAIL ==> %s", oni_error_str(rc));
			return false;
		}else{
			LOGINFO("Hardware re init: OK");
		}

		maxReadSize = getContextOption(MAXREADFRAMESIZE);
		maxWriteSize = getContextOption(MAXWRITEFRAMESIZE);
		acq_clock_khz = getContextOption(ACQCLKHZ);
		sys_clock_khz = getContextOption(SYSCLKHZ);

		if(maxReadSize == -1) return false;
		if(maxWriteSize == -1) return false;
		if(acq_clock_khz == -1) return false;
		if(sys_clock_khz == -1) return false;

		blockReadBytes = setContextOption(BLOCKREADSIZE, blockReadSize);
		blockWriteBytes = setContextOption(BLOCKWRITESIZE, blockWriteSize);

		if(blockReadBytes == -1) return false;
		if(blockWriteBytes == -1) return false;

		bIsContextSetup = startContext(); // is this ok? or should the user do it!?

		getDeviceTypes(true); // hmm leave this to the user?

		return true;

	}

	void readFrame(){

		while(bThread){

			//fu::debug << "something");

			int rc = ONI_ESUCCESS;

			oni_frame_t *frame = NULL;
			rc = oni_read_frame(ctx, &frame);

			if(rc < 0){
				LOGERROR("Frame read error: %s", oni_error_str(rc));
			}else{
				
				auto it = oniDevices.find((unsigned int)frame->dev_idx);

				if(it == oniDevices.end()){
					LOGERROR("ONIDevice doesn't exist with idx: %i", frame->dev_idx);
				}else{
					auto device = it->second;
					device->process(frame);

					//if(device->getDeviceTypeID() == ONIDevice::TypeID::HEARTBEAT){
					//	Rhs2116Device* rhd2116Device1 = (Rhs2116Device*)getDevice(256);
					//	Rhs2116Device* rhd2116Device2 = (Rhs2116Device*)getDevice(257);
					//	rhd2116Device1->printSampleCount();
					//	rhd2116Device2->printSampleCount();
					//}
					
					//fu::debug << "DEV: " << frame->dev_idx << " " << frame->time << " " << device->getName() << fu::endl;
				}

			}

			oni_destroy_frame(frame);

			//std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		}

	}

	void clearDevices(){

		deviceTypes.clear();

		for(auto it = oniDevices.begin(); it != oniDevices.end();it++){
			delete it->second;
		}

		oniDevices.clear();

	}

private:

	bool bIsContextSetup = false;

	unsigned int blockReadSize = 2048;
	unsigned int blockWriteSize = 2048;

	unsigned int blockReadBytes = 0;
	unsigned int blockWriteBytes = 0;

	volatile bool bThread = false;
	std::thread thread;
	std::mutex mutex;

	volatile oni_ctx ctx = NULL;

	std::unordered_map<unsigned int, ONIDevice*> oniDevices;
	std::vector<oni_device_t> deviceTypes = {};

	int host_idx = -1;
	std::string driverName = "riffa";

	uint32_t maxReadSize = -1;
	uint32_t maxWriteSize = -1;
	uint32_t acq_clock_khz = -1;
	uint32_t sys_clock_khz = -1;

	//char* reg_path = NULL;

};

