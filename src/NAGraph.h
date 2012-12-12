#pragma once

#include <hash_map>
#include <fstream>

class Evacuee;
class NAEdge;

#define EVC_SOLVER_METHOD			char
#define EVC_SOLVER_METHOD_SP		0x0
#define EVC_SOLVER_METHOD_CCRP		0x1
#define EVC_SOLVER_METHOD_CASPER	0x2

#define EVC_TRAFFIC_MODEL			char
#define EVC_TRAFFIC_MODEL_FLAT		0x0
#define EVC_TRAFFIC_MODEL_STEP		0x1
#define EVC_TRAFFIC_MODEL_LINEAR	0x2
#define EVC_TRAFFIC_MODEL_CASPER	0x3

// The NAVertex class is what sits on top of the INetworkJunction interface and holds extra
// information about each junction/vertex which are helpful for CASPER algorithm.
// g: cost from source
// h: estimated cost to nearest safe zone
// Edge: the previous edge leading to this vertex
// Previous: the previous vertex leading to this vertex

struct HValue
{
public:
	double Value;
	long EdgeID;
	
	HValue(long edgeID, double value)
	{
		EdgeID = edgeID;
		Value = value;
	}

	static bool LessThan(HValue & a, HValue & b)
	{
		return a.Value < b.Value;
	}
};

class NAVertex
{
private:
	NAEdge * BehindEdge;
	std::vector<HValue> * h;

public:
	double g;
	INetworkJunctionPtr Junction;
	NAVertex * Previous;
	long EID;
	// double posAlong;

	__forceinline double minh() const
	{
		std::vector<HValue>::reference reff = h->front();
		return reff.Value;
	}

	void ResetHValues(void)
	{
		h->clear(); 
		h->reserve(2);
		h->push_back(HValue(0l, FLT_MAX));
	}

	inline void SetBehindEdge(NAEdge * behindEdge);
	NAEdge * GetBehindEdge() { return BehindEdge; }
	bool UpdateHeuristic(long edgeid, NAVertex * n);
	
	NAVertex(void);
	~NAVertex(void) { delete h; }
	NAVertex(const NAVertex& cpy);
	NAVertex(INetworkJunctionPtr junction, NAEdge * behindEdge);
};

typedef NAVertex * NAVertexPtr;
typedef stdext::hash_map<long, NAVertexPtr> NAVertexTable;
typedef stdext::hash_map<long, NAVertexPtr>::_Pairib NAVertexTableInsertReturn;
typedef stdext::hash_map<long, NAVertexPtr>::iterator NAVertexTableItr;
typedef std::pair<long, NAVertexPtr> _NAVertexTablePair;
#define NAVertexTablePair(a) _NAVertexTablePair(a->EID, a)

// This collection object has two jobs:
// it makes sure that there exist only one copy of a vertex in it that is connected to each INetworkJunction.
// this will be helpful to avoid duplicate copies pointing to the same junction structure. So data attached
// to vertex will be always fresh and there will be no inconsistancy. Care has to be taken not to overwrite
// important vertices with new ones. The second job is just a GC. since all vertices are being newed here,
// it can all be deleted at the end here as well.

class NAVertexCache
{
private:
	NAVertexTable * cache;
	std::vector<NAVertex *> * sideCache;

public:
	NAVertexCache(void)
	{
		cache = new DEBUG_NEW_PLACEMENT stdext::hash_map<long, NAVertexPtr>();
		sideCache = new DEBUG_NEW_PLACEMENT std::vector<NAVertex *>();
	}

	~NAVertexCache(void) 
	{
		Clear();
		delete cache;
		delete sideCache;
	}
	
	NAVertexPtr New(INetworkJunctionPtr junction);
	bool UpdateHeuristic(long edgeid, NAVertex * n);
	NAVertexPtr Get(long eid);
	void Clear();
	void CollectAndRelease();
};

class NAVertexCollector
{
private:
	std::vector<NAVertexPtr> * cache;

public:
	NAVertexCollector(void)
	{
		cache = new DEBUG_NEW_PLACEMENT std::vector<NAVertexPtr>();
	}

	~NAVertexCollector(void) 
	{
		Clear();
		delete cache;
	}
	
	NAVertexPtr New(INetworkJunctionPtr junction);
	int Size() { return cache->size(); }
	void Clear();
};

struct EdgeReservation
{
public:
	double FromCost;
	double ToCost;
	Evacuee * Who;

	EdgeReservation(Evacuee * who, double fromCost, double toCost)
	{
		Who = who;
		FromCost = fromCost;
		ToCost = toCost;
	}
};

class EdgeReservations
{
private:
	// std::vector<EdgeReservation> * List;
	float ReservedPop;
	float SaturationDensPerCap;
	float CriticalDens;
	float Capacity;
	bool  DirtyFlag;
	float initDelayCostPerPop;

public:	
	inline void SetClean() { DirtyFlag = false; }

	EdgeReservations(float capacity, float CriticalDensPerCap, float SaturationDensPerCap, float InitDelayCostPerPop);
	EdgeReservations(const EdgeReservations& cpy);
	
	void Clear()
	{
		//List->clear();
	}
	
	~EdgeReservations(void)
	{
		Clear();
		//delete List;
	}
	friend class NAEdge;
};

// EdgeReservations hash map
typedef EdgeReservations * EdgeReservationsPtr;
typedef public stdext::hash_map<long, EdgeReservationsPtr> NAResTable;
typedef stdext::hash_map<long, EdgeReservationsPtr>::iterator NAResTableItr;
typedef std::pair<long, EdgeReservationsPtr> NAResTablePair;

// The NAEdge class is what sits on top of the INetworkEdge interface and holds extra
// information about each edge which are helpful for CASPER algorithm.
// Capacity: road initial capacity
// Cost: road initial travel cost
// reservations: a vector of evacuees commited to use this edge at a certain time/cost period
class NAEdge
{
private:	
	EdgeReservations * reservations;
	EVC_TRAFFIC_MODEL trafficModel;
	double CASPERRatio;
	mutable double cachedCost[2];
	mutable unsigned short calcSaved;
	double GetTrafficSpeedRatio(double allPop) const;

public:
	double OriginalCost;
	esriNetworkEdgeDirection Direction;
	NAVertex * ToVertex;
	INetworkEdgePtr NetEdge;
	INetworkEdgePtr LastExteriorEdge;	
	long EID;
	double GetCost(double newPop, EVC_SOLVER_METHOD method) const;
	double GetCurrentCost() const;
	double LeftCapacity() const;
	double OriginalCapacity() const;

	HRESULT QuerySourceStuff(long * sourceOID, long * sourceID, double * fromPosition, double * toPosition) const;	
	bool AddReservation(/* Evacuee * evacuee, double fromCost, double toCost, */ double population);
	NAEdge(INetworkEdgePtr, long capacityAttribID, long costAttribID, float CriticalDensPerCap, float SaturationDensPerCap, NAResTable *, float InitDelayCostPerPop, EVC_TRAFFIC_MODEL);
	NAEdge(const NAEdge& cpy);

	static bool LessThanNonHur(NAEdge * n1, NAEdge * n2) { return n1->ToVertex->g < n2->ToVertex->g; }
	static bool LessThanHur   (NAEdge * n1, NAEdge * n2) { return n1->ToVertex->g + n1->ToVertex->minh() < n2->ToVertex->g + n2->ToVertex->minh(); }

	float GetReservedPop() const { return reservations->ReservedPop; }
	inline bool IsDirty() const { return reservations->DirtyFlag; }
	inline unsigned short GetCalcSaved() const { return calcSaved; }
};

typedef NAEdge * NAEdgePtr;
typedef public stdext::hash_map<long, NAEdgePtr> NAEdgeTable;
typedef stdext::hash_map<long, NAEdgePtr>::iterator NAEdgeTableItr;
typedef std::pair<long, NAEdgePtr> _NAEdgeTablePair;
#define NAEdgeTablePair(a) _NAEdgeTablePair(a->EID, a)

class NAEdgeClosed
{
private:
	NAEdgeTable * cacheAlong;
	NAEdgeTable * cacheAgainst;

public:
	NAEdgeClosed(void)
	{ 
		cacheAlong = new DEBUG_NEW_PLACEMENT stdext::hash_map<long, NAEdgePtr>();
		cacheAgainst = new DEBUG_NEW_PLACEMENT stdext::hash_map<long, NAEdgePtr>();
	}

	~NAEdgeClosed(void) 
	{
		Clear();
		delete cacheAlong; 
		delete cacheAgainst; 
	}
	
	void Clear() { cacheAlong->clear(); cacheAgainst->clear(); }
	int Size() { return cacheAlong->size() + cacheAgainst->size(); }
	HRESULT Insert(NAEdgePtr edge);
	bool IsClosed(NAEdgePtr edge);
};

typedef std::pair<long, char> _NAEdgeContainerPair;
#define NAEdgeContainerPair(a) _NAEdgeContainerPair(a, 1)

class NAEdgeContainer
{
private:
	stdext::hash_map<long, char> * cacheAlong;
	stdext::hash_map<long, char> * cacheAgainst;

public:
	NAEdgeContainer(void)
	{ 
		cacheAlong = new DEBUG_NEW_PLACEMENT stdext::hash_map<long, char>();
		cacheAgainst = new DEBUG_NEW_PLACEMENT stdext::hash_map<long, char>();
	}

	~NAEdgeContainer(void) 
	{
		Clear();
		delete cacheAlong; 
		delete cacheAgainst; 
	}
	
	void Clear() { cacheAlong->clear(); cacheAgainst->clear(); }
	int Size() { return cacheAlong->size() + cacheAgainst->size(); }
	HRESULT Insert(INetworkEdgePtr edge);
	bool Exist(INetworkEdgePtr edge);
};

// This collection object has two jobs:
// it makes sure that there exist only one copy of an edge in it that is connected to each INetworkEdge.
// this will be helpful to avoid duplicate copies pointing to the same edge structure. So data attached
// to edge will be always fresh and there will be no inconsistancy. Care has to be taken not to overwrite
// important edges with new ones. The second job is just a GC. since all edges are being newed here,
// it can all be deleted at the end here as well.

class NAEdgeCache
{
private:
	// std::vector<NAEdgePtr>	* sideCache;
	NAEdgeTable				* cacheAlong;
	NAEdgeTable				* cacheAgainst;
	long					capacityAttribID;
	long					costAttribID;
	float					saturationPerCap;
	float					criticalDensPerCap;
	bool					twoWayRoadsShareCap;
	NAResTable				* resTableAlong;
	NAResTable				* resTableAgainst;
	float					initDelayCostPerPop;
	EVC_TRAFFIC_MODEL		trafficModel;

public:

	NAEdgeCache(long CapacityAttribID, long CostAttribID, float SaturationPerCap, float CriticalDensPerCap, bool TwoWayRoadsShareCap, float InitDelayCostPerPop, EVC_TRAFFIC_MODEL TrafficModel)
	{
		// sideCache = new DEBUG_NEW_PLACEMENT std::vector<NAEdgePtr>();
		initDelayCostPerPop = InitDelayCostPerPop;
		capacityAttribID = CapacityAttribID;
		costAttribID = CostAttribID;
		cacheAlong = new DEBUG_NEW_PLACEMENT stdext::hash_map<long, NAEdgePtr>();
		cacheAgainst = new DEBUG_NEW_PLACEMENT stdext::hash_map<long, NAEdgePtr>();
		saturationPerCap = SaturationPerCap;
		criticalDensPerCap = CriticalDensPerCap;
		if (saturationPerCap <= criticalDensPerCap) saturationPerCap += criticalDensPerCap;
		twoWayRoadsShareCap = TwoWayRoadsShareCap;
		trafficModel = TrafficModel;

		resTableAlong = new DEBUG_NEW_PLACEMENT NAResTable();
		if (twoWayRoadsShareCap) resTableAgainst = resTableAlong;
		else resTableAgainst = new DEBUG_NEW_PLACEMENT NAResTable();
	}

	~NAEdgeCache(void) 
	{
		Clear();
		// delete sideCache;
		delete cacheAlong; 
		delete cacheAgainst;
		delete resTableAlong;
		if (!twoWayRoadsShareCap) delete resTableAgainst;
	}

	NAEdgePtr New(INetworkEdgePtr edge, bool replace = false);

	NAEdgeTableItr AlongBegin()   const { return cacheAlong->begin();   }
	NAEdgeTableItr AlongEnd()     const { return cacheAlong->end();     }
	NAEdgeTableItr AgainstBegin() const { return cacheAgainst->begin(); }
	NAEdgeTableItr AgainstEnd()   const { return cacheAgainst->end();   }
	int Size() const { return cacheAlong->size() + cacheAgainst->size();}
	
	#pragma warning(push)
	#pragma warning(disable : 4100) /* Ignore warnings for unreferenced function parameters */
	void CleanAllEdgesAndRelease(double maxPredictionCost)
	{				
		#ifdef TRACE
		std::ofstream f;
		int count = 0;		
		for(NAEdgeTableItr cit = cacheAlong->begin(); cit != cacheAlong->end(); cit++)
		{
			if ((*cit).second->ToVertex != NULL && (*cit).second->ToVertex->g > maxPredictionCost && (*cit).second->GetReservedPop() <= 0.0) count++;
		}
		for(NAEdgeTableItr cit = cacheAgainst->begin(); cit != cacheAgainst->end(); cit++)
		{
			if ((*cit).second->ToVertex != NULL && (*cit).second->ToVertex->g > maxPredictionCost && (*cit).second->GetReservedPop() <= 0.0) count++;
		}
		f.open("c:\\evcsolver.log", std::ios_base::out | std::ios_base::app);
		f << "Outside Edges: " << count << " of " << Size() << std::endl;
		f.close();
		#endif

		for(NAResTableItr cit = resTableAlong->begin(); cit != resTableAlong->end(); cit++) (*cit).second->SetClean();
		for(NAResTableItr cit = resTableAgainst->begin(); cit != resTableAgainst->end(); cit++) (*cit).second->SetClean();
	}
	#pragma warning(pop)

	unsigned int TotalCalcSaved() const
	{
		unsigned int total = 0;
		for(NAEdgeTableItr cit = cacheAlong->begin(); cit != cacheAlong->end(); cit++) total += (*cit).second->GetCalcSaved();
		for(NAEdgeTableItr cit = cacheAgainst->begin(); cit != cacheAgainst->end(); cit++) total += (*cit).second->GetCalcSaved();
		return total;
	}

	void Clear();	
};
