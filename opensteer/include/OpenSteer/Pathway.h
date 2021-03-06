// ----------------------------------------------------------------------------
//
//
// OpenSteer -- Steering Behaviors for Autonomous Characters
//
// Copyright (c) 2002-2003, Sony Computer Entertainment America
// Original author: Craig Reynolds <craig_reynolds@playstation.sony.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
//
// ----------------------------------------------------------------------------
//
//
// Pathway and PolylinePathway, for path following.
//
// 10-04-04 bk:  put everything into the OpenSteer namespace
// 06-03-02 cwr: created
//
//
// ----------------------------------------------------------------------------


#ifndef OPENSTEER_PATHWAY_H
#define OPENSTEER_PATHWAY_H


#include "Vec3.h"


namespace OpenSteer {

    // ----------------------------------------------------------------------------
    // Pathway: a pure virtual base class for an abstract pathway in space, as for
    // example would be used in path following.


    class Pathway
    {
    public:
        // Given an arbitrary point ("A"), returns the nearest point ("P") on
        // this path.  Also returns, via output arguments, the path tangent at
        // P and a measure of how far A is outside the Pathway's "tube".  Note
        // that a negative distance indicates A is inside the Pathway.
		OPENSTEER_API virtual Vec3 mapPointToPath(const Vec3& point,
                                     Vec3& tangent,
                                     double& outside) = 0;

        // given a distance along the path, convert it to a point on the path
		OPENSTEER_API virtual Vec3 mapPathDistanceToPoint(double pathDistance) = 0;

        // Given an arbitrary point, convert it to a distance along the path.
		OPENSTEER_API virtual double mapPointToPathDistance(const Vec3& point) = 0;

        // is the given point inside the path tube?
		OPENSTEER_API bool isInsidePath(const Vec3& point)
        {
            double outside; Vec3 tangent;
            mapPointToPath (point, tangent, outside);
            return outside < 0;
        }

        // how far outside path tube is the given point?  (negative is inside)
		OPENSTEER_API double howFarOutsidePath(const Vec3& point)
        {
            double outside; Vec3 tangent;
            mapPointToPath (point, tangent, outside);
            return outside;
        }
    };


    // ----------------------------------------------------------------------------
    // PolylinePathway: a simple implementation of the Pathway protocol.  The path
    // is a "polyline" a series of line segments between specified points.  A
    // radius defines a volume for the path which is the union of a sphere at each
    // point and a cylinder along each segment.


    class PolylinePathway: public virtual Pathway
    {
	private:
		bool isInit;
		OPENSTEER_API void Destruct(void);

    public:

        int pointCount;
        Vec3* points;
        double radius;
        bool cyclic;

		OPENSTEER_API PolylinePathway(void) { isInit = false; }
		OPENSTEER_API virtual ~PolylinePathway(void) { Destruct(); }

        // construct a PolylinePathway given the number of points (vertices),
        // an array of points, and a path radius.
		OPENSTEER_API PolylinePathway(const int _pointCount,
                         const Vec3 _points[],
                         const double _radius,
                         const bool _cyclic);

        // utility for constructors in derived classes
		OPENSTEER_API void initialize(const int _pointCount,
                         const Vec3 _points[],
                         const double _radius,
                         const bool _cyclic);

        // Given an arbitrary point ("A"), returns the nearest point ("P") on
        // this path.  Also returns, via output arguments, the path tangent at
        // P and a measure of how far A is outside the Pathway's "tube".  Note
        // that a negative distance indicates A is inside the Pathway.
		OPENSTEER_API Vec3 mapPointToPath(const Vec3& point, Vec3& tangent, double& outside);


        // given an arbitrary point, convert it to a distance along the path
		OPENSTEER_API double mapPointToPathDistance(const Vec3& point);

        // given a distance along the path, convert it to a point on the path
		OPENSTEER_API Vec3 mapPathDistanceToPoint(double pathDistance);

        // utility methods

        // compute minimum distance from a point to a line segment
		OPENSTEER_API double pointToSegmentDistance(const Vec3& point,
                                      const Vec3& ep0,
                                      const Vec3& ep1);

        // assessor for total path length;
		OPENSTEER_API double getTotalPathLength(void) { return totalPathLength; };

    // XXX removed the "private" because it interfered with derived
    // XXX classes later this should all be rewritten and cleaned up
    // private:

        // xxx shouldn't these 5 just be local variables?
        // xxx or are they used to pass secret messages between calls?
        // xxx seems like a bad design
        double segmentLength;
        double segmentProjection;
        Vec3 local;
        Vec3 chosen;
        Vec3 segmentNormal;

        double* lengths;
        Vec3* normals;
        double totalPathLength;
    };

} // namespace OpenSteer


// ----------------------------------------------------------------------------
#endif // OPENSTEER_PATHWAY_H
