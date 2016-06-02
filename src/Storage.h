﻿#ifndef STORAGE
#define STORAGE

#include <opencv2/core/core.hpp>

#include <vector>
#include <map>

namespace nuisken {
namespace storage {

class SpatioTemporalVolume {
   private:
    cv::Rect rect_;
    int startFrame_;
    int endFrame_;

   public:
    SpatioTemporalVolume(const cv::Rect& rect, int startFrame, int endFrame)
            : rect_(rect), startFrame_(startFrame), endFrame_(endFrame){};

    SpatioTemporalVolume(const cv::Point& topLeft, const cv::Point& bottomRight, int startFrame,
                         int endFrame)
            : rect_(topLeft, bottomRight), startFrame_(startFrame), endFrame_(endFrame){};

    SpatioTemporalVolume(int xCenter, int yCenter, int tCenter, int width, int height, int duration)
            : rect_(xCenter - width / 2, yCenter - height / 2, width, height),
              startFrame_(tCenter - duration / 2),
              endFrame_(tCenter + duration / 2) {}

    double computeVolume() const { return rect_.area() * (endFrame_ - startFrame_); }

    double computeOverlapRatio(const SpatioTemporalVolume& other) const {
        cv::Rect intersectRect = rect_ & other.rect_;
        int intersectStartFrame = std::max(startFrame_, other.startFrame_);
        int intersectEndFrame = std::min(endFrame_, other.endFrame_);
        SpatioTemporalVolume intersectVolume(intersectRect, intersectStartFrame, intersectEndFrame);
        return intersectVolume.computeVolume() /
               (computeVolume() + other.computeVolume() - intersectVolume.computeVolume());
    }
};

/**
 * 座標と値を持つクラス
 */
template <typename T>
class CoordinateValue {
   private:
    T point_;
    double value_;

   public:
    CoordinateValue(){};
    template <typename T>
    CoordinateValue(const T& point, double value)
            : point_(point), value_(value){};

    T getPoint() const { return point_; }

    double getValue() const { return value_; }

    void setPoint(const T& point) { point_ = point; }

    void setValue(double value) { value_ = value; }
};

/**
* 座標とスケールと値を持つクラス
*/
template <typename T>
class CoordinateValueScaleVer {
   private:
    T point_;
    double scale_;
    double value_;

   public:
    CoordinateValueScaleVer(){};
    template <typename T>
    CoordinateValueScaleVer(const T& point, double scale, double value)
            : point_(point), scale_(scale), value_(value){};

    T getPoint() const { return point_; }

    double getScale() const { return scale_; }

    double getValue() const { return value_; }

    void setPoint(const T& point) { point_ = point; }

    void setScale(double scale) { scale_ = scale; }

    void setValue(double value) { value_ = value; }
};

template <std::size_t DIM>
class VoteInfo {
   private:
    cv::Vec<float, DIM> votingPoint_;
    double weight_;
    int classLabel_;
    int index_;

   public:
    VoteInfo(){};

    VoteInfo(const cv::Vec<float, DIM>& votingPoint, double weight, int classLabel, int index)
            : votingPoint_(votingPoint), weight_(weight), classLabel_(classLabel), index_(index){};

    cv::Vec<float, DIM> getVotingPoint() const { return votingPoint_; }

    double getWeight() const { return weight_; }

    int getClassLabel() const { return classLabel_; }

    int getIndex() const { return index_; }

    void setVotingPoint(const cv::Vec<float, DIM>& votingPoint) { votingPoint_ = votingPoint; }

    void setWeight(double weight) { weight_ = weight; }

    void setClassLabel(int classLabel) { classLabel_ = classLabel; }

    void setIndex(int index) { index_ = index; }
};

class VoteInfoScaleVer {
   private:
    cv::Vec3f votingPoint_;
    double weight_;
    double scaleValue_;
    int classLabel_;
    int index_;

   public:
    VoteInfoScaleVer(){};
    VoteInfoScaleVer(const cv::Vec3f& votingPoint, double weight, double scaleValue, int classLabel,
                     int index)
            : votingPoint_(votingPoint),
              weight_(weight),
              scaleValue_(scaleValue),
              classLabel_(classLabel),
              index_(index){};

    cv::Vec3f getVotingPoint() const { return votingPoint_; }

    double getWeight() const { return weight_; }

    double getScaleValue() const { return scaleValue_; }

    int getClassLabel() const { return classLabel_; }

    int getIndex() const { return index_; }

    void setVotingPoint(const cv::Vec3f& votingPoint) { votingPoint_ = votingPoint; }

    void setWeight(double weight) { weight_ = weight; }

    void setScaleValue(double scaleValue) { scaleValue_ = scaleValue; }

    void setClassLabel(int classLabel) { classLabel_ = classLabel; }

    void setIndex(int index) { index_ = index; }
};

template <std::size_t DIM>
class FeatureVoteInfo {
   private:
    typedef VoteInfo<DIM> VoteInfo;

   private:
    cv::Vec3f featurePoint_;

    /**
    * 特徴点に対応する決定木ごとの投票
    */
    std::vector<std::vector<VoteInfo>> treeVotesInfo_;

   public:
    FeatureVoteInfo(const cv::Vec3f& featurePoint,
                    const std::vector<std::vector<VoteInfo>>& treeVotesInfo)
            : featurePoint_(featurePoint), treeVotesInfo_(treeVotesInfo){};

    cv::Vec3f getFeaturePoint() const { return featurePoint_; }

    std::vector<std::vector<VoteInfo>> getTreeVotesInfo() const { return treeVotesInfo_; }

    std::vector<VoteInfo> getTreeVotesInfo(int treeIndex) const {
        return treeVotesInfo_.at(treeIndex);
    }

    void setFeaturePoint(const cv::Vec3f& featurePoint) { featurePoint_ = featurePoint; }

    void setVotesInfo(const std::vector<std::vector<VoteInfo>>& treeVotesInfo) {
        treeVotesInfo_ = treeVotesInfo;
    }

    void setVoteWeight(int treeIndex, int voteIndex, double weight) {
        treeVotesInfo_.at(treeIndex).at(voteIndex).setWeight(weight);
    }
};

class FeatureVoteInfoScaleVer {
   private:
    cv::Vec3f featurePoint_;

    /**
    * 特徴点に対応する決定木ごとの投票
    */
    std::vector<std::vector<VoteInfoScaleVer>> treeVotesInfo_;

   public:
    FeatureVoteInfoScaleVer(const cv::Vec3f& featurePoint,
                            const std::vector<std::vector<VoteInfoScaleVer>>& treeVotesInfo)
            : featurePoint_(featurePoint), treeVotesInfo_(treeVotesInfo){};

    cv::Vec3f getFeaturePoint() const { return featurePoint_; }

    std::vector<std::vector<VoteInfoScaleVer>> getTreeVotesInfo() const { return treeVotesInfo_; }

    std::vector<VoteInfoScaleVer> getTreeVotesInfo(int treeIndex) const {
        return treeVotesInfo_.at(treeIndex);
    }

    void setFeaturePoint(const cv::Vec3f& featurePoint) { featurePoint_ = featurePoint; }

    void setVotesInfo(const std::vector<std::vector<VoteInfoScaleVer>>& treeVotesInfo) {
        treeVotesInfo_ = treeVotesInfo;
    }

    void setVoteWeight(int treeIndex, int voteIndex, double weight) {
        treeVotesInfo_.at(treeIndex).at(voteIndex).setWeight(weight);
    }
};

template <std::size_t DIM>
class VotesInfoMap {
   private:
    typedef FeatureVoteInfo<DIM> FeatureVoteInfo;
    typedef typename std::map<int, std::vector<FeatureVoteInfo>>::iterator Iterator;
    typedef typename std::map<int, std::vector<FeatureVoteInfo>>::const_iterator ConstIterator;

    std::map<int, std::vector<FeatureVoteInfo>> votesInfoMap_;

   public:
    VotesInfoMap(){};
    VotesInfoMap(const std::map<int, std::vector<FeatureVoteInfo>>& votesInfoMap)
            : votesInfoMap_(votesInfoMap){};

    std::vector<FeatureVoteInfo> getFeatureVotesInfo(int frame) const {
        return votesInfoMap_.at(frame);
    }

    void setFeatureVoteInfo(int frame, const FeatureVoteInfo& featureVoteInfo) {
        votesInfoMap_[frame].push_back(featureVoteInfo);
    }

    ConstIterator begin() const { return std::begin(votesInfoMap_); }

    Iterator begin() { return std::begin(votesInfoMap_); }

    ConstIterator end() const { return std::end(votesInfoMap_); }

    Iterator end() { return std::end(votesInfoMap_); }
};

class VotesInfoMapScaleVer {
   private:
    typedef std::map<int, std::vector<FeatureVoteInfoScaleVer>>::iterator Iterator;
    typedef std::map<int, std::vector<FeatureVoteInfoScaleVer>>::const_iterator ConstIterator;

    std::map<int, std::vector<FeatureVoteInfoScaleVer>> votesInfoMap_;

   public:
    VotesInfoMapScaleVer(){};
    VotesInfoMapScaleVer(const std::map<int, std::vector<FeatureVoteInfoScaleVer>>& votesInfoMap)
            : votesInfoMap_(votesInfoMap){};

    std::vector<FeatureVoteInfoScaleVer> getFeatureVotesInfo(int frame) const {
        return votesInfoMap_.at(frame);
    }

    void setFeatureVoteInfo(int frame, const FeatureVoteInfoScaleVer& featureVoteInfo) {
        votesInfoMap_[frame].push_back(featureVoteInfo);
    }

    ConstIterator begin() const { return std::begin(votesInfoMap_); }

    Iterator begin() { return std::begin(votesInfoMap_); }

    ConstIterator end() const { return std::end(votesInfoMap_); }

    Iterator end() { return std::end(votesInfoMap_); }

    int getSize() {
        int size = 0;
        for (const auto& featureVotesInfo : votesInfoMap_) {
            for (const auto& treeVotesInfo : featureVotesInfo.second) {
                for (const auto& votesInfo : treeVotesInfo.getTreeVotesInfo()) {
                    for (const auto& voteInfo : votesInfo) {
                        ++size;
                    }
                }
            }
        }
        return size;
    }
};

template <std::size_t DIM>
class DetectionResult {
   private:
    typedef CoordinateValue<cv::Vec<float, DIM>> LocalMaximum;
    typedef CoordinateValue<cv::Vec3f> ContributionPoint;

    LocalMaximum localMaximum_;
    std::vector<ContributionPoint> contributionPoints_;

   public:
    DetectionResult(){};
    DetectionResult(const LocalMaximum& localMaximum) : localMaximum_(localMaximum) {}
    DetectionResult(const LocalMaximum& localMaximum,
                    const std::vector<ContributionPoint>& contributionPoints)
            : localMaximum_(localMaximum), contributionPoints_(contributionPoints){};

    LocalMaximum getLocalMaximum() const { return localMaximum_; }

    std::vector<ContributionPoint> getContributionPoints() const { return contributionPoints_; }

    void setLocalMaximum(const LocalMaximum& localMaximum) { localMaximum_ = localMaximum; }

    void addContributionPoint(const ContributionPoint& contributionPoint) {
        contributionPoints_.push_back(contributionPoint);
    }

    void addContributionPoint(const cv::Vec3f& featurePoint, double contributionScore) {
        contributionPoints_.push_back(ContributionPoint(featurePoint, contributionScore));
    }
};

class FeatureInfo {
   private:
    int index_;
    int classLabel_;
    double spatialScale_;
    double temporalScale_;
    cv::Vec3i displacementVector_;

   public:
    FeatureInfo(){};
    FeatureInfo(int index, int classLabel, double spatialScale, double temporalScale,
                const cv::Vec3i& displacementVecotr)
            : index_(index),
              classLabel_(classLabel),
              spatialScale_(spatialScale),
              temporalScale_(temporalScale),
              displacementVector_(displacementVecotr){};

    int getIndex() const { return index_; }

    int getClassLabel() const { return classLabel_; }

    double getSpatialScale() const { return spatialScale_; }

    double getTemporalScale() const { return temporalScale_; }

    cv::Vec3i getDisplacementVector() const { return displacementVector_; }
};
}
}

#endif