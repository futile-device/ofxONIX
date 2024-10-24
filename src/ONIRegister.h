//
//  ONIDevice.h
//
//  Created by Matt Gingold on 13.09.2024.
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

#include "ONIUtility.h"

#pragma once

class ONIRegister{

public:

	ONIRegister() : addr(0), name("UNKNOWN") {};
	//ONIRegister(unsigned int val, std::string name) : addr(val), name(name) {};
	const std::string& getName() const { return name; };
	const unsigned int& getAddress() const { return addr; };
	const std::string& getDeviceName() const { return devn; };
	operator unsigned int&() { return addr; }
	operator unsigned int() const { return addr; }

	friend inline bool operator< (const ONIRegister& lhs, const ONIRegister& rhs);
	friend inline bool operator==(const ONIRegister& lhs, const ONIRegister& rhs);
	friend inline std::ostream& operator<<(std::ostream& os, const ONIRegister& obj);

	// copy assignment (copy-and-swap idiom)
	ONIRegister& ONIRegister::operator=(ONIRegister other) noexcept {
		std::swap(addr, other.addr);
		std::swap(name, other.name);
		std::swap(devn, other.devn);
		return *this;
	}

protected:

	unsigned int addr = 0;
	std::string name = "UNKNOWN";
	std::string devn = "UNKNOWN";

};

inline std::ostream& operator<<(std::ostream& os, const ONIRegister& obj) {
	os << obj.addr << " " << obj.name;
	return os;
}

inline bool operator< (const ONIRegister& lhs, const ONIRegister& rhs) { return lhs.addr < rhs.addr; }
inline bool operator> (const ONIRegister& lhs, const ONIRegister& rhs) { return rhs < lhs; }
inline bool operator<=(const ONIRegister& lhs, const ONIRegister& rhs) { return !(lhs > rhs); }
inline bool operator>=(const ONIRegister& lhs, const ONIRegister& rhs) { return !(lhs < rhs); }

inline bool operator==(const ONIRegister& lhs, const ONIRegister& rhs) { return (lhs.addr == rhs.addr && lhs.name == rhs.name && lhs.devn == rhs.devn); }
inline bool operator!=(const ONIRegister& lhs, const ONIRegister& rhs) { return !(lhs == rhs); }
