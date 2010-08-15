/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#ifndef OMPL_GEOMETRIC_PLANNERS_SBL_SBL_
#define OMPL_GEOMETRIC_PLANNERS_SBL_SBL_

#include "ompl/geometric/planners/PlannerIncludes.h"
#include "ompl/base/ProjectionEvaluator.h"
#include "ompl/datastructures/Grid.h"
#include <vector>

namespace ompl
{

    namespace geometric
    {
	
	/**
	   @anchor gSBL
	   
	   @par Short description
	   
	   SBL is a tree-based motion planner that attempts to grow two
	   trees at once: one grows from the starting state and the other
	   from the goal state. Attempts are made to connect these trees
	   at every step of the expansion. If they are connected, a
	   solution path is obtained. However, this solution path is not
	   certain to be valid (the lazy part of the algorithm) so it is
	   checked for validity. If invalid parts are found, they are
	   removed from the tree and exploration of the state space
	   continues until a solution is found. 
	   
	   To guide the exploration, and additional grid data structure is
	   maintained. Grid cells contain states that have been previously
	   visited. When deciding which state to use for further
	   expansion, this grid is used and least filled grid cells have
	   most chances of being selected. The grid is usually imposed on
	   a projection of the state space. This projection needs to be
	   set before using the planner.
	   
	   @par External documentation
	   
	   G. Sanchez and J.C. Latombe.A Single-Query Bi-Directional
	   Probabilistic Roadmap Planner with Lazy Collision
	   Checking. Int. Symposium on Robotics Research (ISRR'01), Lorne,
	   Victoria, Australia, November 2001.
	*/

	/** \brief Single-Query Bi-Directional Probabilistic Roadmap
	   Planner with Lazy Collision Checking */
	class SBL : public base::Planner
	{
	public:
	    
	    /** \brief The constructor needs the instance of the space information */
	    SBL(const base::SpaceInformationPtr &si) : base::Planner(si, "SBL")
	    {
		type_ = base::PLAN_TO_GOAL_SAMPLEABLE_REGION;
		
		maxDistance_ = 0.0;
	    }
	    
	    virtual ~SBL(void)
	    {
		freeMemory();
	    }
	    
	    /** \brief Set the projection evaluator. This class is
		able to compute the projection of a given state. */
	    void setProjectionEvaluator(const base::ProjectionEvaluatorPtr &projectionEvaluator)
	    {
		projectionEvaluator_ = projectionEvaluator;
	    }

	    /** \brief Set the projection evaluator (select one from
		the ones registered with the state manifold). */
	    void setProjectionEvaluator(const std::string &name)
	    {
		projectionEvaluator_ = si_->getStateManifold()->getProjection(name);
	    }
	    
	    /** \brief Get the projection evaluator. */
	    const base::ProjectionEvaluatorPtr& getProjectionEvaluator(void) const
	    {
		return projectionEvaluator_;
	    }
	    	    
	    /** \brief Set the range the planner is supposed to use.

		This parameter greatly influences the runtime of the
		algorithm. It represents the maximum length of a
		motion to be added in the tree of motions. */
	    void setRange(double distance)
	    {
		maxDistance_ = distance;
	    }
	    
	    /** \brief Get the range the planner is using */
	    double getRange(void) const
	    {
		return maxDistance_;
	    }

	    virtual void setup(void);
	    
	    virtual bool solve(double solveTime);
	    virtual void clear(void);
	    
	    virtual void getPlannerData(base::PlannerData &data) const;
	    
	protected:
	    
	    class Motion;	    
	    
	    /** \brief An array of motions */
	    typedef std::vector<Motion*> MotionSet;	

	    /** \brief Representation of a motion */
	    class Motion
	    {
	    public:

		/** \brief Default constructor. Allocates no memory */
		Motion(void) : root(NULL), state(NULL), parent(NULL), valid(false)
		{
		}
		
		/** \brief Constructor that allocates storage for a state */
		Motion(const base::SpaceInformationPtr &si) : root(NULL), state(si->allocState()), parent(NULL), valid(false)
		{
		}
		
		~Motion(void)
		{
		}
		
		/** \brief The root of the tree this motion would get to, if we were to follow parent pointers */
		const base::State *root;
		/** \brief The state this motion leads to */
		base::State       *state;

		/** \brief The parent motion -- it contains the state this motion originates at */
		Motion            *parent;
		
		/** \brief Flag indicating whether this motion has been checked for validity. */
		bool               valid;
		
		/** \brief The set of motions descending from the current motion */
		MotionSet          children;
	    };
	    
	    /** \brief Representation of a search tree. Two instances will be used. One for start and one for goal */
	    struct TreeData
	    {
		TreeData(void) : grid(0), size(0)
		{
		}
		
		/** \brief The grid of motions corresponding to this tree */
		Grid<MotionSet> grid;

		/** \brief The number of motions (in total) from the tree */
		unsigned int    size;
	    };
	    
	    /** \brief Free the memory allocated by the planner */
	    void freeMemory(void)
	    {
		freeGridMotions(tStart_.grid);
		freeGridMotions(tGoal_.grid);
	    }
	    
	    /** \brief Free the memory used by the motions contained in a grid */
	    void freeGridMotions(Grid<MotionSet> &grid);
	    
	    /** \brief Add a motion to a tree */
	    void addMotion(TreeData &tree, Motion *motion);

	    /** \brief Select a motion from a tree */
	    Motion* selectMotion(TreeData &tree);

	    /** \brief Remove a motion from a tree */
	    void removeMotion(TreeData &tree, Motion *motion);

	    /** \brief Since solutions are computed in a lazy fashion,
		once trees are connected, the solution found needs to
		be checked for validity. This function checks whether
		the reverse path from a given motion to a root is
		valid */
	    bool isPathValid(TreeData &tree, Motion *motion);

	    /** \brief Check if a solution can be obtained by connecting two trees using a specified motion */
	    bool checkSolution(bool start, TreeData &tree, TreeData &otherTree, Motion *motion, std::vector<Motion*> &solution);
	    
	    /** \brief The employed state sampler */
	    base::StateSamplerPtr                      sampler_;
	    
	    /** \brief The employed projection evaluator */
	    base::ProjectionEvaluatorPtr               projectionEvaluator_;
	    
	    /** \brief The start tree */
	    TreeData                                   tStart_;

	    /** \brief The goal tree */
	    TreeData                                   tGoal_;

	    /** \brief The maximum length of a motion to be added in the tree */
	    double                                     maxDistance_;

	    /** \brief The random number generator to be used */
	    RNG                                        rng_;	
	};
	
    }
}

#endif
    
