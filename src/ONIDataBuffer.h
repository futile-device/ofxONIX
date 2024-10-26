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

#include "ONISettingTypes.h"
#include "ONIUtility.h"

#pragma once

template<typename DataType>
class ONIDataBuffer{

public:

	virtual ~ONIDataBuffer(){
		buffer.clear();
	}

	void setup(const std::string& name, size_t size){
		this->name = name;
		resize(size);
	}

	void resize(const size_t& size){
		buffer.clear();
		buffer.resize(size);
		currentIDX = 0;
	}

	inline const std::string& getName(){
		return name;
	}

	inline void push(const DataType& data){
		buffer[currentIDX] = data;
		increaseIDX();
	}

	DataType& at(const size_t& index){
		return buffer[index];
	}

	DataType& currentBuffer(){
		return buffer[currentIDX];
	}

	DataType& lastBuffer(){
		return buffer[lastIDX()];
	}

	DataType& nextBuffer(){
		return buffer[nextIDX()];
	}

	inline size_t increaseIDX(){
		currentIDX = (currentIDX + 1) % buffer.size();
		return currentIDX;
	}

	inline size_t lastIDX(){
		return offsetIDX(-1);
	}

	inline size_t nextIDX(){
		return offsetIDX(+1);
	}

	inline size_t offsetIDX(const size_t& offset){
		return (currentIDX + offset) % buffer.size();
	}

	inline std::vector<DataType>& getBuffer(){
		return buffer;
	}

	inline const size_t& size(){
		return buffer.size();
	}

protected:

	std::string name = "UNKNOWN";

	std::vector<DataType> buffer;
	size_t currentIDX = 0;

};

//typedef ONIDataBuffer<Rhs2116DataFrame> Rhs2116FrameBuffer;