#ifndef VOTING_SPACE
#define VOTING_SPACE

#include <opencv2/core/core.hpp>

#include <vector>
#include <array>
#include <unordered_map>

namespace nuisken {
namespace houghforests {

class VotingSpace {
   private:
    std::unordered_map<std::size_t, float> votes_;
    std::vector<double> scales_;
    std::size_t width_;
    std::size_t height_;
    std::size_t nScales_;
    std::size_t maxT_;
    std::size_t minT_;
    std::size_t deleteStep_;

   public:
    VotingSpace(std::size_t width, std::size_t height, std::size_t nScales,
                const std::vector<double>& scales, std::size_t deleteStep, std::size_t bufferLength)
            : width_(width),
              height_(height),
              nScales_(nScales),
              scales_(scales),
              maxT_(bufferLength),
              minT_(0),
              deleteStep_(deleteStep){};
    ~VotingSpace(){};

    void addInput(const cv::Vec3i& point, std::size_t scaleIndex, float weight);
    void deleteOldVotes();
    void getVotes(std::vector<std::array<float, 4>>& votingPoints, std::vector<float>& weights,
                  int startT, int endT) const;

    void computePointAndScale(std::size_t index, cv::Vec3i& point, std::size_t& scaleIndex) const;
    std::array<float, 4> computePoint(std::size_t index) const;
    std::size_t computeIndex(const cv::Vec3i& point, std::size_t scaleIndex) const;

    std::size_t getWidth() const { return width_; }

    std::size_t getHeight() const { return height_; }

    // std::array<float, DIMENSION_SIZE_> convertFromVecToArray(const cv::Vec4f& point) const;
    // cv::Vec4f convertFromArrayToVec(const std::array<float, DIMENSION_SIZE_>& point) const;
};
}
}

#endif