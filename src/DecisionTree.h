﻿#ifndef DECISION_TREE
#define DECISION_TREE

#include "TreeNode.hpp"
#include "TreeParameters.h"

#include <opencv2/core/core.hpp>

#include <fstream>
#include <map>
#include <memory>

namespace nuisken {
namespace randomforests {

template <class Type>
class DecisionTree {
   private:
    using FeatureRawPtr = typename Type::FeatureType*;
    using LeafPtr = std::shared_ptr<typename Type::LeafType>;

   private:
    Type type;

    TreeParameters parameters;

    std::unique_ptr<TreeNode<Type>> root;

    std::map<int, int> leafIndices;

   public:
    DecisionTree(){};

    DecisionTree(const Type& type, const TreeParameters& parameters)
            : type(type), parameters(parameters){};

    DecisionTree(DecisionTree<Type>&& other) {
        type = other.type;
        parameters = other.parameters;
        root = std::move(other.root);
        leafIndices = other.leafIndices;
    }

    void setParameters(const TreeParameters& parameters) { this->parameters = parameters; }

    LeafPtr match(const FeatureRawPtr& feature) const { return root->match(feature); };

    int getNumberOfLeaves() const;

    void setType(const Type& type) { this->type = type; }

    void grow(const std::vector<FeatureRawPtr>& features);

    void mapLeafIndices();

    void save(std::ofstream& treeStream) const;
    void load(std::ifstream& treeStream);

   private:
    void trainNode(std::unique_ptr<TreeNode<Type>>& node,
                   const std::vector<FeatureRawPtr>& trainingData, const TreeParameters& parameters,
                   int& nodeIndex);

    int getNumberOfLeaves(const std::unique_ptr<TreeNode<Type>>& node, int numberOfLeaves) const;

    void mapLeafIndices(const std::unique_ptr<TreeNode<Type>>& node, int& leafIndex);

    void numberNodes();
    void numberNodes(std::unique_ptr<TreeNode<Type>>& node, int& nodeIndex);
};
}
}

#endif