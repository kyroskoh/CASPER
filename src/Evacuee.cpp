// ===============================================================================================
// Evacuation Solver: Evacuee class Implementation
// Description: Contains code for evacuee, evacuee path, evacuee path segment (which wrappes graph
// edge) and other useful container types.
//
// Copyright (C) 2014 Kaveh Shahabi
// Distributed under the Apache Software License, Version 2.0. (See accompanying file LICENSE.txt)
//
// Author: Kaveh Shahabi
// URL: http://github.com/spatial-computing/CASPER
// ===============================================================================================

#include "StdAfx.h"
#include "Evacuee.h"
#include "NAVertex.h"
#include "NAEdge.h"
#include "Dynamic.h"

HRESULT PathSegment::GetGeometry(INetworkDatasetPtr ipNetworkDataset, IFeatureClassContainerPtr ipFeatureClassContainer, bool & sourceNotFoundFlag, IGeometryPtr & geometry)
{
	HRESULT hr = S_OK;
	ICurvePtr subCurve;
	if (FAILED(hr = Edge->GetGeometry(ipNetworkDataset, ipFeatureClassContainer, sourceNotFoundFlag, geometry))) return hr;
	if (fromRatio != 0.0 || toRatio != 1.0)
	{
		// We must use only a curve of the line geometry
		ICurve3Ptr ipCurve(geometry);
		if (FAILED(hr = ipCurve->GetSubcurve(fromRatio, toRatio, VARIANT_TRUE, &subCurve))) return hr;
		geometry = subCurve;
	}
	return hr;
}

EvcPath::EvcPath(double initDelayCostPerPop, double routedPop, int order, Evacuee * evc, SafeZone * mySafeZone) :
	baselist(), MySafeZone(mySafeZone), RoutedPop(routedPop), Status(PathStatus::ActiveComplete)
{
	PathStartCost = evc->StartingCost;
	FinalEvacuationCost = RoutedPop * initDelayCostPerPop + PathStartCost;
	ReserveEvacuationCost = RoutedPop * initDelayCostPerPop + PathStartCost;
	OrginalCost = RoutedPop * initDelayCostPerPop + PathStartCost;
	Order = order;
	myEvc = evc;
}

EvcPath::EvcPath(const EvcPath & that) : baselist(), MySafeZone(that.MySafeZone), RoutedPop(that.RoutedPop), Status(that.Status)
{
	PathStartCost = that.PathStartCost;
	FinalEvacuationCost = that.FinalEvacuationCost;
	ReserveEvacuationCost = that.ReserveEvacuationCost;
	OrginalCost = that.OrginalCost;
	Order = that.Order;
	myEvc = that.myEvc;
}

double PathSegment::GetCurrentCost(EvcSolverMethod method) const { return Edge->GetCurrentCost(method) * abs(GetEdgePortion()); }
bool EvcPath::MoreThanPathOrder1(const Evacuee * e1, const Evacuee * e2) { return e1->Paths->front()->Order > e2->Paths->front()->Order; }
bool EvcPath::LessThanPathOrder1(const Evacuee * e1, const Evacuee * e2) { return e1->Paths->front()->Order < e2->Paths->front()->Order; }

// first i have to move the evacuee. then cut the path and back it up. mark the evacuee to be processed again.
size_t EvcPath::DynamicStep_MoveOnPath(const std::unordered_set<EvcPath *, EvcPath::PtrHasher, EvcPath::PtrEqual> & AffectedPaths, std::vector<EvcPath *> & allPaths,
	std::unordered_set<NAEdge *, NAEdgePtrHasher, NAEdgePtrEqual> & DynamicallyAffectedEdges, double CurrentTime, EvcSolverMethod method, INetworkQueryPtr ipNetworkQuery, int & pathGenerationCount)
{
	size_t count = 0, segment = 0, activeCompleteCount = 0;
	double pathCost = 0.0, edgeRatio = 0.0, edgeCost = 0.0;
	std::vector<std::pair<NAEdgePtr, EvcPathPtr>> RemoveReservations;

	if (CurrentTime > 0.0)
	{
		std::sort(allPaths.begin(), allPaths.end(), EvcPath::MoreThanPathOrder2);
		for (auto path : allPaths)
		{
			if (path->myEvc->Status != EvacueeStatus::Unreachable && !path->empty() && path->Status == PathStatus::ActiveComplete)
			{
				// find the segment where we need to cut the path
				if (path->FinalEvacuationCost > CurrentTime)
				{
					for (pathCost = path->PathStartCost, segment = 0; pathCost < CurrentTime && segment < path->size(); ++segment)
						pathCost += path->at(segment)->GetCurrentCost(method);

					// segment is always moving one step ahead of pathCost and segCost.
					--segment;
				}
				else
				{
					pathCost = path->FinalEvacuationCost;
					segment = path->size() - 1;
				}

				// this is the case where the head of population has reached the safe zone but the tail of it
				// is not. Because the initDelayPerPop is non-zero. We will consider this a path that cannot be splited.
				// we have to mark this path as frozen but this somehow at merge we have to be able to tell if someone is stuck or finished
				if (pathCost <= CurrentTime)
				{
					path->Status = PathStatus::FrozenComplete;
					path->myEvc->Status = EvacueeStatus::Processed;
					continue;
				}

				// move the evacuee to this segment
				edgeCost = path->at(segment)->Edge->GetCurrentCost(method);
				edgeRatio = (pathCost - CurrentTime) / edgeCost;
				path->myEvc->DynamicMove(path->at(segment)->Edge, edgeRatio, ipNetworkQuery, CurrentTime);
				path->at(segment)->SetToRatio(edgeRatio);
				path->Status = PathStatus::FrozenSplitted;
				
				// this path is not affected by this round of dynamic changes so no need to count it to be proccessed again.
				// simply split into two paths and mark last one as active
				if (AffectedPaths.find(path) != AffectedPaths.end())
				{
					// pop out the rest of the segments in this path
					for (size_t i = path->size() - 1; i > segment; --i)
					{
						RemoveReservations.push_back(std::pair<NAEdgePtr, EvcPathPtr>(path->at(i)->Edge, path));
						delete path->at(i);
					}

					// we also remove this reservation because the next path will start here and we don't want the evacuee to overlap itself
					RemoveReservations.push_back(std::pair<NAEdgePtr, EvcPathPtr>(path->at(segment)->Edge, path));

					// setup this path as frozen and mark the evacuee as unporcessed
					path->myEvc->Status        = EvacueeStatus::Unprocessed;
					path->myEvc->PredictedCost = path->FinalEvacuationCost;
					path->myEvc->FinalCost     = path->FinalEvacuationCost;
					path->MySafeZone->Reserve (-path->RoutedPop);
					++count;
				}
				else
				{
					// split this path into two seperate paths: one splittedfrozen and the other active. keep evacuee as processed.
					EvcPathPtr newPath = new DEBUG_NEW_PLACEMENT EvcPath(*path);
					newPath->Status = PathStatus::ActiveComplete;
					newPath->Order = ++pathGenerationCount;
					newPath->PathStartCost = CurrentTime;
					++activeCompleteCount;

					// pop out the rest of the segments in this path and then add it to the newPath
					for (size_t i = path->size() - 1; i > segment; --i)
					{
						newPath->push_front(path->at(i));
						path->at(i)->Edge->SwapReservation(path, newPath);
					}

					// we also remove this reservation because the next path will start here and we don't want the evacuee to overlap itself
					PathSegmentPtr dupSegment = new DEBUG_NEW_PLACEMENT PathSegment(path->at(segment)->Edge, edgeRatio);
					path->at(segment)->Edge->SwapReservation(path, newPath);
					newPath->push_front(dupSegment);
					newPath->myEvc->Paths->push_front(newPath);
					path->myEvc->Status = EvacueeStatus::Processed;
				}
				
				path->erase(path->begin() + segment + 1, path->end());
			}
		}
	}

	// once all paths have been splitted and evacuees moved, now we execute all those edge reservation removals
	for (auto p : RemoveReservations)
	{
		p.first->RemoveReservation(p.second, method, true);
		DynamicallyAffectedEdges.insert(p.first);
	}

	return count > 0 ? count : activeCompleteCount;
}

size_t EvcPath::DynamicStep_UnreachableEvacuees(std::shared_ptr<EvacueeList> AllEvacuees, double StartCost)
{
	size_t count = 0;
	for (auto e : *AllEvacuees)
		if (e->Status == EvacueeStatus::Unreachable)
		{
			e->Status = EvacueeStatus::Unprocessed;
			e->PredictedCost = CASPER_INFINITY;
			++count;
			e->StartingCost = StartCost;
		}
	return count;
}

void EvcPath::DynamicStep_MergePaths(std::shared_ptr<EvacueeList> AllEvacuees)
{
	std::vector<EvcPathPtr> frozenList;
	EvcPathPtr mainPath = nullptr, fp = nullptr;

	for (auto evc : *AllEvacuees)
		if (evc->Status != EvacueeStatus::Unreachable)
		{
			// first identify the main path and the frozen ones
			frozenList.clear();
			mainPath = nullptr;
			for (auto p : *evc->Paths)
			{
				if (p->Status == PathStatus::FrozenSplitted) frozenList.push_back(p);
				else
				{
					if (mainPath) throw std::logic_error("One evacuee has many unfrozen/main paths");
					mainPath = p;
				}
			}

			if (frozenList.empty()) continue;
			if (!mainPath)
			{
				// in this case the evacuee has some frozen paths so originally could evacuate but after this dynamic
				// change it no longer can move so it is considered stuck
				/// TODO What happens here is that the evacuee is marked as processed instead of unreachable. Is this going to create some problems for me?
				for (auto p : *evc->Paths) delete p;
				evc->Paths->clear();
				continue;
			}

			_ASSERT_EXPR(evc->Paths->front() == mainPath, L"Front path has to be non-frozen");

			// now merge the frozen ones to the main one in the order they are created
			for (auto p = frozenList.cbegin(); p != frozenList.cend(); ++p)
			{
				fp = *p;
				_ASSERT_EXPR(NAEdge::IsEqualNAEdgePtr(mainPath->front()->Edge, fp->back()->Edge), L"Two half-paths need to share an edge at merge section");
				_ASSERT_EXPR(std::fabs(mainPath->front()->GetFromRatio() - fp->back()->GetToRatio()) < 0.0001 , L"Two half-paths need to be splitted at around the same edge ratio");

				mainPath->front()->SetFromRatio(fp->back()->GetFromRatio());
				for (auto seg = fp->rbegin() + 1; seg != fp->rend(); ++seg) mainPath->push_front(*seg);
				delete fp->back();
				fp->clear();
				delete fp;
			}

			// leave the main path as the only path for this evacuee
			mainPath->PathStartCost = 0.0;
			evc->Paths->clear();
			evc->Paths->push_front(mainPath);
		}
		else
		{
			// this is the case where the evacuee is now stuck accroding to CARMA loop. so we should just release the memory for all paths
			for (auto p : *evc->Paths) delete p;
			evc->Paths->clear();
		}
}

void EvcPath::DetachPathsFromEvacuee(Evacuee * evc, EvcSolverMethod method, std::unordered_set<NAEdgePtr, NAEdgePtrHasher, NAEdgePtrEqual> & touchedEdges, std::shared_ptr<std::vector<EvcPathPtr>> detachedPaths)
{
	// It's time to clean up the evacuee object and reset it for the next iteration
	// To do this we first collect all its paths, take away all edge reservations, and then reset some of its fields.
	// at the end keep a record of touched edges for a 'HowDirty' call
	EvcPathPtr path = nullptr;
	for (auto i = evc->Paths->begin(); i != evc->Paths->end();)
	{
		path = *i;
		if (!path || path->Status != PathStatus::ActiveComplete) ++i; // ignore frozen paths. They are not to be detached
		else
		{
			/// TODO should we also change safezone reservation?
			path->MySafeZone->Reserve(-path->RoutedPop);

			for (auto s = path->crbegin(); s != path->crend(); ++s)
			{
				(*s)->Edge->RemoveReservation(path, method, true);
				touchedEdges.insert((*s)->Edge);
			}

			// this erase acts as iterator advancement too. next we either backup the path in a vector or delete it all together
			i = evc->Paths->erase(i);
			if (detachedPaths) detachedPaths->push_back(path); else delete path; 
		}
	}
}

void EvcPath::ReattachToEvacuee(EvcSolverMethod method, std::unordered_set<NAEdgePtr, NAEdgePtrHasher, NAEdgePtrEqual> & touchedEdges)
{
	for (const auto & s : *this)
	{
		s->Edge->AddReservation(this, method, true);
		touchedEdges.insert(s->Edge);
	}
	/// TODO should we also change safezone reservation?
	MySafeZone->Reserve(RoutedPop);
	myEvc->Paths->push_front(this);
}

double EvcPath::GetMinCostRatio(double MaxEvacuationCost) const
{
	if (MaxEvacuationCost <= 0.0) MaxEvacuationCost = FinalEvacuationCost;
	double PredictionCostRatio = (ReserveEvacuationCost - myEvc-> PredictedCost) / MaxEvacuationCost;
	double EvacuationCostRatio = (FinalEvacuationCost   - ReserveEvacuationCost) / MaxEvacuationCost;
	return min(PredictionCostRatio, EvacuationCostRatio);
}

double EvcPath::GetAvgCostRatio(double MaxEvacuationCost) const
{
	if (MaxEvacuationCost <= 0.0) MaxEvacuationCost = FinalEvacuationCost;
	return (FinalEvacuationCost - myEvc->PredictedCost) / (2.0 * MaxEvacuationCost);
}

void EvcPath::DoesItNeedASecondChance(double ThreasholdForCost, double ThreasholdForPathOverlap, std::vector<EvacueePtr> & AffectingList, double ThisIterationMaxCost, EvcSolverMethod method)
{
	double PredictionCostRatio = (ReserveEvacuationCost - myEvc-> PredictedCost) / ThisIterationMaxCost;
	double EvacuationCostRatio = (FinalEvacuationCost   - ReserveEvacuationCost) / ThisIterationMaxCost;

	if (this->Status == PathStatus::ActiveComplete && (PredictionCostRatio >= ThreasholdForCost || EvacuationCostRatio >= ThreasholdForCost))
	{
		if (myEvc->Status == EvacueeStatus::Processed)
		{
			// since the prediction was bad it probably means the evacuee has more than average vehicles so it should be processed sooner
			AffectingList.push_back(myEvc);
			myEvc->Status = EvacueeStatus::Unprocessed;
		}

		// We have to add the affecting list to be re-routed as well
		// We do this by selecting the paths that have some overlap
		std::vector<EvcPathPtr> crossing;
		Histogram<EvcPathPtr> FreqOfOverlaps;
		crossing.reserve(50);

		for (const auto & seg : *this)
		{
			seg->Edge->GetUniqeCrossingPaths(crossing, true);
			FreqOfOverlaps.WeightedAdd(crossing, seg->GetCurrentCost(method));
		}

		double cutOffWeight = ThreasholdForPathOverlap * FreqOfOverlaps.maxWeight;
		for (const auto & pair : FreqOfOverlaps)
		{
			if (pair.first->Status == PathStatus::ActiveComplete && pair.first->myEvc->Status == EvacueeStatus::Processed && pair.second > cutOffWeight)
			{
				AffectingList.push_back(pair.first->myEvc);
				pair.first->myEvc->Status = EvacueeStatus::Unprocessed;
			}
		}
	}
}

void EvcPath::AddSegment(EvcSolverMethod method, PathSegmentPtr segment)
{
	this->push_front(segment);
	segment->Edge->AddReservation(this, method);
	double p = abs(segment->GetEdgePortion());
	ReserveEvacuationCost += segment->Edge->GetCurrentCost(method) * p;
	OrginalCost += segment->Edge->OriginalCost * p;
}

void EvcPath::CalculateFinalEvacuationCost(double initDelayCostPerPop, EvcSolverMethod method)
{
	FinalEvacuationCost = RoutedPop * initDelayCostPerPop + this->PathStartCost;
	for (const auto & pathSegment : *this) FinalEvacuationCost += pathSegment->GetCurrentCost(method);
	myEvc->FinalCost = max(myEvc->FinalCost, FinalEvacuationCost);
}

HRESULT EvcPath::AddPathToFeatureBuffers(ITrackCancel * pTrackCancel, INetworkDatasetPtr ipNetworkDataset, IFeatureClassContainerPtr ipFeatureClassContainer, bool & sourceNotFoundFlag,
	IStepProgressorPtr ipStepProgressor, double & globalEvcCost, IFeatureBufferPtr ipFeatureBufferR, IFeatureCursorPtr ipFeatureCursorR,
	long evNameFieldIndex, long evacTimeFieldIndex, long orgTimeFieldIndex, long popFieldIndex, long zoneNameFieldIndex)
{
	HRESULT hr = S_OK;
	IPointCollectionPtr pline = IPointCollectionPtr(CLSID_Polyline);
	long pointCount = -1;
	VARIANT_BOOL keepGoing;
	IGeometryPtr ipGeometry;
	esriGeometryType type;
	IPointCollectionPtr pcollect;
	IPointPtr p;
	VARIANT RouteOID;

	for (const auto & pathSegment : *this)
	{
		// Check to see if the user wishes to continue or cancel the solve (i.e., check whether or not the user has hit the ESC key to stop processing)
		if (pTrackCancel)
		{
			if (FAILED(hr = pTrackCancel->Continue(&keepGoing))) return hr;
			if (keepGoing == VARIANT_FALSE) return E_ABORT;
		}

		// take a path segment from the stack
		pointCount = -1;
		_ASSERT(pathSegment->GetEdgePortion() > 0.0);
		if (FAILED(hr = pathSegment->GetGeometry(ipNetworkDataset, ipFeatureClassContainer, sourceNotFoundFlag, ipGeometry))) return hr;

		ipGeometry->get_GeometryType(&type);

		// get all the points from this polyline and store it in the point stack
		_ASSERT(type == esriGeometryPolyline);

		if (type == esriGeometryPolyline)
		{
			pathSegment->pline = (IPolylinePtr)ipGeometry;
			pcollect = pathSegment->pline;
			if (FAILED(hr = pcollect->get_PointCount(&pointCount))) return hr;

			// if this is not the last path segment then the last point is redundant.
			pointCount--;
			for (long i = 0; i < pointCount; i++)
			{
				if (FAILED(hr = pcollect->get_Point(i, &p))) return hr;
				if (FAILED(hr = pline->AddPoint(p))) return hr;
			}
		}
	}

	// Add the last point of the last path segment to the polyline
	if (pointCount > -1)
	{
		pcollect->get_Point(pointCount, &p);
		pline->AddPoint(p);
	}

	// add the initial delay cost
	/// FinalEvacuationCost += RoutedPop * initDelayCostPerPop; // we no longer calculate Final cost since the dynamic step is doing it more accurately
	globalEvcCost = max(globalEvcCost, FinalEvacuationCost);

	// Store the feature values on the feature buffer
	if (FAILED(hr = ipFeatureBufferR->putref_Shape((IPolylinePtr)pline))) return hr;
	if (FAILED(hr = ipFeatureBufferR->put_Value(evNameFieldIndex, myEvc->Name))) return hr;
	if (FAILED(hr = ipFeatureBufferR->put_Value(evacTimeFieldIndex, ATL::CComVariant(FinalEvacuationCost)))) return hr;
	if (FAILED(hr = ipFeatureBufferR->put_Value(orgTimeFieldIndex, ATL::CComVariant(OrginalCost)))) return hr;
	if (FAILED(hr = ipFeatureBufferR->put_Value(popFieldIndex, ATL::CComVariant(RoutedPop)))) return hr;
	if (zoneNameFieldIndex >= 0 && MySafeZone != nullptr) { if (FAILED(hr = ipFeatureBufferR->put_Value(zoneNameFieldIndex, ATL::CComVariant(MySafeZone->Name)))) return hr; }

	// Insert the feature buffer in the insert cursor
	if (FAILED(hr = ipFeatureCursorR->InsertFeature(ipFeatureBufferR, &RouteOID))) return hr;

	#ifdef DEBUG
	std::wostringstream os_;
	os_.precision(3);
	os_ << RouteOID.intVal << ',' << myEvc->PredictedCost << ',' << ReserveEvacuationCost << ',' << FinalEvacuationCost << std::endl;
	OutputDebugStringW(os_.str().c_str());
	#endif
	#ifdef TRACE
	std::ofstream f;
	f.open("c:\\evcsolver.log", std::ios_base::out | std::ios_base::app);
	f << RouteOID.intVal << ',' << myEvc->PredictedCost << ',' << ReserveEvacuationCost << ',' << FinalEvacuationCost << std::endl;
	f.close();
	#endif
	
	// Step the progress bar before continuing to the next Evacuee point
	if (ipStepProgressor) ipStepProgressor->Step();

	// Check to see if the user wishes to continue or cancel the solve (i.e., check whether or not the user has hit the ESC key to stop processing)
	if (pTrackCancel)
	{
		if (FAILED(hr = pTrackCancel->Continue(&keepGoing))) return hr;
		if (keepGoing == VARIANT_FALSE) return E_ABORT;
	}
	return hr;
}

Evacuee::Evacuee(VARIANT name, double pop, UINT32 objectID)
{
	StartingCost = 0.0;
	ObjectID = objectID;
	Name = name;
	VerticesAndRatio = new DEBUG_NEW_PLACEMENT std::vector<NAVertexPtr>();
	Paths = new DEBUG_NEW_PLACEMENT std::list<EvcPathPtr>();
	Population = pop;
	PredictedCost = CASPER_INFINITY;
	Status = EvacueeStatus::Unprocessed;
	ProcessOrder = -1;
	FinalCost = CASPER_INFINITY;
	DiscoveryLeaf = nullptr;
}

Evacuee::~Evacuee(void)
{
	for (auto & p : *Paths) delete p;
	for (auto v : *VerticesAndRatio) delete v;
	Paths->clear();
	VerticesAndRatio->clear();
	delete VerticesAndRatio;
	delete Paths;
}

void Evacuee::DynamicMove(NAEdgePtr edge, double toRatio, INetworkQueryPtr ipNetworkQuery, double startTime)
{
	INetworkElementPtr ipElement = nullptr;
	for (auto v : *VerticesAndRatio) delete v;
	VerticesAndRatio->clear();

	if (FAILED(ipNetworkQuery->CreateNetworkElement(esriNETJunction, &ipElement))) return;
	INetworkJunctionPtr toJunction(ipElement);
	if (FAILED(edge->NetEdge->QueryJunctions(nullptr, toJunction))) return;

	NAVertexPtr myVertex = new DEBUG_NEW_PLACEMENT NAVertex(toJunction, edge);
	myVertex->GVal = 1.0 - toRatio;
	DiscoveryLeaf = edge;
	StartingCost = startTime;

	VerticesAndRatio->push_back(myVertex);
}

EvacueeList::~EvacueeList()
{
	for (const auto & e : *this) delete e;
	clear();
}

void MergeEvacueeClusters(std::unordered_map<long, std::list<EvacueePtr>> & EdgeEvacuee, std::vector<EvacueePtr> & ToErase, double OKDistance)
{
	EvacueePtr left = nullptr;
	NAEdgePtr edge = nullptr;
	for (const auto & l : EdgeEvacuee)
	{
		left = nullptr;
		edge = l.second.front()->VerticesAndRatio->front()->GetBehindEdge();
		for (const auto & i : l.second)
		{
			if (left && abs(i->VerticesAndRatio->front()->GVal - left->VerticesAndRatio->front()->GVal) <= OKDistance / edge->OriginalCost)
			{
				// merge i with left
				ToErase.push_back(i);
				left->Population += i->Population;
			}
			else left = i;
		}
	}
}

void SortedInsertIntoMapOfLists(std::unordered_map<long, std::list<EvacueePtr>> & EdgeEvacuee, long eid, EvacueePtr evc)
{
	auto i = EdgeEvacuee.insert(std::pair<long, std::list<EvacueePtr>>(eid, std::list<EvacueePtr>()));
	auto j = i.first->second.begin();
	while (j != i.first->second.end() && evc->VerticesAndRatio->front()->GVal > (*j)->VerticesAndRatio->front()->GVal) ++j;
	i.first->second.insert(j, evc);
}

void EvacueeList::FinilizeGroupings(double OKDistance, DynamicMode DynamicCASPEREnabled)
{
	// turn off seperation flag if dynamic capser is enabled
	if (DynamicCASPEREnabled == DynamicMode::Full || DynamicCASPEREnabled == DynamicMode::Smart)
	{
		SeperationDisabledForDynamicCASPER = CheckFlag(groupingOption, EvacueeGrouping::Separate);
		groupingOption &= ~EvacueeGrouping::Separate;
	}

	if (CheckFlag(groupingOption, EvacueeGrouping::Merge))
	{
		std::unordered_map<long, EvacueePtr> VertexEvacuee;
		std::unordered_map<long, std::list<EvacueePtr>> EdgeAlongEvacuee, EdgeAgainstEvacuee, DoubleEdgeEvacuee;
		std::vector<EvacueePtr> ToErase;
		NAVertexPtr v1;
		NAEdgePtr e1;
		
		for (const auto & evc : *this)
		{
			v1 = evc->VerticesAndRatio->front();
			e1 = v1->GetBehindEdge();
			if (!e1) // evacuee mapped to intersection
			{
				auto i = VertexEvacuee.find(v1->EID);
				if (i == VertexEvacuee.end()) VertexEvacuee.insert(std::pair<long, EvacueePtr>(v1->EID, evc));
				else
				{
					ToErase.push_back(evc);
					i->second->Population += evc->Population;
				}
			}
			else if (evc->VerticesAndRatio->size() == 2) SortedInsertIntoMapOfLists(DoubleEdgeEvacuee, e1->EID, evc);  // evacuee mapped to both side of the street segment
			else if (e1->Direction == esriNetworkEdgeDirection::esriNEDAlongDigitized) SortedInsertIntoMapOfLists(EdgeAlongEvacuee, e1->EID, evc);
			else SortedInsertIntoMapOfLists(EdgeAgainstEvacuee, e1->EID, evc);
		}

		MergeEvacueeClusters(EdgeAgainstEvacuee, ToErase, OKDistance);
		MergeEvacueeClusters(EdgeAlongEvacuee,   ToErase, OKDistance);
		MergeEvacueeClusters(DoubleEdgeEvacuee,  ToErase, OKDistance);

		for (const auto & e : ToErase)
		{
			unordered_erase(e, [&](const EvacueePtr & evc1, const EvacueePtr & evc2)->bool { return evc1->ObjectID == evc2->ObjectID; });
			delete e;
		}
	}
	shrink_to_fit();
}

void NAEvacueeVertexTable::InsertReachable(std::shared_ptr<EvacueeList> list, CARMASort sortDir, std::shared_ptr<NAEdgeContainer> leafs)
{
	for(const auto & evc : *list)
	{
		if (evc->Status == EvacueeStatus::Unprocessed && evc->Population > 0.0)
		{
			// reset evacuation prediction for continues carma sort
			if (sortDir == CARMASort::BWCont || sortDir == CARMASort::FWCont) evc->PredictedCost = CASPER_INFINITY;

			// this is to help CARMA find this evacuee again if it happens to be in a clean part of the tree/graph
			if (evc->DiscoveryLeaf)
			{
				// Speedup: check if this evacuee is for sure trapped in this case we mark it as unreachable even before CARMA starts
				if (evc->DiscoveryLeaf->OriginalCost >= CASPER_INFINITY)
				{
					evc->Status = EvacueeStatus::Unreachable;
					continue;
				}
				leafs->Insert(evc->DiscoveryLeaf->EID, (unsigned char)3);
			}
			evc->Status = EvacueeStatus::CARMALooking;

			for (const auto & v : *evc->VerticesAndRatio)
			{
				if (find(v->EID) == end()) insert(_NAEvacueeVertexTablePair(v->EID, std::vector<EvacueePtr>()));
				at(v->EID).push_back(evc);
			}
		}
	}
}

void NAEvacueeVertexTable::RemoveDiscoveredEvacuees(NAVertexPtr myVertex, NAEdgePtr myEdge, std::shared_ptr<std::vector<EvacueePtr>> SortedEvacuees, double pop, EvcSolverMethod method)
{
	const auto & pair = find(myVertex->EID);
	NAVertexPtr foundVertexRatio = nullptr;
	NAEdgePtr behindEdge = nullptr;
	double newPredictedCost = 0.0, edgeCost = 0.0;

	if (pair != end())
	{
		for (const auto & evc : pair->second)
		{
			foundVertexRatio = nullptr;
			for (const auto & v : *evc->VerticesAndRatio)
			{
				if (v && v->EID == myVertex->EID)
				{
					foundVertexRatio = v;
					break;
				}
			}
			if (foundVertexRatio && evc->Status == EvacueeStatus::CARMALooking)
			{
				behindEdge = foundVertexRatio->GetBehindEdge();
				if (behindEdge) edgeCost = behindEdge->GetCost(pop, method);
				else edgeCost = 0.0;
				if (edgeCost < CASPER_INFINITY)
				{
					evc->Status = EvacueeStatus::Unprocessed;
					newPredictedCost = myVertex->GVal + foundVertexRatio->GVal * edgeCost;
					evc->PredictedCost = min(evc->PredictedCost, newPredictedCost + evc->StartingCost);

					// because this edge helped us find a new evacuee, we save it as a leaf for the next carma loop
					evc->DiscoveryLeaf = myEdge;
				}
				else evc->Status = EvacueeStatus::Unreachable;
				SortedEvacuees->push_back(evc);
			}
		}
		erase(myVertex->EID);
	}
}

void NAEvacueeVertexTable::LoadSortedEvacuees(std::shared_ptr<std::vector<EvacueePtr>> SortedEvacuees) const
{
	#ifdef TRACE
	std::ofstream f;
	f.open("c:\\evcsolver.log", std::ios_base::out | std::ios_base::app);
	f << "List of unreachable evacuees =";
	#endif
	for (const auto & evcList : *this)
		for (const auto & evc : evcList.second)
		{
			if (evc->Status == EvacueeStatus::CARMALooking || evc->PredictedCost >= CASPER_INFINITY)
			{
				evc->Status = EvacueeStatus::Unreachable;
				#ifdef TRACE
				f << ' ' << ATL::CW2A(e->Name.bstrVal);
				#endif
			}
			else SortedEvacuees->push_back(evc);
		}
	#ifdef TRACE
	f << std::endl;
	f.close();
	#endif
}

SafeZone::~SafeZone() { delete VertexAndRatio; }

SafeZone::SafeZone(INetworkJunctionPtr _junction, NAEdge * _behindEdge, double posAlong, VARIANT cap, VARIANT name)
	: junction(_junction), behindEdge(_behindEdge), positionAlong(posAlong), capacity(0.0), Name(name.dblVal)
{
	reservedPop = 0.0;
	VertexAndRatio = new DEBUG_NEW_PLACEMENT NAVertex(junction, behindEdge);
	VertexAndRatio->GVal = posAlong;
	if (cap.vt == VT_R8) capacity = cap.dblVal;
	else if (cap.vt == VT_BSTR) swscanf_s(cap.bstrVal, L"%lf", &capacity);
}

double SafeZone::SafeZoneCost(double population2Route, EvcSolverMethod solverMethod, double costPerDensity, double * globalDeltaCost)
{
	double cost = 0.0;
	double totalPop = population2Route + reservedPop;
	if (capacity == 0.0 && costPerDensity > 0.0) return CASPER_INFINITY;
	if (totalPop > capacity && capacity > 0.0) cost += costPerDensity * ((totalPop / capacity) - 1.0);
	if (behindEdge) cost += behindEdge->GetCost(population2Route, solverMethod, globalDeltaCost) * positionAlong;
	return cost;
}

bool SafeZone::IsRestricted(std::shared_ptr<NAEdgeCache> ecache, NAEdge * leadingEdge, double costPerDensity)
{
	HRESULT hr = S_OK;
	bool restricted = capacity == 0.0 && costPerDensity > 0.0;
	ArrayList<NAEdgePtr> * adj = nullptr;

	if (behindEdge && !restricted)
	{
		restricted = true;
		if (SUCCEEDED(hr = ecache->QueryAdjacencies(VertexAndRatio, leadingEdge, QueryDirection::Forward, &adj)))
		{
			for (const auto & currentEdge : *adj) if (NAEdge::IsEqualNAEdgePtr(behindEdge, currentEdge)) restricted = false;
		}
	}
	return restricted;
}

bool SafeZoneTable::insert(SafeZonePtr z)
{
	auto insertRet = std::unordered_map<long, SafeZonePtr>::insert(std::pair<long, SafeZonePtr>(z->VertexAndRatio->EID, z));
	if (!insertRet.second) delete z;
	return insertRet.second;
}

bool SafeZoneTable::CheckDiscoveredSafePoint(std::shared_ptr<NAEdgeCache> ecache, NAVertexPtr myVertex, NAEdgePtr myEdge, NAVertexPtr & finalVertex, double & TimeToBeat, SafeZonePtr & BetterSafeZone, double costPerDensity,
	double population2Route, EvcSolverMethod solverMethod, double & globalDeltaCost, bool & foundRestrictedSafezone) const
{
	bool found = false;
	auto i = find(myVertex->EID);

	if (i != end())
	{
		// Handle the last turn restriction here ... and the remaining capacity-aware cost.
		if (!i->second->IsRestricted(ecache, myEdge, costPerDensity))
		{
			double costLeft = i->second->SafeZoneCost(population2Route, solverMethod, costPerDensity, &globalDeltaCost);
			if (TimeToBeat > costLeft + myVertex->GVal + myVertex->GlobalPenaltyCost + globalDeltaCost)
			{
				BetterSafeZone = i->second;
				TimeToBeat = costLeft + myVertex->GVal + myVertex->GlobalPenaltyCost + globalDeltaCost;
				finalVertex = myVertex;
			}
		}
		else
		{
			// found a safe zone but it was restricted
			foundRestrictedSafezone = true;
		}
		found = true;
	}
	return found;
}
