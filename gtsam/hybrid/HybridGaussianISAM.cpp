/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file HybridGaussianISAM.h
 * @date March 31, 2022
 * @author Fan Jiang
 * @author Frank Dellaert
 * @author Varun Agrawal
 */

#include <gtsam/base/treeTraversal-inst.h>
#include <gtsam/hybrid/HybridBayesTree.h>
#include <gtsam/hybrid/HybridGaussianFactorGraph.h>
#include <gtsam/hybrid/HybridGaussianISAM.h>
#include <gtsam/inference/ISAM-inst.h>
#include <gtsam/inference/Key.h>

#include <iterator>

namespace gtsam {

// Instantiate base class
// template class ISAM<HybridBayesTree>;

/* ************************************************************************* */
HybridGaussianISAM::HybridGaussianISAM() {}

/* ************************************************************************* */
HybridGaussianISAM::HybridGaussianISAM(const HybridBayesTree& bayesTree)
    : Base(bayesTree) {}

/* ************************************************************************* */
void HybridGaussianISAM::updateInternal(
    const HybridGaussianFactorGraph& newFactors,
    HybridBayesTree::Cliques* orphans,
    const boost::optional<size_t>& maxNrLeaves,
    const boost::optional<Ordering>& ordering,
    const HybridBayesTree::Eliminate& function) {
  // Remove the contaminated part of the Bayes tree
  BayesNetType bn;
  const KeySet newFactorKeys = newFactors.keys();
  if (!this->empty()) {
    KeyVector keyVector(newFactorKeys.begin(), newFactorKeys.end());
    this->removeTop(keyVector, &bn, orphans);
  }

  // Add the removed top and the new factors
  FactorGraphType factors;
  factors += bn;
  factors += newFactors;

  // Add the orphaned subtrees
  for (const sharedClique& orphan : *orphans)
    factors += boost::make_shared<BayesTreeOrphanWrapper<Node> >(orphan);

  // Get all the discrete keys from the factors
  KeySet allDiscrete = factors.discreteKeys();

  // Create KeyVector with continuous keys followed by discrete keys.
  KeyVector newKeysDiscreteLast;
  // Insert continuous keys first.
  for (auto& k : newFactorKeys) {
    if (!allDiscrete.exists(k)) {
      newKeysDiscreteLast.push_back(k);
    }
  }
  // Insert discrete keys at the end
  std::copy(allDiscrete.begin(), allDiscrete.end(),
            std::back_inserter(newKeysDiscreteLast));

  // Get an ordering where the new keys are eliminated last
  const VariableIndex index(factors);

  Ordering elimination_ordering;
  if (ordering) {
    elimination_ordering = *ordering;
  } else {
    elimination_ordering = Ordering::ColamdConstrainedLast(
        index,
        KeyVector(newKeysDiscreteLast.begin(), newKeysDiscreteLast.end()),
        true);
  }

  // eliminate all factors (top, added, orphans) into a new Bayes tree
  HybridBayesTree::shared_ptr bayesTree =
      factors.eliminateMultifrontal(elimination_ordering, function, index);

  if (maxNrLeaves) {
    bayesTree->prune(*maxNrLeaves);
  }

  // Re-add into Bayes tree data structures
  this->roots_.insert(this->roots_.end(), bayesTree->roots().begin(),
                      bayesTree->roots().end());
  this->nodes_.insert(bayesTree->nodes().begin(), bayesTree->nodes().end());
}

/* ************************************************************************* */
void HybridGaussianISAM::update(const HybridGaussianFactorGraph& newFactors,
                                const boost::optional<size_t>& maxNrLeaves,
                                const boost::optional<Ordering>& ordering,
                                const HybridBayesTree::Eliminate& function) {
  Cliques orphans;
  this->updateInternal(newFactors, &orphans, maxNrLeaves, ordering, function);
}

}  // namespace gtsam
