#include "VotingSpace.h"
#include "Utils.h"

#include <iostream>

namespace nuisken {
namespace houghforests {

void VotingSpace::inputVote(const cv::Vec3i& point, std::size_t scaleIndex, float weight) {
    cv::Vec3i discretizedPoint = discretizePoint(point);
    if (discretizedPoint(T) < static_cast<long long>(minT_) || discretizedPoint(X) < 0 ||
        discretizedPoint(X) >= width_ || discretizedPoint(Y) < 0 ||
        discretizedPoint(Y) >= height_ || scaleIndex < 0 || scaleIndex >= nScales_) {
        return;
    }
    std::size_t index = computeIndex(discretizedPoint, scaleIndex);
    votes_[index] += weight;
}

void VotingSpace::deleteOldVotes() {
    int deleteEndT = minT_ + deleteStep_;
    std::size_t beginIndex = computeIndex(cv::Vec3i(minT_, 0, 0), 0);
    std::size_t endIndex = computeIndex(cv::Vec3i(deleteEndT, 0, 0), 0);
    for (auto it = std::cbegin(votes_); it != std::cend(votes_);) {
        if (it->first >= beginIndex && it->first < endIndex) {
            votes_.erase(it++);
        } else {
            ++it;
        }
    }

    minT_ += deleteStep_;
    maxT_ += deleteStep_;
}

void VotingSpace::getVotes(std::vector<std::array<float, 4>>& votingPoints,
                           std::vector<float>& weights, int beginT, int endT) const {
    int beginIndex = computeIndex(cv::Vec3i(beginT, 0, 0), 0);
    int endIndex = computeIndex(cv::Vec3i(endT, 0, 0), 0);
    for (const auto& vote : votes_) {
        if (vote.first >= beginIndex && vote.first < endIndex) {
            votingPoints.push_back(computePoint(vote.first));
            weights.push_back(vote.second);
        }
    }
}

std::size_t VotingSpace::discretizePoint(std::size_t originalPoint) const {
    return originalPoint * discretizeRatio_;
}

std::size_t VotingSpace::calculateOriginalPoint(std::size_t discretizedPoint) const {
    return discretizedPoint / discretizeRatio_;
}

cv::Vec3i VotingSpace::discretizePoint(const cv::Vec3i& originalPoint) const {
    return originalPoint * discretizeRatio_;
}

cv::Vec3i VotingSpace::calculateOriginalPoint(const cv::Vec3i& discretizedPoint) const {
    return discretizedPoint / discretizeRatio_;
}

std::size_t VotingSpace::computeIndex(const cv::Vec3i& point, std::size_t scaleIndex) const {
    std::size_t index = (point(0) * (height_ * width_ * nScales_)) +
                        (point(1) * (width_ * nScales_)) + (point(2) * nScales_) + scaleIndex;
    return index;
}

void VotingSpace::computePointAndScale(std::size_t index, cv::Vec3i& point,
                                       std::size_t& scaleIndex) const {
    std::size_t t = index / (height_ * width_ * nScales_);
    std::size_t y = index / (width_ * nScales_) % height_;
    std::size_t x = index / nScales_ % width_;
    point = cv::Vec3i(t, y, x);

    scaleIndex = index % nScales_;
}

std::array<float, 4> VotingSpace::computePoint(std::size_t index) const {
    std::size_t t = index / (height_ * width_ * nScales_);
    std::size_t y = index / (width_ * nScales_) % height_;
    std::size_t x = index / nScales_ % width_;

    std::size_t scaleIndex = index % nScales_;

    std::array<float, 4> point = {t, y, x, scales_.at(scaleIndex)};
    return point;
}
}
}