/*
 * vim: ts=4 sw=4 et tw=0 wm=0
 *
 * libcola - A library providing force-directed network layout using the 
 *           stress-majorization method subject to separation constraints.
 *
 * Copyright (C) 2006-2008  Monash University
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library in the file LICENSE; if not, 
 * write to the Free Software Foundation, Inc., 59 Temple Place, 
 * Suite 330, Boston, MA  02111-1307  USA
 *
*/

#include "libvpsc/assertions.h"
#include "commondefs.h"
#include "cola.h"
#include "convex_hull.h"
#include "cluster.h"

using vpsc::generateXConstraints;
using vpsc::generateYConstraints;

namespace cola {
    using namespace std;

    Cluster::Cluster()
        : varWeight(0.0001), 
          internalEdgeWeightFactor(1.), 
          bounds(-1,1,-1,1),
          desiredBoundsSet(false), 
          desiredBounds(-1,1,-1,1),
          border(7)
      {}
	void Cluster::setDesiredBounds(const vpsc::Rectangle db) {
		desiredBoundsSet=true;
		desiredBounds=db;
	}
	void Cluster::unsetDesiredBounds() {
		desiredBoundsSet=false;
	}
    void Cluster::computeBoundingRect(const vpsc::Rectangles& rs) {
        double minX=DBL_MAX, maxX=-DBL_MAX, minY=DBL_MAX, maxY=-DBL_MAX;
        for(vector<Cluster*>::const_iterator i=clusters.begin(); i!=clusters.end(); i++) {
            (*i)->computeBoundingRect(rs);
            vpsc::Rectangle r=(*i)->bounds;
            minX=min(r.getMinX(),minX);
            maxX=max(r.getMaxX(),maxX);
            minY=min(r.getMinY(),minY);
            maxY=max(r.getMaxY(),maxY);
        }
        for(vector<unsigned>::const_iterator i=nodes.begin(); i!=nodes.end(); i++) {
            vpsc::Rectangle* r=rs[*i];
            minX=min(r->getMinX(),minX);
            maxX=max(r->getMaxX(),maxX);
            minY=min(r->getMinY(),minY);
            maxY=max(r->getMaxY(),maxY);
        }
        bounds=vpsc::Rectangle(minX,maxX,minY,maxY);
    }

	void ConvexCluster::computeBoundary(const vpsc::Rectangles& rs) {
        unsigned n=4*nodes.size();
        valarray<double> X(n);
        valarray<double> Y(n);
        unsigned pctr=0;
        for(vector<unsigned>::const_iterator i=nodes.begin(); i!=nodes.end(); i++) {
            vpsc::Rectangle* r=rs[*i];
            // Bottom Right
            X[pctr]=r->getMaxX();
            Y[pctr++]=r->getMinY();
            // Top Right
            X[pctr]=r->getMaxX();
            Y[pctr++]=r->getMaxY();
            // Top Left
            X[pctr]=r->getMinX();
            Y[pctr++]=r->getMaxY();
            // Bottom Left
            X[pctr]=r->getMinX();
            Y[pctr++]=r->getMinY();
        }
        /*
        for(unsigned i=0;i<n;i++) {
            printf("X[%d]=%f, Y[%d]=%f;\n",i,X[i],i,Y[i]);
        }
        */
        vector<unsigned> hull;
        hull::convex(X,Y,hull);
        hullX.resize(hull.size());
        hullY.resize(hull.size());
        hullRIDs.resize(hull.size());
        hullCorners.resize(hull.size());
        for(unsigned j=0;j<hull.size();j++) {
            hullX[j]=X[hull[j]];
            hullY[j]=Y[hull[j]];
            hullRIDs[j]=hull[j]/4;
            hullCorners[j]=hull[j]%4;
        }
    }
    void RectangularCluster::computeBoundary(const vpsc::Rectangles& rs) {
        double xMin=DBL_MAX, xMax=-DBL_MAX, yMin=DBL_MAX, yMax=-DBL_MAX;
        for(unsigned i=0;i<nodes.size();i++) {
            xMin=std::min(xMin,rs[nodes[i]]->getMinX());
            xMax=std::max(xMax,rs[nodes[i]]->getMaxX());
            yMin=std::min(yMin,rs[nodes[i]]->getMinY());
            yMax=std::max(yMax,rs[nodes[i]]->getMaxY());
        }
        hullX.resize(4);
        hullY.resize(4);
        hullX[3]=xMin;
        hullY[3]=yMin;
        hullX[2]=xMin;
        hullY[2]=yMax;
        hullX[1]=xMax;
        hullY[1]=yMax;
        hullX[0]=xMax;
        hullY[0]=yMin;
    }
    void RootCluster::computeBoundary(const vpsc::Rectangles& rs) {
        for(unsigned i=0;i<clusters.size();i++) {
            clusters[i]->computeBoundary(rs);
        }
    }
	void Cluster::updateBounds(const vpsc::Dim dim) {
		if(dim==vpsc::HORIZONTAL) {
			bounds=vpsc::Rectangle(vMin->finalPosition,vMax->finalPosition,bounds.getMinY(),bounds.getMaxY());
		} else {
			bounds=vpsc::Rectangle(bounds.getMinX(),bounds.getMaxX(),vMin->finalPosition,vMax->finalPosition);
		}
        for(unsigned i=0;i<clusters.size();i++) {
            clusters[i]->updateBounds(dim);
        }
	}
	vpsc::Rectangle Cluster::getMinRect( const vpsc::Dim dim, const vpsc::Rectangle& bounds) {
        if(dim==vpsc::HORIZONTAL) {
            length=bounds.width();
            vMin=vXMin;
            vMin->desiredPosition=bounds.getMinX();
            return vpsc::Rectangle(bounds.getMinX()-border,
                             bounds.getMinX()+border,
                             bounds.getMinY(),bounds.getMaxY());
        } else {
            length=bounds.height();
            vMin=vYMin;
            vMin->desiredPosition=bounds.getMinY();
            return vpsc::Rectangle(bounds.getMinX(),bounds.getMaxX(),
                             bounds.getMinY()-border,
                             bounds.getMinY()+border);
        }
    }
	vpsc::Rectangle Cluster::getMaxRect( const vpsc::Dim dim, vpsc::Rectangle const & bounds) {
        if(dim==vpsc::HORIZONTAL) {
            vMax=vXMax;
            vMax->desiredPosition=bounds.getMaxX();
            return vpsc::Rectangle(bounds.getMaxX()-border, bounds.getMaxX()+border,
                             bounds.getMinY(), bounds.getMaxY());
        } else {
            vMax=vYMax;
            vMax->desiredPosition=bounds.getMaxY();
            return vpsc::Rectangle(bounds.getMinX(), bounds.getMaxX(),
                             bounds.getMaxY()-border, bounds.getMaxY()+border);
        }
    }
    void Cluster::createVars(
			const vpsc::Dim dim,
			const vpsc::Rectangles& rs, 
			vpsc::Variables& vars) {
        ASSERT(clusters.size()>0||nodes.size()>0);
        for(vector<Cluster*>::iterator i=clusters.begin();i!=clusters.end();i++) {
            (*i)->createVars(dim,rs,vars);
        }
        if(dim==vpsc::HORIZONTAL) {
			double desiredMinX = bounds.getMinX(), desiredMaxX = bounds.getMaxX();
			if(desiredBoundsSet) {
				desiredMinX = desiredBounds.getMinX();
				desiredMaxX = desiredBounds.getMaxX();
			}
            vars.push_back(vXMin=new vpsc::Variable(
                        vars.size(),desiredMinX,varWeight));
            vars.push_back(vXMax=new vpsc::Variable(
                        vars.size(),desiredMaxX,varWeight));
        } else {
			double desiredMinY = bounds.getMinY(), desiredMaxY = bounds.getMaxY();
			if(desiredBoundsSet) {
				desiredMinY = desiredBounds.getMinY();
				desiredMaxY = desiredBounds.getMaxY();
			}
            vars.push_back(vYMin=new vpsc::Variable(
                        vars.size(),desiredMinY,varWeight));
            vars.push_back(vYMax=new vpsc::Variable(
                        vars.size(),desiredMaxY,varWeight));
        }
    }
    void Cluster::generateNonOverlapConstraints(
			const vpsc::Dim dim,
            const NonOverlapConstraints nonOverlapConstraints,
			const vpsc::Rectangles& rs,
			const vpsc::Variables& vars,
            vpsc::Constraints& cs) {
        ASSERT(clusters.size()>0||nodes.size()>0);
        for(unsigned i=0;i<clusters.size();i++) {
            Cluster* c=clusters[i];
            c->generateNonOverlapConstraints(dim,nonOverlapConstraints,rs,vars,cs);
        }
        // n is the number of dummy vars and rectangles that need to be
        // considered in generating non-overlap constraints within this
        // cluster.
        // One var/rect for each node, one for each child cluster, one for
        // the LHS of this cluster and one for the RHS.
        unsigned n=nodes.size()+clusters.size()+2;
        vpsc::Variables lvs(n);
        vpsc::Rectangles lrs(n);
        unsigned vctr=0;
        for(vector<unsigned>::iterator i=nodes.begin();i!=nodes.end();i++) {
            lvs[vctr]=vars[*i];
            lrs[vctr]=rs[*i];
            //printf("  adding var %d, w=%f, h=%f\n",*i,rs[*i]->width(),rs[*i]->height());
            vctr++;
        }
        map<vpsc::Variable*, Cluster*> varClusterMap;
        for(vector<Cluster*>::iterator i=clusters.begin();i!=clusters.end();i++) {
            Cluster* c=*i;
            lvs[vctr]=c->vMin;
            varClusterMap[c->vMin]=c;
            lrs[vctr]=&c->bounds;
            //printf("  adding cluster %d, w=%f, h=%f\n",c->vMin->id,lrs[vctr]->width(),lrs[vctr]->height());
            vctr++;
        }
        vpsc::Rectangle rMin=getMinRect(dim,bounds), 
                  rMax=getMaxRect(dim,bounds);
        lvs[vctr]=vMin;
        lrs[vctr++]=&rMin;
        lvs[vctr]=vMax;
        lrs[vctr++]=&rMax;

        //printf("Processing cluster: vars=%d,%d length=%f\n",vMin->id,vMax->id,length);
        vector<vpsc::Constraint*> tmp_cs;
        double hAdjust=0;
        if(dim==vpsc::HORIZONTAL) {
            hAdjust=1;
            vpsc::Rectangle::setXBorder(0.001);
            // use rs->size() rather than n because some of the variables may
            // be dummy vars with no corresponding rectangle
            generateXConstraints(lrs,lvs,tmp_cs,nonOverlapConstraints==Both?true:false); 
            vpsc::Rectangle::setXBorder(0);
        } else {
            generateYConstraints(lrs,lvs,tmp_cs); 
        }
        for(unsigned i=0;i<tmp_cs.size();i++) {
            vpsc::Constraint* co = tmp_cs[i];
            // need to remap outgoing constraints of each cluster to maxVar of
            // cluster.
            map<vpsc::Variable*, Cluster*>::iterator f=varClusterMap.find(co->left);
            //std::cout << *co << std::endl;
            if(f!=varClusterMap.end()) {
                Cluster* cl=f->second;
                co->left=cl->vMax;
                co->gap-=cl->length/2.-hAdjust;
                //std::cout << "modified "<<*co << std::endl;
            }
            f=varClusterMap.find(co->right);
            if(f!=varClusterMap.end()) {
                Cluster* cl=f->second;
                co->gap-=cl->length/2.-hAdjust;
                //std::cout << "modified "<<*co << std::endl;
            }
            cs.push_back(co);
        }
    } 

    /** recursively delete all clusters */
    void Cluster::clear() {
        for_each(clusters.begin(),clusters.end(),delete_object());
        clusters.clear();
    }
    /**
     * @return the total area covered by contents of this cluster (not
     * including space between nodes/clusters)
     */
    double Cluster::area(const vpsc::Rectangles& rs) {
        double a=0;
        for(vector<unsigned>::iterator i=nodes.begin();i!=nodes.end();++i) {
            vpsc::Rectangle* r=rs[*i];
            a+=r->width()*r->height();
        }
        for(Clusters::iterator i=clusters.begin();i!=clusters.end();++i) {
            a+=(*i)->area(rs);
        }
        return a;
    }

} // namespace cola
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
