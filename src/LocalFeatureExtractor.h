#ifndef LOCAL_FEATURE_EXTRACTOR
#define LOCAL_FEATURE_EXTRACTOR

#include "hog.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui.hpp>

#include <vector>

namespace nuisken {
namespace houghforests {

class LocalFeatureExtractor {
   public:
    enum Axis { X = 2, Y = 1, T = 0 };
    enum PoolingType { AVERAGE, MAX };
    static const int N_CHANNELS_;
    static const int N_HOG_BINS_;

   private:
    using Descriptor = std::vector<float>;
    using Feature = std::vector<float>;
    using MultiChannelFeature = std::vector<Feature>;
    using Video = std::vector<cv::Mat1b>;
    using ColorVideo = std::vector<cv::Mat3b>;

    cv::VideoCapture videoCapture_;
    Video colorVideo_;
    std::vector<Video> scaleVideos_;
    std::vector<MultiChannelFeature> scaleChannelFeatures_;
    std::vector<double> scales_;
    int localWidth_;
    int localHeight_;
    int localDuration_;
    int xBlockSize_;
    int yBlockSize_;
    int tBlockSize_;
    int xStep_;
    int yStep_;
    int tStep_;
    int width_;
    int height_;
    std::size_t storedColorVideoBeginT_;
    std::size_t storedFeatureBeginT_;
    int nStoredFeatureFrames_;
    bool isEnded_;

    HoG hog_;

   public:
    LocalFeatureExtractor(){};

    LocalFeatureExtractor(const std::string& videoFilePath, std::vector<double> scales,
                          int localWidth, int localHeight, int localDuration, int xBlockSize,
                          int yBlockSize, int tBlockSize, int xStep, int yStep, int tStep)
            : videoCapture_(videoFilePath),
              scaleVideos_(scales.size()),
              scaleChannelFeatures_(scales.size(), MultiChannelFeature(N_CHANNELS_)),
              scales_(scales),
              localWidth_(localWidth),
              localHeight_(localHeight),
              localDuration_(localDuration),
              xBlockSize_(xBlockSize),
              yBlockSize_(yBlockSize),
              tBlockSize_(tBlockSize),
              xStep_(xStep),
              yStep_(yStep),
              tStep_(tStep),
              storedColorVideoBeginT_(0),
              storedFeatureBeginT_(0),
              nStoredFeatureFrames_(0),
              isEnded_(false) {
        makeLocalSizeOdd(localWidth_);
        makeLocalSizeOdd(localHeight_);
        makeLocalSizeOdd(localDuration_);
    }

    LocalFeatureExtractor(const cv::VideoCapture& videoCapture, std::vector<double> scales,
                          int localWidth, int localHeight, int localDuration, int xBlockSize,
                          int yBlockSize, int tBlockSize, int xStep, int yStep, int tStep)
            : videoCapture_(videoCapture),
              scaleVideos_(scales.size()),
              scaleChannelFeatures_(scales.size(), MultiChannelFeature(N_CHANNELS_)),
              scales_(scales),
              localWidth_(localWidth),
              localHeight_(localHeight),
              localDuration_(localDuration),
              xBlockSize_(xBlockSize),
              yBlockSize_(yBlockSize),
              tBlockSize_(tBlockSize),
              xStep_(xStep),
              yStep_(yStep),
              tStep_(tStep),
              storedColorVideoBeginT_(0),
              storedFeatureBeginT_(0),
              nStoredFeatureFrames_(0),
              isEnded_(false) {
        makeLocalSizeOdd(localWidth_);
        makeLocalSizeOdd(localHeight_);
        makeLocalSizeOdd(localDuration_);
    }

    void extractLocalFeatures(std::vector<std::vector<cv::Vec3i>>& scalePoints,
                              std::vector<std::vector<Descriptor>>& scaleDescriptors);
    void extractLocalFeatures(std::vector<std::vector<cv::Vec3i>>& scalePoints,
                              std::vector<std::vector<Descriptor>>& scaleDescriptors,
                              ColorVideo& usedVideo, std::size_t& usedVideoBeginT);

    int getFPS() const { return videoCapture_.get(cv::CAP_PROP_FPS); }
    bool isEnded() const { return isEnded_; }
    std::size_t getStoredColorVideoBeginT() const { return storedColorVideoBeginT_; }
    std::size_t getStoredFeatureBeginT() const { return storedFeatureBeginT_; }

    void visualizeDenseFeature(const std::vector<cv::Vec3i>& points,
                               const std::vector<Descriptor>& features, int width, int height,
                               int duration) const;
    void visualizeDenseFeature(const Descriptor& features) const;
    void visualizePooledDenseFeature(const std::vector<cv::Vec3i>& points,
                                     const std::vector<Descriptor>& features) const;
    void visualizePooledDenseFeature(const Descriptor& feature) const;

   private:
    void makeLocalSizeOdd(int& size) const;
    void readOriginalScaleVideo();
    void generateScaledVideos();
    void denseSampling(int scaleIndex, std::vector<cv::Vec3i>& points,
                       std::vector<Descriptor>& descriptors) const;
    void denseSamplingHOG(int scaleIndex, std::vector<cv::Vec3i>& points,
                          std::vector<Descriptor>& descriptors) const;
    void deleteOldData();

    void extractFeatures(int scaleIndex, int beginFrame, int endFrame);
    void extractIntensityFeature(Feature& features, int scaleIndex, int beginFrame, int endFrame);
    void extractXDerivativeFeature(Feature& features, int scaleIndex, int beginFrame, int endFrame);
    void extractYDerivativeFeature(Feature& features, int scaleIndex, int beginFrame, int endFrame);
    void extractTDerivativeFeature(Feature& features, int scaleIndex, int beginFrame, int endFrame);
    void extractFlowFeature(Feature& xFeatures, Feature& yFeatures, int scaleIndex, int beginFrame,
                            int endFrame);
    void extractHOGFeature(std::vector<Feature>& features, int scaleIndex, int beginFrame,
                           int endFrame);

    Feature extractIntensityFeature(const cv::Mat1b& frame) const;
    Feature extractXDerivativeFeature(const cv::Mat1b& frame) const;
    Feature extractYDerivativeFeature(const cv::Mat1b& frame) const;
    Feature extractTDerivativeFeature(const cv::Mat1b& prev, const cv::Mat1b& next) const;
    std::vector<Feature> extractFlowFeature(const cv::Mat1b& prev, const cv::Mat1b& next) const;
    std::vector<Feature> extractHOGFeature(const cv::Mat1b& frame) const;

    Descriptor getDescriptor(int scaleIndex, const cv::Vec3i& topLeftPoint, int width,
                             int height) const;
    Descriptor getHOGDescriptor(int scaleIndex, const cv::Vec3i& topLeftPoint, int width,
                                int height) const;
    int calculateFeatureIndex(int x, int y, int t, int width, int height) const;

    Descriptor calculateHistogram(const std::vector<Descriptor>& binValues) const;
    Descriptor calculateBlockHistogram(const std::vector<Descriptor>& binValues, int beginX,
                                       int beginY, int beginT) const;
    Descriptor pooling(const Descriptor& descriptor, PoolingType type) const;
    float averagePooling(const Descriptor& descriptor, int beginX, int beginY, int beginT) const;
    float maxPooling(const Descriptor& descriptor, int beginX, int beginY, int beginT) const;
};
}
}

#endif