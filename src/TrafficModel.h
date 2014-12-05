// ===============================================================================================
// Evacuation Solver: Traffic model class definition
// Description:
//
// Copyright (C) 2014 Kaveh Shahabi
// Distributed under the Apache Software License, Version 2.0. (See accompanying file LICENSE.txt)
//
// Author: Kaveh Shahabi
// URL: http://github.com/spatial-computing/CASPER
// ===============================================================================================

#pragma once

#include "StdAfx.h"
#include "utils.h"

// Hash map for cached calculated congestions
typedef public std::unordered_map<double, double> FlowCongestionMap;
typedef std::unordered_map<double, double>::const_iterator FlowCongestionMapItr;
typedef std::pair<double, double> FlowCongestionMapPair;

typedef FlowCongestionMap * FlowCongestionMapPtr;
typedef public std::unordered_map<double, FlowCongestionMapPtr> CapacityFlowsMap;
typedef std::unordered_map<double, FlowCongestionMapPtr>::const_iterator CapacityFlowsMapItr;
typedef std::pair<double, FlowCongestionMapPtr> CapacityFlowsMapPair;

class TrafficModel
{
private:
	EvcTrafficModel model;
	double saturationDensPerCap;
	CapacityFlowsMap * myCache;
	unsigned int cacheMiss;
	unsigned int cacheHit;

	double internalGetCongestionPercentage(double capacity, double flow) const;

public:
	double InitDelayCostPerPop;
	double CriticalDensPerCap;

	TrafficModel(EvcTrafficModel Model, double _criticalDensPerCap, double _saturationDensPerCap, double _initDelayCostPerPop);
	virtual ~TrafficModel(void);
	TrafficModel(const TrafficModel & that) = delete;
	TrafficModel & operator=(const TrafficModel &) = delete;
	double GetCongestionPercentage(double capacity, double flow);
	double LeftCapacityOnEdge(double capacity, double reservedFlow, double originalEdgeCost) const;
	double GetCacheHitPercentage() const { return 100.0 * cacheHit / (cacheHit + cacheMiss); }
};

